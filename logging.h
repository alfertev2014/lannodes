#ifndef LOGGING_H
#define LOGGING_H

#include <string.h>
#include <stdio.h>

#define logPosition(...) fprintf(stderr, "error at file \'%s\':%d, function: %s\n", __FILE__, __LINE__, __func__)

void logInfo(const char *message);

void logError(const char *message);

void die(const char *message);

#endif // LOGGING_H
