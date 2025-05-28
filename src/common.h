#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef _WIN32
#define OS_PATH_MAX 4096
#else
#define OS_PATH_MAX 260
#endif

#define UNREACHABLE() \
    do {              \
        assert(0);    \
        abort();      \
    } while (0)

#endif
