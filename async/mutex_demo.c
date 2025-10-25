// mutex_demo.c
// POSIX C demo (macOS) showing:
//  - producer -> consumers via pthread_cond_broadcast + condition variable
//  - consumers -> collector via a shared array protected by a mutex
//
// Compile: cc -o mutex_demo mutex_demo.c -pthread
// Run: ./mutex_demo

/*
Easy simplifications with significant benefit
Track a shared “available count” (incremented by the producer, decremented by consumers) 
instead of calling any_available_locked() and rescanning the array; each consumer could 
then grab work with a single pass or directly index into the batch, removing two O(n) 
scans per claim.

Introduce a condition variable for the collector that consumers signal when they flip an 
entry to COL_NEW; the collector can then wait instead of spinning, drastically reducing 
CPU usage and lock hold times while simplifying the control flow inside the loop
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#define TOTAL_CHUNKS 999   // there are no boundary checks - this must be a multiple of batch_size
#define BATCH_SIZE 333
#define N_CONSUMERS 4

typedef enum { AVAIL_EMPTY = 0, AVAIL_AVAILABLE, AVAIL_TAKEN } avail_t;
typedef enum { COL_NONE = 0, COL_NEW, COL_DONE } col_t;

typedef struct {
    int id;
    uint64_t checksum;    // consumer writes, collector doubles
    int who;              // which thread wrote the checksum
    avail_t avail;        // protected by prod_cons_mutex
    col_t col_status;     // protected by collector_mutex
} entry_t;

static entry_t entries[TOTAL_CHUNKS];

/* Producer-consumer synchronization */
static pthread_mutex_t prod_cons_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t prod_cons_cond   = PTHREAD_COND_INITIALIZER;
static int batch_remaining = 0;     // how many items in current batch remain unclaimed
static bool terminate_consumers = false;

/* Collector synchronization */
static pthread_mutex_t collector_mutex = PTHREAD_MUTEX_INITIALIZER;


/* Helper: check if any AVAILABLE entry exists (caller must hold prod_cons_mutex) */
static bool any_available_locked(void) {
    for (int i = 0; i < TOTAL_CHUNKS; ++i) {
        if (entries[i].avail == AVAIL_AVAILABLE) return true;
    }
    return false;
}


/* =======================
   Chunked Producer
   ======================= */
void *c_producer_thread(void *arg) {
    (void)arg;
    for (int batch_start = 0; batch_start < TOTAL_CHUNKS; batch_start += BATCH_SIZE) {
        pthread_mutex_lock(&prod_cons_mutex);

        // publish the batch
        for (int i = batch_start; i < batch_start + BATCH_SIZE; ++i) {
            entries[i].id = i;
            entries[i].checksum = 0;
            entries[i].avail = AVAIL_AVAILABLE; // mark as ready to be claimed
            // col_status left alone; collector will track when consumer sets COL_NEW
        }
        batch_remaining = BATCH_SIZE;
        printf("Producer: posted batch %d..%d\n", batch_start, batch_start + BATCH_SIZE - 1);

        // wake all consumers that a batch is available
        pthread_cond_broadcast(&prod_cons_cond);

        // wait until all chunks in this batch are claimed
        while (batch_remaining > 0) {
            // releases mutex while waiting and re-acquires upon wake
            pthread_cond_wait(&prod_cons_cond, &prod_cons_mutex);
        }

        pthread_mutex_unlock(&prod_cons_mutex);
    }

    // all batches produced, tell consumers to terminate
    pthread_mutex_lock(&prod_cons_mutex);
    terminate_consumers = true;
    pthread_cond_broadcast(&prod_cons_cond); // wake any waiting consumers
    pthread_mutex_unlock(&prod_cons_mutex);

    printf("Producer: all batches posted and consumed; terminating.\n");
    return NULL;
}


/* =======================
   Chunked Consumer
   ======================= */
