#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <mach/mach.h>
#include <mach/semaphore.h>
#include "macos_timed_mutex.h"

// Convert absolute timespec (monotonic) to relative mach_timespec_t
static int abs_timespec_to_rel_mach(const struct timespec *absmon, mach_timespec_t *out) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
        return -1;

    if (absmon->tv_sec < now.tv_sec ||
        (absmon->tv_sec == now.tv_sec && absmon->tv_nsec <= now.tv_nsec))
        return -1; // already expired

    time_t sec = absmon->tv_sec - now.tv_sec;
    long nsec = absmon->tv_nsec - now.tv_nsec;
    if (nsec < 0) {
        sec -= 1;
        nsec += 1000000000L;
    }
    if (sec < 0) return -1;

    out->tv_sec = (unsigned int)sec;
    out->tv_nsec = (clock_res_t)nsec;
    return 0;
}

int timed_mutex_init(timed_mutex_t *tm) {
    int r = pthread_mutex_init(&tm->mutex, NULL);
    if (r != 0) return r;
    kern_return_t kr = semaphore_create(mach_task_self(), &tm->wait_sem, SYNC_POLICY_FIFO, 0);
    if (kr != KERN_SUCCESS) {
        pthread_mutex_destroy(&tm->mutex);
        return EINVAL;
    }
    return 0;
}

int timed_mutex_destroy(timed_mutex_t *tm) {
    kern_return_t kr = semaphore_destroy(mach_task_self(), tm->wait_sem);
    int r = pthread_mutex_destroy(&tm->mutex);
    if (kr != KERN_SUCCESS) return EINVAL;
    return r;
}

int timed_mutex_trylock(timed_mutex_t *tm) {
    return pthread_mutex_trylock(&tm->mutex);
}

int timed_mutex_lock(timed_mutex_t *tm) {
    return pthread_mutex_lock(&tm->mutex);
}

int timed_mutex_unlock(timed_mutex_t *tm) {
    int r = pthread_mutex_unlock(&tm->mutex);
    if (r != 0) return r;
    // Wake a waiting thread, if any
    kern_return_t kr = semaphore_signal(tm->wait_sem);
    return (kr == KERN_SUCCESS) ? 0 : EINVAL;
}

// Emulated timed lock (returns 0, ETIMEDOUT, or another errno)
int timed_mutex_timedlock(timed_mutex_t *tm, const struct timespec *abs_timeout_mon) {
    int ret = pthread_mutex_trylock(&tm->mutex);
    if (ret == 0) return 0;
    if (ret != EBUSY) return ret;

    mach_timespec_t rel;
    if (abs_timespec_to_rel_mach(abs_timeout_mon, &rel) != 0)
        return ETIMEDOUT;

    kern_return_t kr = semaphore_timedwait(tm->wait_sem, rel);
    if (kr == KERN_OPERATION_TIMED_OUT)
        return ETIMEDOUT;
    if (kr != KERN_SUCCESS)
        return EINVAL;

    // Woken — try to take the mutex
    for (int i = 0; i < 100; ++i) {
        ret = pthread_mutex_trylock(&tm->mutex);
        if (ret == 0) return 0;
        if (ret != EBUSY) return ret;
        struct timespec s = {0, 2000}; // 2 µs yield
        nanosleep(&s, NULL);
    }
    // Final fallback (rare)
    return pthread_mutex_lock(&tm->mutex);
}
