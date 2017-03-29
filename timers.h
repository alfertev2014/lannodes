#ifndef TIMERS_H
#define TIMERS_H

#include <sys/signal.h>

typedef union {
    int intValue;
    void *ptrValue;
} TimerHandlerArgument;

typedef void (*TimerHandler)(TimerHandlerArgument);

struct Timer
{
    int timerIndex;

    int init(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument);
    int deinit();
    int start();
    int stop();

    static int initTimerSystem();
    static int runAllPendingTimouts();

    static int lockTimers(sigset_t *old_mask);
    static int unlockTimers(const sigset_t *old_mask);
};

#endif // TIMERS_H
