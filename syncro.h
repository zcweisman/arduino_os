#ifndef SYNCRO_H
#define SYNCRO_H
#include "os.h"

typedef struct mutex_t {
   int value;
   int waitlist[MAX_THREADS];
}mutex_t;

typedef struct semaphore_t {
    int value;
    int waitlist[MAX_THREADS];
}semaphore_t;

void mutex_init(mutex_t* m);
void mutex_lock(mutex_t* m);
void mutex_unlock(mutex_t* m);

void sem_init(semaphore_t* s, int8_t value);
void sem_wait(semaphore_t* s);
void sem_signal(semaphore_t* s);
void sem_signal_swap(semaphore_t* s);

#endif