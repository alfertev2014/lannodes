#ifndef TIMERS_H
#define TIMERS_H


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// timers
#include <signal.h>
#include <time.h>

void handler(int sig, struct siginfo_t *si, void *uc)
{

}

struct Timer
{
    timer_t timerId;
    bool isArmed;
    int timeout_sec;

    int init(int timeout)
    {
        this->timeout_sec = timeout;
        this->isArmed = false;


        long long freq_nanosecs;
        sigset_t mask;

        /* Register the signal handler */
        struct sigaction sa;
        sa.sa_flags = SA_SIGINFO;
        sa.sa_sigaction = handler;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGUSR1, &sa, NULL);

        /* Block timer signal temporarily */

        printf("Blocking signal %d\n", SIG);
        sigemptyset(&mask);
        sigaddset(&mask, SIGUSR1);
        if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
        {
            // TODO: Log error
            return -1;
        }

        struct sigevent sigev;
        sigev.sigev_notify = SIGEV_THREAD_ID;
        sigev.sigev_signo = SIGUSR1;
        sigev.sigev_value.sival_ptr = (void*)&this->timerId;
        if (timer_create(CLOCK_MONOTONIC, &sigev, &this->timerId) == -1)
        {
            // TODO: Log error
            return -1;
        }
    }

    void start()
    {
        struct itimerspec its;

        /* Start the timer */
        its.it_value.tv_sec = 10;
        its.it_value.tv_nsec = 0;
        its.it_interval.tv_sec = its.it_value.tv_sec;
        its.it_interval.tv_nsec = its.it_value.tv_nsec;

        timer_settime(timerid, 0, &its, NULL);
    }
};



#endif // TIMERS_H
