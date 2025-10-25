// mutex_demo.c
// POSIX C demo (macOS) showing:
//  - producer -> consumers via pthread_cond_broadcast + condition variable
//  - consumers -> collector via a shared array protected by a mutex
//
// Compile: cc -o mutex_demo mutex_demo.c -pthread
// Run: ./mutex_demo

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_CHUNKS 999
#define BATCH_SIZE 333
#define N_CONSUMERS 7

typedef enum { AVAIL_EMPTY = 0, AVAIL_AVAILABLE, AVAIL_TAKEN } avail_t;
typedef enum { COL_NONE = 0, COL_NEW, COL_DONE } col_t;

typedef struct {
    int id;
    uint64_t checksum;    // consumer writes, collector doubles
    int who;			  // which thread wrote the checksum
    avail_t avail;        // protected by prod_cons_mutex
    col_t col_status;     // protected by collector_mutex
} entry_t;

static entry_t entries[TOTAL_CHUNKS];

/* Producer-consumer synchronization */
static pthread_mutex_t prod_cons_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t prod_cons_cond   = PTHREAD_COND_INITIALIZER;
static int batch_remaining = 0;     // how many items in current batch remain unclaimed
static bool terminate_consumers = false;
static bool all_produced = false;

/* Collector synchronization */
static pthread_mutex_t collector_mutex = PTHREAD_MUTEX_INITIALIZER;

static int claim_available(void) {
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (entries[i].avail == AVAIL_AVAILABLE) {
            entries[i].avail = AVAIL_TAKEN;
            return i;
        }
    }
    return -1;
}

/* Helper: check if any AVAILABLE entry exists (caller must hold prod_cons_mutex) */
static bool any_available_locked(void) {
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (entries[i].avail == AVAIL_AVAILABLE) return true;
    }
    return false;
}

/* unchunked Producer thread */
void *u_producer_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&prod_cons_mutex);
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        entries[i].id = i;
        entries[i].checksum = 0;
        entries[i].avail = AVAIL_AVAILABLE;
    }
    all_produced = true;
    pthread_cond_broadcast(&prod_cons_cond); // wake consumers
    pthread_mutex_unlock(&prod_cons_mutex);

    printf("Producer: all chunks posted.\n");
    return NULL;
}

/* Consumers */
/* unchunked Consumer */
void *u_consumer_thread(void *arg) {
    int instance_id = (int)(intptr_t)arg;
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self() ^ instance_id;

    while (1) {
        pthread_mutex_lock(&prod_cons_mutex);
        while (!any_available_locked() && !all_produced) {
            pthread_cond_wait(&prod_cons_cond, &prod_cons_mutex);
        }

        int picked_id = claim_available();
        pthread_mutex_unlock(&prod_cons_mutex);

        if (picked_id == -1) {
            // No more work
            break;
        }

        // Skewed work based on thread id
        int X = (rand_r(&seed) % ((500000 * (instance_id+1)) - 1000 + 1)) + 1000;
//        int X = (rand_r(&seed) % (500000 - 1000 + 1)) + 1000;
        uint64_t checksum = 0;
        for (int k = 1; k <= X; ++k) checksum += (uint64_t)k;

        pthread_mutex_lock(&collector_mutex);
        entries[picked_id].checksum = checksum;
        entries[picked_id].col_status = COL_NEW;
        entries[picked_id].who = instance_id;
//        printf("Consumer #%d: chunk id=%d checksum=%llu\n", instance_id, picked_id, (unsigned long long)checksum);
        
        pthread_mutex_unlock(&collector_mutex);
    }

    printf("Consumer #%d: terminating.\n", instance_id);
    return NULL;
}

/* chunked Producer thread */
void *c_producer_thread(void *arg) {
    (void)arg;
    for (int batch_start = 0; batch_start < TOTAL_CHUNKS; batch_start += BATCH_SIZE) {
        pthread_mutex_lock(&prod_cons_mutex);

        // publish the batch
        for (int i = batch_start; i < batch_start + BATCH_SIZE; ++i) {
            entries[i].id = i;
            entries[i].checksum = 0;
            entries[i].avail = AVAIL_AVAILABLE;
            // leave col_status alone (collector will observe when consumer sets NEW)
        }
        batch_remaining = BATCH_SIZE;
        printf("Producer: posted batch %d..%d\n", batch_start, batch_start + BATCH_SIZE - 1);

        // wake consumers that a batch is available
        pthread_cond_broadcast(&prod_cons_cond);

        // wait until batch_remaining becomes zero (i.e., all claimed)
        while (batch_remaining > 0) {
            pthread_cond_wait(&prod_cons_cond, &prod_cons_mutex);
        }

        pthread_mutex_unlock(&prod_cons_mutex);
    }

    // all batches produced and consumed by consumers (they claimed them)
    pthread_mutex_lock(&prod_cons_mutex);
    all_produced = true;
    terminate_consumers = true;
    pthread_cond_broadcast(&prod_cons_cond); // wake any waiting consumers so they can exit
    pthread_mutex_unlock(&prod_cons_mutex);

    printf("Producer: all batches posted and consumed; terminating.\n");
    return NULL;
}

