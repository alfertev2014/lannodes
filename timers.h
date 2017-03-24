#ifndef TIMERS_H
#define TIMERS_H

#include <time.h>

typedef void (*TimerHandler)(void*);

typedef union {
    int intValue;
    void *ptrValue;
} TimerHandlerArgument;

struct Timer
{
    int timerIndex;

    int init(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument);
    int deinit();
    int start();
    int stop();
};

#endif // TIMERS_H
