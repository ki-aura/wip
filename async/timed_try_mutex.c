// timed_try_mutex_mac.c
// Minimal macOS demo showing trylock + timedlock emulation
// Compile: cc -o timed_try_mutex_mac timed_try_mutex_mac.c -pthread
// Run: ./timed_try_mutex_mac

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include "macos_timed_mutex.h"

#define N_THREADS 3

// ------------------------------------------------------------
// Demo usage
// ------------------------------------------------------------

timed_mutex_t global_lock;

void *thread_func(void *arg) {
    int id = (int)(intptr_t)arg;

    if (timed_mutex_trylock(&global_lock) == 0) {
        printf("Thread %d: got lock immediately\n", id);
        sleep(1);
        timed_mutex_unlock(&global_lock);
    } else {
        printf("Thread %d: trylock failed, attempting timed lock\n", id);
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        ts.tv_sec += 2; // wait up to 2 seconds
        int ret = timed_mutex_timedlock(&global_lock, &ts);
        if (ret == 0) {
            printf("Thread %d: acquired lock after wait\n", id);
            sleep(1);
            timed_mutex_unlock(&global_lock);
        } else if (ret == ETIMEDOUT) {
            printf("Thread %d: timed out waiting for lock\n", id);
        } else {
            printf("Thread %d: timed lock error %d\n", id, ret);
        }
    }

    return NULL;
}

int main(void) {
    pthread_t threads[N_THREADS];
    timed_mutex_init(&global_lock);

    for (int i = 0; i < N_THREADS; ++i)
        pthread_create(&threads[i], NULL, thread_func, (void *)(intptr_t)i);

    for (int i = 0; i < N_THREADS; ++i)
        pthread_join(threads[i], NULL);

    timed_mutex_destroy(&global_lock);
    printf("Main: all threads finished\n");
    return 0;
}
