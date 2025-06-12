#ifndef UTLFIXEDBUF_H
#define UTLFIXEDBUF_H

#include "stddef.h"
#include "utlallocator.h"

typedef struct UtlFixedBufferAllocator UtlFixedBufferAllocator;

struct UtlFixedBufferAllocator {
    void *(*alloc)(UtlFixedBufferAllocator *fixedbuf, size_t size);
    void *(*remap)(UtlFixedBufferAllocator *fixedbuf, void *buf,
                   size_t new_size);
    void (*free)(UtlFixedBufferAllocator *fixedbuf, void *buf);

    size_t size;
    size_t capacity;
    void *data;
};

static inline UtlAllocator *utlfixedbuf_allocator(
    UtlFixedBufferAllocator *fixedbuf) {
    return (UtlAllocator *)fixedbuf;
}

#define utlfixedbuf_init(buf, len)                                             \
    {                                                                          \
        .alloc = utlfixedbuf_alloc, .remap = utlfixedbuf_remap,                \
        .free = utlfixedbuf_free, .size = 0, .capacity = (len), .data = (buf), \
    }

void utlfixedbuf_reset(UtlFixedBufferAllocator *fixedbuf);

void *utlfixedbuf_alloc(UtlFixedBufferAllocator *fixedbuf, size_t size);
void *utlfixedbuf_remap(UtlFixedBufferAllocator *fixedbuf, void *buf,
                        size_t new_size);
void utlfixedbuf_free(UtlFixedBufferAllocator *fixedbuf, void *buf);

static inline bool _utlfixedbuf_is_owned(UtlFixedBufferAllocator *fixedbuf,
                                         void *buf) {
    return (uint8_t *)fixedbuf->data <= (uint8_t *)buf &&
           (uint8_t *)buf < (uint8_t *)fixedbuf->data + fixedbuf->capacity;
}

static inline size_t _utlfixedbuf_get_buf_size(void *buf) {
    return *((size_t *)buf - 1);
}

#ifdef UTLFIXEDBUF_IMPLEMENTATION

#include <string.h>

void utlfixedbuf_reset(UtlFixedBufferAllocator *fixedbuf) {
    fixedbuf->size = 0;
}

void *utlfixedbuf_alloc(UtlFixedBufferAllocator *fixedbuf, size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t requested_size = size + sizeof(size_t);

    size_t inc = _utl_alignment_loss(fixedbuf->size, _UTL_MAX_ALIGNMENT);
    if (fixedbuf->capacity - fixedbuf->size < requested_size + inc) {
        return NULL;
    }

    void *buf = (uint8_t *)fixedbuf->data + fixedbuf->size + inc;
    fixedbuf->size += requested_size + inc;

    *((size_t *)buf) = size;
    buf = (size_t *)buf + 1;

    return buf;
}

static inline bool _utlfixedbuf_is_last_allocated(
    UtlFixedBufferAllocator *fixedbuf, void *buf) {
    size_t buf_size = _utlfixedbuf_get_buf_size(buf);
    uint8_t *prev = (uint8_t *)fixedbuf->data + fixedbuf->size - buf_size;
    return prev == (uint8_t *)buf;
}

void *utlfixedbuf_remap(UtlFixedBufferAllocator *fixedbuf, void *buf,
                        size_t new_size) {
    if (!buf) {
        return utlfixedbuf_alloc(fixedbuf, new_size);
    }

    size_t old_size = _utlfixedbuf_get_buf_size(buf);
    if (old_size >= new_size) {
        return buf;
    }

    uint32_t bytes_left = fixedbuf->capacity - fixedbuf->size + old_size;
    void *new_arr;
    if (_utlfixedbuf_is_last_allocated(fixedbuf, buf) &&
        bytes_left >= new_size) {
        utlfixedbuf_free(fixedbuf, buf);
        new_arr = utlfixedbuf_alloc(fixedbuf, new_size);
    } else {
        new_arr = utlfixedbuf_alloc(fixedbuf, new_size);
        if (new_arr) {
            memcpy(new_arr, buf, old_size);
        }
    }
    return new_arr;
}

void utlfixedbuf_free(UtlFixedBufferAllocator *fixedbuf, void *buf) {
    if (_utlfixedbuf_is_last_allocated(fixedbuf, buf)) {
        size_t buf_size = _utlfixedbuf_get_buf_size(buf);
        fixedbuf->size -= buf_size + sizeof(size_t);
    }
}

#endif  // UTLFIXEDBUF_IMPLEMENTATION

#endif  // UTLFIXEDBUF_H
