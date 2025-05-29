#ifndef PRECOMPAT_H
#define PRECOMPAT_H

#ifdef __APPLE__
// Evitar que se incluya semaphore.h del sistema
#define _SEMAPHORE_H
#define _SYS_SEMAPHORE_H_
#endif // __APPLE__

#endif // PRECOMPAT_H