// chunked consumer
void *c_consumer_thread(void *arg) {
    int instance_id = (int)(intptr_t)arg;

    // seed RNG independently
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self() ^ (unsigned int)instance_id;

    while (1) {
        int picked_id = -1;

        // Claim an AVAILABLE chunk (using prod_cons_mutex)
        pthread_mutex_lock(&prod_cons_mutex);

        // Wait until an AVAILABLE entry is present or termination requested
        while (!any_available_locked() && !terminate_consumers) {
            pthread_cond_wait(&prod_cons_cond, &prod_cons_mutex);
        }

        // If termination and no available, exit loop
        if (!any_available_locked() && terminate_consumers) {
            pthread_mutex_unlock(&prod_cons_mutex);
            break;
        }

        // pick the first AVAILABLE entry
        for (int i = 0; i < TOTAL_CHUNKS; ++i) {
            if (entries[i].avail == AVAIL_AVAILABLE) {
                entries[i].avail = AVAIL_TAKEN;
                picked_id = entries[i].id;
                batch_remaining--;
                // If batch_remaining reached 0, notify producer
                if (batch_remaining == 0) {
                    pthread_cond_broadcast(&prod_cons_cond);
                }
                break;
            }
        }

        pthread_mutex_unlock(&prod_cons_mutex);

        if (picked_id == -1) {
            // Spurious wake or termination; loop again
            continue;
        }

        // Simulate work: choose X in [1000, 500000], loop from 1..X summing. Bias this by thread ID to show differnces
        int X = (rand_r(&seed) % (500000 - 1000 + 1)) + 1000;
        uint64_t checksum;
        for (int repeat = 0; repeat <= instance_id; repeat++){
			checksum = 0;
        	for (int k = 1; k <= X; ++k) {
        		checksum += (uint64_t)k;
        	}
        }

        // Update shared array for collector under collector_mutex
        pthread_mutex_lock(&collector_mutex);
        entries[picked_id].checksum = checksum;
        entries[picked_id].col_status = COL_NEW;
        entries[picked_id].who = instance_id;
//        printf("Consumer #%d: chunk id=%d checksum=%llu\n", instance_id, picked_id, (unsigned long long)checksum);
        
        pthread_mutex_unlock(&collector_mutex);

        // loop and pick up more work
    }

    printf("Consumer #%d: terminating.\n", instance_id);
    return NULL;
}


/* Collector thread with per-consumer totals and counts */
void *collector_thread(void *arg) {
    (void)arg;

    uint64_t consumer_totals[N_CONSUMERS] = {0};  // sum of checksums per consumer
    int consumer_counts[N_CONSUMERS] = {0};       // number of chunks processed per consumer

    while (1) {
        

        pthread_mutex_lock(&collector_mutex);
        int done_count = 0;
        for (int i = 0; i < TOTAL_CHUNKS; ++i) {
            if (entries[i].col_status == COL_NEW) {
                // double the checksum and mark done
                entries[i].checksum *= 2ULL;
                entries[i].col_status = COL_DONE;

                // accumulate total per consumer
                if (entries[i].who >= 0 && entries[i].who < N_CONSUMERS) {
                    consumer_totals[entries[i].who] += entries[i].checksum;
                    consumer_counts[entries[i].who]++;  // increment count
                }
            }
            if (entries[i].col_status == COL_DONE) ++done_count;
        }

        // If all done, print final array and totals
        if (done_count == TOTAL_CHUNKS) {
            printf("\nCollector: all %d entries DONE. Final results:\n", TOTAL_CHUNKS);
            for (int i = 0; i < TOTAL_CHUNKS; ++i) {
            }

            printf("\nTotal work per consumer:\n");
            for (int c = 0; c < N_CONSUMERS; ++c) {
                printf("Consumer #%d: chunks=%3d, total checksum=%20llu\n", 
                        c, consumer_counts[c], (unsigned long long)consumer_totals[c]);
            }

            pthread_mutex_unlock(&collector_mutex);
            break;
        }

        pthread_mutex_unlock(&collector_mutex);
    }

    printf("Collector: terminating.\n");
    return NULL;
}


int main(void) {
    pthread_t prod_thread;
    pthread_t coll_thread;
    pthread_t cons_threads[N_CONSUMERS];

    // initialize entries
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        entries[i].id = i;
        entries[i].checksum = 0;
        entries[i].avail = AVAIL_EMPTY;
        entries[i].col_status = COL_NONE;
    }

    // Start collector first (per spec)
    if (pthread_create(&coll_thread, NULL, collector_thread, NULL) != 0) {
        perror("pthread_create collector");
        exit(1);
    }

    // Start consumers
    for (int i = 0; i < N_CONSUMERS; ++i) {
        if (pthread_create(&cons_threads[i], NULL, c_consumer_thread, (void *)(intptr_t)i) != 0) {
            perror("pthread_create consumer");
            exit(1);
        }
    }

    // Start producer
    if (pthread_create(&prod_thread, NULL, c_producer_thread, NULL) != 0) {
        perror("pthread_create producer");
        exit(1);
    }

    // join producer and consumers and collector
    pthread_join(prod_thread, NULL);
    for (int i = 0; i < N_CONSUMERS; ++i) pthread_join(cons_threads[i], NULL);
    pthread_join(coll_thread, NULL);

    printf("Main: all threads joined, exiting.\n");
    return 0;
}
