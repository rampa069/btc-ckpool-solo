#ifndef MACOS_SEM_H
#define MACOS_SEM_H

#ifdef __APPLE__

#include <pthread.h>
#include <errno.h>
#include <time.h>

// Estructura para reemplazar semáforos en macOS
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
} macos_sem_t;

// Funciones de semáforos usando mutex/cond
static inline int macos_sem_init(macos_sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    
    if (pthread_mutex_init(&sem->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&sem->cond, NULL) != 0) {
        pthread_mutex_destroy(&sem->mutex);
        return -1;
    }
    sem->count = (int)value;
    return 0;
}

static inline int macos_sem_destroy(macos_sem_t *sem) {
    int ret1 = pthread_mutex_destroy(&sem->mutex);
    int ret2 = pthread_cond_destroy(&sem->cond);
    return (ret1 != 0 || ret2 != 0) ? -1 : 0;
}

static inline int macos_sem_wait(macos_sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0) {
        pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

static inline int macos_sem_trywait(macos_sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    if (sem->count == 0) {
        pthread_mutex_unlock(&sem->mutex);
        errno = EAGAIN;
        return -1;
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

static inline int macos_sem_post(macos_sem_t *sem) {
    pthread_mutex_lock(&sem->mutex);
    sem->count++;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

static inline int macos_sem_timedwait(macos_sem_t *sem, const struct timespec *abs_timeout) {
    int ret;
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0) {
        ret = pthread_cond_timedwait(&sem->cond, &sem->mutex, abs_timeout);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&sem->mutex);
            errno = ETIMEDOUT;
            return -1;
        }
        if (ret != 0) {
            pthread_mutex_unlock(&sem->mutex);
            errno = ret;
            return -1;
        }
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return 0;
}

#endif // __APPLE__

#endif // MACOS_SEM_H
