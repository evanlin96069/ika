#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define OS_PATH_MAX 4096
#define OS_PATH_SEP "\\"
#else
#define OS_PATH_MAX 260
#define OS_PATH_SEP "/"
#endif

#define UNREACHABLE() \
    do {              \
        assert(0);    \
        abort();      \
    } while (0)

#define UNUSED(x) (void)!(x)

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

#define ika_debug(...) ika_log(LOG_DEBUG, __VA_ARGS__)

typedef enum LogType {
    LOG_DEBUG,
    LOG_NOTE,
    LOG_WARNING,
    LOG_ERROR,
} LogType;

void ika_log(LogType level, const char* fmt, ...);

#endif
