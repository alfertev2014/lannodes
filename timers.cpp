#include "timers.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// timers
#include <signal.h>
#include <time.h>

#define MAX_TIMERS_COUNT 10

struct TimerDescriptor
{
    bool created;
    int nextIndex;

    bool isArmed;
    int timeout;
    bool isInterval;

    timer_t timerId;
    TimerHandler handler;
    TimerHandlerArgument handlerArgument;
};

struct TimerSystem
{
    struct TimerDescriptor timers[MAX_TIMERS_COUNT];
    int nextTimerIndex;

    int init();
    int getNextTimerIndex();
    int createTimer(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument);
    int startTimerByIndex(int index);
    int stopTimerByIndex(int index);
    int deleteTimerByIndex(int index);
} timerSystem;

int registerSignalHandler();

int TimerSystem::init()
{
    if (registerSignalHandler() == -1) {
        // TODO: Log error
        return -1;
    }

    for (int i = 0; i < MAX_TIMERS_COUNT; ++i) {
        this->timers[i].handler = NULL;
        this->timers[i].handlerArgument = NULL;
        this->timers[i].created = false;
        this->timers[i].nextIndex = i + 1;
    }

    this->timers[MAX_TIMERS_COUNT - 1].nextIndex = -1;

    this->nextTimerIndex = 0;

    return 0;
}

int TimerSystem::getNextTimerIndex()
{
    int currentIndex = this->nextTimerIndex;

    if (currentIndex == -1) {
        // TODO: Log error
        return -1;
    }

    struct TimerDescriptor *current = &this->timers[currentIndex];

    struct sigevent sigev;
    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo = SIGUSR1;
    sigev.sigev_value.sival_int = currentIndex;
    if (timer_create(CLOCK_REALTIME, &sigev, &current->timerId) == -1) {
        // TODO: Log error
        return -1;
    }

    this->nextTimerIndex = current->nextIndex;
    current->created = true;

    return currentIndex;
}

int TimerSystem::createTimer(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument)
{
    int index = this->getNextTimerIndex();
    if (index == -1) {
        // TODO: Log error
        return -1;
    }

    struct TimerDescriptor *timer = this->timers[index];

    timer->timeout = interval;
    timer->isInterval = repeat;
    timer->isArmed = false;

    timer->handler = handler;
    timer->handlerArgument = argument;

    return index;
}

int TimerSystem::startTimerByIndex(int index)
{
    if (index < 0 || index >= MAX_TIMERS_COUNT) {
        // TODO: Log error
        return -1;
    }

    struct TimerDescriptor *timer = &this->timers[index];

    int seconds = timer->timeout / 1000;
    int nanoseconds = (timer->timeout % 1000) * 1000;

    /* Start the timer */
    struct itimerspec its;
    its.it_value.tv_sec = seconds;
    its.it_value.tv_nsec = nanoseconds;

    if (isInterval) {
        its.it_interval.tv_sec = seconds;
        its.it_interval.tv_nsec = nanoseconds;
    }
    else {
        its.it_interval.tv_sec = 0;
        its.it_interval.tv_nsec = 0;
    }

    if (timer_settime(timer->timerId, 0, &its, NULL) == -1) {
        // TODO: Log error
        return -1;
    }

    timer->isArmed = true;
    return 0;
}

int TimerSystem::stopTimerByIndex(int index)
{
    if (index < 0 || index >= MAX_TIMERS_COUNT) {
        // TODO: Log error
        return -1;
    }

    struct TimerDescriptor *timer = &this->timers[index];

    /* Stop the timer */
    struct itimerspec its;
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timer->timerId, 0, &its, NULL) == -1) {
        // TODO: Log error
        return -1;
    }

    timer->isArmed = false;
    return 0;
}

int TimerSystem::deleteTimerByIndex(int index)
{
    if (index < 0 || index >= MAX_TIMERS_COUNT) {
        // TODO: Log error
        return -1;
    }

    struct TimerDescriptor *timer = &this->timers[index];

    if (!timer->created) {
        // TODO: Log error
        return -1;
    }

    timer->nextIndex = this->nextTimerIndex;
    this->nextTimerIndex = index;

    if (timer_delete(timer->timerId) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

void signalHandler(int sig, struct siginfo_t *si, void *uc)
{

}

int registerSignalHandler()
{
    sigset_t mask;

    /* Register the signal handler */
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    /* Block timer signal temporarily */

    printf("Blocking signal %d\n", SIG);
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}


int Timer::init(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument)
{
    int index = timerSystem.createTimer(interval, repeat, handler, argument);
    if (index == -1) {
        // TODO: Log error
        return -1;
    }
    this->timerIndex = index;

    return 0;
}

int Timer::deinit()
{
    return timerSystem.deleteTimerByIndex(this->timerIndex);
}

int Timer::start()
{
    return timerSystem.startTimerByIndex(this->timerIndex);
}

int Timer::stop()
{
    return timerSystem.stopTimerByIndex(this->timerIndex);
}
