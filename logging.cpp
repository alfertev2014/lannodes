#include "logging.h"

#include <time.h>
#include <stdlib.h>

void logInfo(const char *message)
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    printf("\033[1;31m%d:%d:%d\033[0m %s\n",
           ltm->tm_hour, ltm->tm_min, ltm->tm_sec,
           message);
}


void logError(const char *message)
{
    time_t now = time(0);
    tm *ltm = localtime(&now);
    fprintf(stderr, "\033[1;31m%d:%d:%d\033[0m %s\n",
            ltm->tm_hour, ltm->tm_min, ltm->tm_sec,
            message);
}

void die(const char *message)
{
    fprintf(stderr, "Die: %s", message);
    fflush(stderr);
    fflush(stdout);
    exit(EXIT_FAILURE);
    while(true);
}
