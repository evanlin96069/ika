#ifndef UTLVECTOR_H
#define UTLVECTOR_H

#include <string.h>

#include "allocator/utlallocator.h"

#define UtlVector(T)             \
    struct {                     \
        size_t size;             \
        T* data;                 \
        size_t capacity;         \
        UtlAllocator* allocator; \
    }

typedef UtlVector(void*) UtlVector_void;

#define utlvector_init(_allocator) \
    { .allocator = (_allocator) }

#define utlvector_push(v, val)                                         \
    (_utlvector_ensure((UtlVector_void*)(v), sizeof(*(v)->data), 1) && \
     ((v)->data[(v)->size++] = (val), true))

#define utlvector_pushall(v, arr, count)                                     \
    (_utlvector_ensure((UtlVector_void*)(v), sizeof(*(v)->data), (count)) && \
     (memcpy(&(v)->data[(v)->size], (arr), (count) * sizeof(*(arr))),        \
      (v)->size += (count), true))

#define utlvector_pop(v) ((v)->size > 0 && ((void)(v)->data[--(v)->size], true))

#define utlvector_clear(v) (v)->size = 0;

#define utlvector_deinit(v)                              \
    do {                                                 \
        (v)->size = 0;                                   \
        (v)->capacity = 0;                               \
        (v)->allocator->free((v)->allocator, (v)->data); \
        (v)->data = NULL;                                \
    } while (0)

bool _utlvector_ensure(UtlVector_void* v, size_t element_size,
                       size_t add_count);

#ifdef UTLVECTOR_IMPLEMENTATION

bool _utlvector_ensure(UtlVector_void* v, size_t element_size,
                       size_t add_count) {
    size_t needed = v->size + add_count;
    if (needed <= v->capacity) {
        return true;
    }

    size_t new_capacity = (v->capacity == 0) ? 16 : v->capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }

    void* new_buf =
        v->allocator->remap(v->allocator, v->data, new_capacity * element_size);

    if (new_buf == NULL) {
        return false;
    }

    v->data = new_buf;
    v->capacity = new_capacity;
    return true;
}

#endif  // UTLVECTOR_IMPLEMENTATION

#endif  // UTLVECTOR_H
