#include "timers.h"

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

// timers
#include <time.h>
#include <signal.h>

#define MAX_TIMERS_COUNT 10
#define MAX_TIMEOUTS_COUNT 20

struct TimerDescriptor
{
    bool created;
    int nextFreeIndex;

    bool isArmed;
    int timeout;
    bool isInterval;

    timer_t timerId;
    TimerHandler handler;
    TimerHandlerArgument handlerArgument;
};

static void signalHandler(int sig, siginfo_t *si, void *uc);

struct TimerSystem
{
private:
    struct TimerDescriptor timers[MAX_TIMERS_COUNT];
    int firstFreeIndex;

    volatile int timeoutQueue[MAX_TIMEOUTS_COUNT];
    volatile int timeoutHead;
    volatile int timeoutTail;

public:
    int init();
    int createTimer(int interval, bool repeat, TimerHandler handler, TimerHandlerArgument argument);
    int startTimerByIndex(int index);
    int stopTimerByIndex(int index);
    int deleteTimerByIndex(int index);

    int runAllTimeouts();
private:
    int getNextTimerIndex();
    int raiseTimerByIndex(int index);

    static int registerSignalHandler();

    friend void signalHandler(int sig, siginfo_t *si, void *uc);
} timerSystem;

int TimerSystem::init()
{
    for (int i = 0; i < MAX_TIMERS_COUNT; ++i) {
        TimerDescriptor *timer = &this->timers[i];
        timer->handler = NULL;
        timer->handlerArgument.ptrValue = NULL;
        timer->created = false;
        timer->nextFreeIndex = i + 1;
    }

    this->timers[MAX_TIMERS_COUNT - 1].nextFreeIndex = -1;
    this->firstFreeIndex = 0;

    this->timeoutHead = -1;
    this->timeoutTail = -1;

    if (TimerSystem::registerSignalHandler() == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

int TimerSystem::getNextTimerIndex()
{
    int currentIndex = this->firstFreeIndex;

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

    this->firstFreeIndex = current->nextFreeIndex;
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

    struct TimerDescriptor *timer = &this->timers[index];

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

    if (timer->isInterval) {
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

    timer->nextFreeIndex = this->firstFreeIndex;
    this->firstFreeIndex = index;

    if (timer_delete(timer->timerId) == -1) {
        // TODO: Log error
        return -1;
    }

    return 0;
}

// in signal handler !!!!!!!
int TimerSystem::raiseTimerByIndex(int index)
{
    if (index < 0 || index > MAX_TIMERS_COUNT) {
        // TODO: Log error
        return -1;
    }

    if (this->timeoutHead == this->timeoutTail && this->timeoutTail != -1) {
        // TODO: Log error, queue is full
        return -1;
    }

    if (this->timeoutTail != -1) {
        this->timeoutQueue[this->timeoutTail] = index;
        ++this->timeoutTail;
        if (this->timeoutTail == MAX_TIMEOUTS_COUNT)
            this->timeoutTail = 0;
        this->timeoutQueue[this->timeoutTail] = index;
    }
    else {
        this->timeoutQueue[0] = index;
        this->timeoutTail = 0;
        this->timeoutHead = 0;
    }
}

int TimerSystem::runAllTimeouts()
{
    sigset_t mask;
    sigset_t orig_mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);

    if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
        // TODO: Log error
        return -1;
    }

    if (this->timeoutHead == -1)
        return 0;

    while(this->timeoutHead != this->timeoutTail) {
        int raisedIndex = this->timeoutQueue[this->timeoutHead];
        ++this->timeoutHead;

        if (this->timeoutHead != this->timeoutTail) {
            this->timeoutTail = -1;
            this->timeoutHead = -1;
        }

        if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
            // TODO: Log error
            return -1;
        }

        TimerDescriptor *timer = &this->timers[raisedIndex];
        timer->handler(timer->handlerArgument);

        if (sigprocmask(SIG_BLOCK, &mask, &orig_mask) < 0) {
            // TODO: Log error
            return -1;
        }
    }

    if (sigprocmask(SIG_SETMASK, &orig_mask, NULL) < 0) {
        // TODO: Log error
        return -1;
    }
    return 0;
}

static void signalHandler(int sig, siginfo_t *si, void *uc)
{
    int index = si->si_value.sival_int;

    timerSystem.raiseTimerByIndex(index);
}

int TimerSystem::registerSignalHandler()
{
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signalHandler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

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
    if (timerSystem.stopTimerByIndex(this->timerIndex) == -1) {
        return -1;
    }
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

int Timer::runAllPendingTimouts()
{
    return timerSystem.runAllTimeouts();
}
