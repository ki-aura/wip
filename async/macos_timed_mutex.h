#ifndef MACOS_TIMED_MIUTEX_H
#define MACOS_TIMED_MIUTEX_H

#ifdef __APPLE__
#include <sys/time.h>

// ------------------------------------------------------------
// Custom timed mutex implementation using Mach semaphores
// ------------------------------------------------------------
typedef struct {
    pthread_mutex_t mutex;
    semaphore_t wait_sem;
} timed_mutex_t;


/* macOS does not expose pthread_mutex_timedlock, so we provide a fallback. */
int timed_mutex_timedlock(timed_mutex_t *tm, const struct timespec *abs_timeout_mon);
int timed_mutex_init(timed_mutex_t *tm) ;
int timed_mutex_destroy(timed_mutex_t *tm) ;
int timed_mutex_trylock(timed_mutex_t *tm) ;
int timed_mutex_lock(timed_mutex_t *tm) ;
int timed_mutex_unlock(timed_mutex_t *tm) ;

// Map the standard function name to the fallback
//#define timed_mutex_timedlock macos_pthread_mutex_timedlock
#endif


#endif // MACOS_TIMED_MIUTEX_H