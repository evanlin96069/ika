#ifndef UTLSTACKFALLBACK_H
#define UTLSTACKFALLBACK_H

#include "utlfixedbuf.h"

typedef struct UtlStackFallbackAllocator UtlStackFallbackAllocator;

struct UtlStackFallbackAllocator {
    void *(*alloc)(UtlStackFallbackAllocator *sf, size_t size);
    void *(*remap)(UtlStackFallbackAllocator *sf, void *buf, size_t new_size);
    void (*free)(UtlStackFallbackAllocator *sf, void *buf);

    UtlFixedBufferAllocator allocator;
    UtlAllocator *fallback;
};

static inline UtlAllocator *utlstackfallback_allocator(
    UtlStackFallbackAllocator *sf) {
    return (UtlAllocator *)sf;
}

#define utlstackfallback_init(buf, len, _fallback)   \
    {                                                \
        .alloc = utlstackfallback_alloc,             \
        .remap = utlstackfallback_remap,             \
        .free = utlstackfallback_free,               \
        .allocator = utlfixedbuf_init((buf), (len)), \
        .fallback = (_fallback),                     \
    }

void *utlstackfallback_alloc(UtlStackFallbackAllocator *sf, size_t size);
void *utlstackfallback_remap(UtlStackFallbackAllocator *sf, void *buf,
                             size_t new_size);
void utlstackfallback_free(UtlStackFallbackAllocator *sf, void *buf);

#ifdef UTLSTACKFALLBACK_IMPLEMENTATION

#include <string.h>

void *utlstackfallback_alloc(UtlStackFallbackAllocator *sf, size_t size) {
    void *buf = sf->allocator.alloc(&sf->allocator, size);
    if (!buf) {
        buf = sf->fallback->alloc(sf->fallback, size);
    }
    return buf;
}

void *utlstackfallback_remap(UtlStackFallbackAllocator *sf, void *buf,
                             size_t new_size) {
    if (!buf) {
        return utlstackfallback_alloc(sf, new_size);
    }

    if (_utlfixedbuf_is_owned(&sf->allocator, buf)) {
        void *new_buf = sf->allocator.remap(&sf->allocator, buf, new_size);
        if (!new_buf) {
            new_buf = sf->fallback->alloc(sf->fallback, new_size);
            if (new_buf) {
                size_t old_size = _utlfixedbuf_get_buf_size(buf);
                memcpy(new_buf, buf, old_size);
            }
        }
        return new_buf;
    }

    return sf->fallback->remap(sf->fallback, buf, new_size);
}
void utlstackfallback_free(UtlStackFallbackAllocator *sf, void *buf) {
    if (_utlfixedbuf_is_owned(&sf->allocator, buf)) {
        sf->allocator.free(&sf->allocator, buf);
    } else {
        sf->fallback->free(sf->fallback, buf);
    }
}

#endif  // UTLSTACKFALLBACK_IMPLEMENTATION

#endif  // UTLSTACKFALLBACK_H