#ifndef TIMERS_H
#define TIMERS_H

#include <time.h>

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
};

int runAllPendingTimouts();

#endif // TIMERS_H
