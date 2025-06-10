#ifndef UTLALLOCATOR_H
#define UTLALLOCATOR_H

#include "../utldef.h"

typedef struct UtlAllocator {
    void *(*alloc)(void *context, size_t size);
    void *(*remap)(void *context, void *buf, size_t new_size);
    void (*free)(void *context, void *buf);
} UtlAllocator;

extern UtlAllocator utl_c_allocator;

#define _UTL_MAX_ALIGNMENT 16

static inline size_t _utl_alignment_loss(size_t bytes_allocated,
                                         size_t alignment) {
    size_t offset = bytes_allocated & (alignment - 1);
    if (offset == 0)
        return 0;
    return alignment - offset;
}

#ifdef UTLALLOCATOR_IMPLEMENTATION

#include <stdlib.h>

static void *_utl_c_alloc(void *context, size_t size) {
    (void)context;
    return malloc(size);
}

static void *_utl_c_remap(void *context, void *buf, size_t new_size) {
    (void)context;
    return realloc(buf, new_size);
}

static void _utl_c_free(void *context, void *buf) {
    (void)context;
    free(buf);
}

UtlAllocator utl_c_allocator = {
    .alloc = _utl_c_alloc,
    .remap = _utl_c_remap,
    .free = _utl_c_free,
};

#endif  // UTLALLOCATOR_IMPLEMENTATION

#endif  // UTLALLOCATOR_H