void *c_consumer_thread(void *arg) {
    int instance_id = (int)(intptr_t)arg;

    // independent RNG seed per thread to avoid collisions
    unsigned int seed = (unsigned int)time(NULL) ^ (unsigned int)(uintptr_t)pthread_self() ^ (unsigned int)instance_id;

    while (1) {
        int picked_id = -1;

        // --------------------------
        // Claim an AVAILABLE chunk
        // --------------------------
        pthread_mutex_lock(&prod_cons_mutex);

        // Wait until a chunk is available or termination is requested
        while (!any_available_locked() && !terminate_consumers) {
            // releases mutex and waits for producer broadcast
            pthread_cond_wait(&prod_cons_cond, &prod_cons_mutex);
        }

        // Exit if termination requested and no chunks available
        if (!any_available_locked() && terminate_consumers) {
            pthread_mutex_unlock(&prod_cons_mutex);
            break;
        }

        // pick the first AVAILABLE entry (under mutex protection)
        for (int i = 0; i < TOTAL_CHUNKS; ++i) {
            if (entries[i].avail == AVAIL_AVAILABLE) {
                entries[i].avail = AVAIL_TAKEN; // mark as claimed
                picked_id = entries[i].id;
                batch_remaining--; // decrement remaining count in batch

                // notify producer if batch is fully claimed
                if (batch_remaining == 0) {
                    // pthread_cond_broadcast(&prod_cons_cond);
					// in this case a signal rather than broadcast does work, but 
					// broadcast is safer as there could have been multiple producers
					pthread_cond_signal(&prod_cons_cond);
                }
                break;
            }
        }

        pthread_mutex_unlock(&prod_cons_mutex); // release lock so other consumers can claim

        if (picked_id == -1) {
            // spurious wake or no chunk found; loop again
            continue;
        }

        // --------------------------
        // Simulate work
        // --------------------------
        // artificial work skewed by thread ID
        int X = (rand_r(&seed) % (500000 - 1000 + 1)) + 1000;
        uint64_t checksum;
        for (int repeat = 0; repeat <= instance_id; repeat++) { // ARTIFICIALLY CREATE MORE WORK BASED ON THREAD ID
            checksum = 0; // reset per repetition to avoid accumulation
            for (int k = 1; k <= X; ++k) {
                checksum += (uint64_t)k;
            }
        }

        // --------------------------
        // Update collector
        // --------------------------
        pthread_mutex_lock(&collector_mutex);
        entries[picked_id].checksum = checksum;  // write under mutex
        entries[picked_id].col_status = COL_NEW; // mark as ready for collector
        entries[picked_id].who = instance_id;
//        printf("Consumer #%d: chunk id=%d checksum=%llu\n", instance_id, picked_id, (unsigned long long)checksum);        
        pthread_mutex_unlock(&collector_mutex);
    }

    printf("Consumer #%d: terminating.\n", instance_id);
    return NULL;
}


/* =======================
   Collector
   ======================= */
void *collector_thread(void *arg) {
    (void)arg;

    uint64_t consumer_totals[N_CONSUMERS] = {0};  // sum of checksums per consumer
    int consumer_counts[N_CONSUMERS] = {0};       // number of chunks processed per consumer

    while (1) {
    	usleep(100);
        pthread_mutex_lock(&collector_mutex);
        int done_count = 0;

        for (int i = 0; i < TOTAL_CHUNKS; ++i) {
            if (entries[i].col_status == COL_NEW) {
                // double the checksum (just for demo work)
                entries[i].checksum *= 2ULL;
                entries[i].col_status = COL_DONE;

                // accumulate totals
                if (entries[i].who >= 0 && entries[i].who < N_CONSUMERS) {
                    consumer_totals[entries[i].who] += entries[i].checksum;
                    consumer_counts[entries[i].who]++;
                }
            }

            if (entries[i].col_status == COL_DONE) done_count++;
        }

        // terminate once all chunks done
        if (done_count == TOTAL_CHUNKS) {
            printf("\nCollector: all %d entries DONE. Final results:\n", TOTAL_CHUNKS);

            printf("\nTotal work per consumer:\n");
            for (int c = 0; c < N_CONSUMERS; ++c) {
                printf("Consumer #%d: chunks=%3d total checksum=%llu  avg checksum=%llu\n", 
                        c, consumer_counts[c], (unsigned long long)consumer_totals[c],
                        consumer_counts[c]>0 ? (unsigned long long)(consumer_totals[c] / consumer_counts[c]) : 0);
            }

            pthread_mutex_unlock(&collector_mutex);
            break;
        }

        pthread_mutex_unlock(&collector_mutex);
    }

    printf("Collector: terminating.\n");
    return NULL;
}


/* =======================
   Main
   ======================= */
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

    // Start collector first (to ensure consumer updates have somewhere to go)
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

    // join all threads
    pthread_join(prod_thread, NULL);
    for (int i = 0; i < N_CONSUMERS; ++i) pthread_join(cons_threads[i], NULL);
    pthread_join(coll_thread, NULL);

    printf("Main: all threads joined, exiting.\n");
    return 0;
}
