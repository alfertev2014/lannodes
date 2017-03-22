#ifndef TIMERS_H
#define TIMERS_H


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// timers
#include <signal.h>
#include <time.h>

struct Timer
{
    timer_t timerId;
    bool isArmed;
    int timeout_sec;

    int init(int timeout)
    {
        this->timeout_sec = timeout;
        isArmed = false;

        struct sigevent sigev;
        sigev.sigev_notify = SIGEV_THREAD_ID;
        sigev.sigev_signo = SIGUSR1;
        sigev.sigev_value.sival_ptr
        timer_create(CLOCK_MONOTONIC, &sigev)
    }
};



#endif // TIMERS_H
