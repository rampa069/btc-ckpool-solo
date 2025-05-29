#ifndef COMPAT_H
#define COMPAT_H

#ifdef __APPLE__
#include <pthread.h>
#include <libgen.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <alloca.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <netinet/tcp.h>
#include <time.h>

// Incluir semáforos para macOS ANTES que cualquier otro header
#include "macos_sem.h"

// Constantes prctl
#define PR_SET_NAME 15
#define PR_GET_NAME 16

// Socket options
#ifndef SO_SNDBUFFORCE
#define SO_SNDBUFFORCE SO_SNDBUF
#endif
#ifndef SO_RCVBUFFORCE
#define SO_RCVBUFFORCE SO_RCVBUF
#endif

// Epoll constants
#define EPOLLIN     0x001
#define EPOLLOUT    0x004
#define EPOLLERR    0x008
#define EPOLLHUP    0x010
#define EPOLLRDHUP  0x2000
#define EPOLLONESHOT 0x40000000
#define EPOLLET     0x80000000

#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
#define EPOLL_CLOEXEC O_CLOEXEC

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};

// strdupa implementation - crea copia en stack
#define strdupa(s) strcpy(alloca(strlen(s) + 1), s)

// Constantes faltantes en macOS
#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#ifndef TCP_KEEPIDLE
#define TCP_KEEPIDLE TCP_KEEPALIVE
#endif

#ifndef POLLRDHUP
#define POLLRDHUP 0x2000
#endif

#ifndef TIMER_ABSTIME
#define TIMER_ABSTIME 1
#endif

// Implementaciones de funciones faltantes
static inline int pthread_mutex_timedlock(pthread_mutex_t *mutex, 
                                         const struct timespec *abs_timeout) {
    // Implementación simple usando trylock + nanosleep
    struct timespec now, remaining;
    clock_gettime(CLOCK_REALTIME, &now);
    
    // Si ya pasó el timeout
    if (now.tv_sec > abs_timeout->tv_sec || 
        (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
        return ETIMEDOUT;
    }
    
    // Intentar lock inmediato
    int ret = pthread_mutex_trylock(mutex);
    if (ret == 0) return 0;
    if (ret != EBUSY) return ret;
    
    // Calcular tiempo restante
    remaining.tv_sec = abs_timeout->tv_sec - now.tv_sec;
    remaining.tv_nsec = abs_timeout->tv_nsec - now.tv_nsec;
    if (remaining.tv_nsec < 0) {
        remaining.tv_sec--;
        remaining.tv_nsec += 1000000000;
    }
    
    // Espera activa con pequeños sleeps (no ideal pero funciona)
    struct timespec short_sleep = {0, 1000000}; // 1ms
    while (remaining.tv_sec > 0 || remaining.tv_nsec > 0) {
        nanosleep(&short_sleep, NULL);
        ret = pthread_mutex_trylock(mutex);
        if (ret == 0) return 0;
        if (ret != EBUSY) return ret;
        
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_timeout->tv_sec || 
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
            return ETIMEDOUT;
        }
    }
    
    return ETIMEDOUT;
}

static inline int pthread_rwlock_timedwrlock(pthread_rwlock_t *lock,
                                           const struct timespec *abs_timeout) {
    // Similar implementación para write lock
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    
    if (now.tv_sec > abs_timeout->tv_sec || 
        (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
        return ETIMEDOUT;
    }
    
    int ret = pthread_rwlock_trywrlock(lock);
    if (ret == 0) return 0;
    if (ret != EBUSY) return ret;
    
    struct timespec short_sleep = {0, 1000000};
    while (1) {
        nanosleep(&short_sleep, NULL);
        ret = pthread_rwlock_trywrlock(lock);
        if (ret == 0) return 0;
        if (ret != EBUSY) return ret;
        
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_timeout->tv_sec || 
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
            return ETIMEDOUT;
        }
    }
}

