#include "syncro.h"
#include <avr/interrupt.h>

void mutex_init(mutex_t* m)
{
    int i;
    cli();
    m->value = 1;
    for(i; i < MAX_THREADS; i++)
        m->waitlist[i] = 0;
    sei();
}

void mutex_lock(mutex_t* m)
{
    cli();
    //Lock is not currently held
    if(m->value == 1)
        m->value--;
    //Lock is held
    else
    {
        m->waitlist[getCurrentThread()] = 1;
        blocked();
    }
    sei();
}

void mutex_unlock(mutex_t* m)
{
    cli();
    int i;
    if(!m->value)
    {
        m->value = 1;
        //Make sure we have no one waiting on us
        //This biases early threads
        for(i; i < MAX_THREADS; i++)
        {
            if(m->waitlist[i])
            {
                m->waitlist[i] = 0;
                setThreadState(i, THREAD_READY);
                break;
            }
        }
    }
    sei();   
}


void sem_init(semaphore_t* s, int8_t value)
{
    cli();
    int i;
    s->value = value;
    for(i; i < MAX_THREADS; i++)
        s->waitlist[i] = 0;
    sei();
}


void sem_wait(semaphore_t* s)
{
    cli();
    //Semaphore unavailable
    if(--s->value < 0)
    {
        s->waitlist[getCurrentThread()] = 1;
        setThreadState(getCurrentThread(), THREAD_WAITING);
        threadSwap(get_next_thread(), getCurrentThread());
    }
    sei();
}


void sem_signal(semaphore_t* s)
{
    cli();
    int i;
    //Make sure we have no one waiting on us
    //This biases early threads
    if(++s->value <= 0)
    {
        for(i; i < MAX_THREADS; i++)
        {
            if(s->waitlist[i])
            {
                s->waitlist[i] = 0;
                setThreadState(i, THREAD_READY);
                break;
            }
        }
    }
    sei();  
}


void sem_signal_swap(semaphore_t* s)
{
    cli();
    int i;
    //Make sure we have no one waiting on us
    //This biases early threads
    if(++s->value <= 0)
    {
        for(i; i < MAX_THREADS; i++)
        {
            if(s->waitlist[i])
            {
                s->waitlist[i] = 0;
                setThreadState(i, THREAD_RUNNING);
                setThreadState(getCurrentThread(), THREAD_READY);
                threadSwap(i, getCurrentThread());
                break;
            }
        }
    }
    sei();  
}

