#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <unistd.h> // For sleep

// The mutex shared by all threads
static pthread_mutex_t shared_mutex = PTHREAD_MUTEX_INITIALIZER;

// A simple shared counter to demonstrate protected access
static int shared_resource_counter = 0;

#ifdef __APPLE__
#include <sys/time.h>

/* macOS does not expose pthread_mutex_timedlock, so we provide a fallback. */
static int macos_pthread_mutex_timedlock(pthread_mutex_t *mutex, const struct timespec *abs_timeout) {
    int rv;
    struct timespec now;
    struct timespec sleep_duration = {0, 10000000}; // Sleep for 10ms

    // Loop until lock is acquired or timeout is reached
    while ((rv = pthread_mutex_trylock(mutex)) == EBUSY) {
        // Get current time
        if (clock_gettime(CLOCK_REALTIME, &now) == -1) {
            return -1; // Error
        }

        // Check for timeout: if now >= abs_timeout, return ETIMEDOUT
        if (now.tv_sec > abs_timeout->tv_sec ||
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
            return ETIMEDOUT;
        }
        
        // Wait for a short duration
        nanosleep(&sleep_duration, NULL); 
    }
    return rv; // 0 on success, or another error from trylock
}

// Map the standard function name to the fallback
#define pthread_mutex_timedlock macos_pthread_mutex_timedlock
#endif


// --- Thread 1: Demonstrates pthread_mutex_trylock ---
void *trylock_thread(void *arg) {
    (void)arg;
    printf("Thread 1 (trylock) started.\n");

    // Wait a moment for Thread 2 to acquire the lock first
    sleep(1);

    if (pthread_mutex_trylock(&shared_mutex) == 0) {
        // Success: Mutex was acquired immediately
        printf("Thread 1 (trylock): Successfully acquired the mutex.\n");
        shared_resource_counter += 100;
        printf("Thread 1: Resource is now %d.\n", shared_resource_counter);
        pthread_mutex_unlock(&shared_mutex);
    } else {
        // Failure: Mutex was busy (locked by Thread 2)
        printf("Thread 1 (trylock): Mutex is busy (EBUSY). Will not block.\n");
        // Thread 1 can now do other work instead of waiting
    }

    printf("Thread 1 (trylock) exiting.\n");
    return NULL;
}

// --- Thread 2: Demonstrates pthread_mutex_timedlock ---
void *timedlock_thread(void *arg) {
    (void)arg;
    struct timespec ts;
    int s;

    // 1. Acquire the lock (and hold it briefly to test the other threads)
    printf("Thread 2 (timedlock): Acquiring mutex and holding for 3 seconds.\n");
    if (pthread_mutex_lock(&shared_mutex) != 0) {
        perror("Thread 2 failed to acquire initial lock");
        return NULL;
    }
    
    // Hold the lock for 3 seconds
    shared_resource_counter += 1;
    printf("Thread 2: Resource is now %d.\n", shared_resource_counter);
    sleep(3); 
    
    // Release the lock
    printf("Thread 2: Releasing initial lock.\n");
    pthread_mutex_unlock(&shared_mutex);


    // 2. Demonstrate timedlock with a short timeout
    printf("Thread 2 (timedlock): Re-trying to acquire mutex with a 1-second timeout...\n");
    
    // Get the current time
    if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
        perror("clock_gettime");
        return NULL;
    }

    // Set a timeout 1 second in the future
    ts.tv_sec += 1;

    // Attempt to acquire the lock with a timeout
    s = pthread_mutex_timedlock(&shared_mutex, &ts);

    if (s == 0) {
        // Success: Mutex was acquired within the 1 second
        printf("Thread 2 (timedlock): Acquired mutex within the timeout.\n");
        shared_resource_counter *= 2;
        printf("Thread 2: Resource is now %d.\n", shared_resource_counter);
        pthread_mutex_unlock(&shared_mutex);
    } else if (s == ETIMEDOUT) {
        // Failure: Timeout expired
        printf("Thread 2 (timedlock): Timed out waiting for mutex (ETIMEDOUT).\n");
    } else {
        // Other error
        errno = s; // Set errno for perror
        perror("Thread 2 pthread_mutex_timedlock failed");
    }

    printf("Thread 2 (timedlock) exiting.\n");
    return NULL;
}

// --- Main Function ---
int main(void) {
    pthread_t t1, t2;

    printf("Main: Starting demo...\n");

    // Start Thread 2 first so it can initially hold the lock
    if (pthread_create(&t2, NULL, timedlock_thread, NULL) != 0) {
        perror("pthread_create t2");
        return 1;
    }

    // Start Thread 1 (will attempt trylock while T2 holds the lock)
    if (pthread_create(&t1, NULL, trylock_thread, NULL) != 0) {
        perror("pthread_create t1");
        return 1;
    }

    // Wait for threads to finish
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Clean up resources (good practice for statically initialized mutexes too)
    pthread_mutex_destroy(&shared_mutex); 

    printf("Main: Demo finished. Final shared resource value: %d\n", shared_resource_counter);
    return 0;
}