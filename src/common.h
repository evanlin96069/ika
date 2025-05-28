#ifndef COMMON_H
#define COMMON_H

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define UNREACHABLE() \
    do {              \
        assert(0);    \
        abort();      \
    } while (0)

#endif
