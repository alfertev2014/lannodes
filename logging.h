#ifndef LOGGING_H
#define LOGGING_H

#include <errno.h>
#include <string.h>
#include <stdio.h>

#define logError(...) fprintf(stderr, " in file \'%s\':%d, function: %s\n", __FILE__, __LINE__, __func__)

#endif // LOGGING_H
