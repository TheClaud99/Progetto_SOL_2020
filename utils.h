#if !defined(UTILS_H)
#define UTILS_H

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#define ec_meno1(c, s)      \
    if ((c) == -1)          \
    {                       \
        perror(s);          \
        exit(EXIT_FAILURE); \
    }
#define ec_null(c, s)       \
    if ((c) == NULL)        \
    {                       \
        perror(s);          \
        exit(EXIT_FAILURE); \
    }
#define PERROR(s)           \
    {                       \
        perror(s);          \
        exit(EXIT_FAILURE); \
    }

void Pthread_mutex_lock(pthread_mutex_t *mtx)
{
    int err;
    if ((err = pthread_mutex_lock(mtx)) != 0)
    {
        errno = err;
        perror("lock");
        pthread_exit((void *)&errno);
    }
}

void Pthread_mutex_unlock(pthread_mutex_t *mtx)
{
    int err;
    if ((err = pthread_mutex_unlock(mtx)) != 0)
    {
        errno = err;
        perror("unlock");
        pthread_exit((void *)&errno);
    }
}

void Pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mtx)
{
    int err;
    if ((err = pthread_cond_wait(cond, mtx)) != 0)
    {
        errno = err;
        perror("cond wait");
        pthread_exit((void *)&errno);
    }
}

void Pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
    int err;
    if ((err = pthread_cond_timedwait(cond, mutex, abstime)) != 0 && err != ETIMEDOUT)
    {
        errno = err;
        perror("cond wait");
        pthread_exit((void *)&errno);
    }
}

void Pthread_cond_signal(pthread_cond_t *cond)
{
    int err;
    if ((err = pthread_cond_signal(cond)) != 0)
    {
        errno = err;
        perror("cond signal");
        pthread_exit((void *)&errno);
    }
}

void summstotimespec(struct timespec *ts, long msec)
{
    int nano_sec = (ts->tv_nsec + (msec % 1000) * 1000000);
    ts->tv_sec +=  msec / 1000 + nano_sec / 1000000000;
    ts->tv_nsec = nano_sec % 1000000000;
}

/* msleep(): Sleep for the requested number of milliseconds. */
int msleep(long msec)
{
    struct timespec ts;
    int res;

    if (msec < 0)
    {
        errno = EINVAL;
        return -1;
    }

    ts.tv_sec = msec / 1000;
    ts.tv_nsec = (msec % 1000) * 1000000;

    do
    {
        res = nanosleep(&ts, &ts);
    } while (res && errno == EINTR);

    return res;
}

#endif /* UTILS_H */