static inline int pthread_rwlock_timedrdlock(pthread_rwlock_t *lock,
                                           const struct timespec *abs_timeout) {
    // Similar para read lock
    struct timespec now;
    clock_gettime(CLOCK_REALTIME, &now);
    
    if (now.tv_sec > abs_timeout->tv_sec || 
        (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
        return ETIMEDOUT;
    }
    
    int ret = pthread_rwlock_tryrdlock(lock);
    if (ret == 0) return 0;
    if (ret != EBUSY) return ret;
    
    struct timespec short_sleep = {0, 1000000};
    while (1) {
        nanosleep(&short_sleep, NULL);
        ret = pthread_rwlock_tryrdlock(lock);
        if (ret == 0) return 0;
        if (ret != EBUSY) return ret;
        
        clock_gettime(CLOCK_REALTIME, &now);
        if (now.tv_sec > abs_timeout->tv_sec || 
            (now.tv_sec == abs_timeout->tv_sec && now.tv_nsec >= abs_timeout->tv_nsec)) {
            return ETIMEDOUT;
        }
    }
}

static inline int clock_nanosleep(clockid_t clock_id, int flags,
                                 const struct timespec *request,
                                 struct timespec *remain) {
    if (flags == TIMER_ABSTIME) {
        struct timespec now;
        clock_gettime(clock_id, &now);
        
        struct timespec diff;
        diff.tv_sec = request->tv_sec - now.tv_sec;
        diff.tv_nsec = request->tv_nsec - now.tv_nsec;
        
        if (diff.tv_nsec < 0) {
            diff.tv_sec--;
            diff.tv_nsec += 1000000000;
        }
        
        if (diff.tv_sec < 0) {
            if (remain) {
                remain->tv_sec = 0;
                remain->tv_nsec = 0;
            }
            return 0;
        }
        
        return nanosleep(&diff, remain);
    } else {
        return nanosleep(request, remain);
    }
}

static inline int epoll_create(int size) {
    return kqueue();
}

static inline int epoll_create1(int flags) {
    int fd = kqueue();
    if (fd == -1) return -1;
    
    if (flags & EPOLL_CLOEXEC) {
        fcntl(fd, F_SETFD, FD_CLOEXEC);
    }
    
    return fd;
}

static inline int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event) {
    struct kevent kev[2];
    int nchanges = 0;
    
    if (op == EPOLL_CTL_DEL) {
        EV_SET(&kev[0], fd, EVFILT_READ, EV_DELETE, 0, 0, NULL);
        EV_SET(&kev[1], fd, EVFILT_WRITE, EV_DELETE, 0, 0, NULL);
        nchanges = 2;
    } else {
        int filter = 0;
        if (event->events & EPOLLIN) {
            EV_SET(&kev[nchanges++], fd, EVFILT_READ, 
                   op == EPOLL_CTL_MOD ? EV_ENABLE : EV_ADD, 0, 0, 
                   (void*)(uintptr_t)event->data.u64);
        }
        if (event->events & EPOLLOUT) {
            EV_SET(&kev[nchanges++], fd, EVFILT_WRITE, 
                   op == EPOLL_CTL_MOD ? EV_ENABLE : EV_ADD, 0, 0, 
                   (void*)(uintptr_t)event->data.u64);
        }
    }
    
    return kevent(epfd, kev, nchanges, NULL, 0, NULL);
}

static inline int epoll_wait(int epfd, struct epoll_event *events, 
                            int maxevents, int timeout) {
    struct kevent kevents[maxevents];
    struct timespec ts, *tsp = NULL;
    
    if (timeout >= 0) {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000;
        tsp = &ts;
    }
    
    int n = kevent(epfd, NULL, 0, kevents, maxevents, tsp);
    if (n == -1) return -1;
    
    for (int i = 0; i < n; i++) {
        events[i].data.u64 = (uint64_t)(uintptr_t)kevents[i].udata;
        events[i].events = 0;
        
        if (kevents[i].filter == EVFILT_READ) {
            events[i].events |= EPOLLIN;
        }
        if (kevents[i].filter == EVFILT_WRITE) {
            events[i].events |= EPOLLOUT;
        }
        if (kevents[i].flags & EV_ERROR) {
            events[i].events |= EPOLLERR;
        }
        if (kevents[i].flags & EV_EOF) {
            events[i].events |= EPOLLHUP;
        }
    }
    
    return n;
}

static inline int prctl(int option, const char* arg2, unsigned long arg3, 
                       unsigned long arg4, unsigned long arg5) {
    (void)arg3;
    (void)arg4;
    (void)arg5;
    
    if (option == PR_SET_NAME) {
        return pthread_setname_np(arg2);
    }
    if (option == PR_GET_NAME) {
        return pthread_getname_np(pthread_self(), (char*)arg2, 16);
    }
    
    errno = EINVAL;
    return -1;
}

static inline int feenableexcept(int excepts) {
    // No-op en macOS
    return 0;
}

#endif // __APPLE__

#endif // COMPAT_H
