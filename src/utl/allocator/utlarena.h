#ifndef UTLARENA_H
#define UTLARENA_H

#include "../utldef.h"
#include "utlallocator.h"

typedef struct UtlArenaAllocator UtlArenaAllocator;

struct UtlArenaAllocator {
    void *(*alloc)(UtlArenaAllocator *arena, size_t size);
    void *(*remap)(UtlArenaAllocator *arena, void *buf, size_t new_size);
    void (*free)(UtlArenaAllocator *arena, void *buf);

    UtlAllocator *_allocator;
    size_t first_block_size;

    struct UtlArenaAllocatorBlock *blocks;
    struct UtlArenaAllocatorBlock *current_block;
    struct UtlArenaAllocatorBlock *last_block;
};

static inline UtlAllocator *utlarena_allocator(UtlArenaAllocator *arena) {
    return (UtlAllocator *)arena;
}

#define utlarena_init(block_size, allocator)              \
    {                                                     \
        .alloc = utlarena_alloc, .remap = utlarena_remap, \
        .free = utlarena_free, ._allocator = (allocator), \
        .first_block_size = (block_size),                 \
    }

void utlarena_deinit(UtlArenaAllocator *arena);
void utlarena_reset(UtlArenaAllocator *arena);

void *utlarena_alloc(UtlArenaAllocator *arena, size_t size);
void *utlarena_remap(UtlArenaAllocator *arena, void *buf, size_t new_size);
void utlarena_free(UtlArenaAllocator *arena, void *buf);

#ifdef UTLARENA_IMPLEMENTATION

#include <string.h>

typedef struct UtlArenaAllocatorBlock {
    size_t size;
    size_t capacity;
    void *data;
    struct UtlArenaAllocatorBlock *next;
} UtlArenaAllocatorBlock;

void utlarena_deinit(UtlArenaAllocator *arena) {
    UtlAllocator *allocator = arena->_allocator;
    UtlArenaAllocatorBlock *curr = arena->blocks;
    while (curr) {
        UtlArenaAllocatorBlock *temp = curr;
        curr = curr->next;
        allocator->free(allocator, temp->data);
        allocator->free(allocator, temp);
    }
    arena->blocks = NULL;
    arena->current_block = NULL;
    arena->last_block = NULL;
}

void utlarena_reset(UtlArenaAllocator *arena) {
    UtlArenaAllocatorBlock *curr = arena->blocks;
    while (curr) {
        curr->size = 0;
        curr = curr->next;
    }
    arena->current_block = arena->blocks;
}

static inline size_t _utl_block_bytes_left(UtlArenaAllocatorBlock *block) {
    size_t inc = _utl_alignment_loss(block->size, _UTL_MAX_ALIGNMENT);
    return block->capacity - (block->size + inc);
}

static bool _utlarena_alloc_block(UtlArenaAllocator *arena,
                                  size_t requested_size) {
    size_t allocated_size;
    if (!arena->blocks) {
        allocated_size = arena->first_block_size;
    } else {
        allocated_size = arena->last_block->capacity;
    }

    if (allocated_size < 1)
        allocated_size = 1;

    while (allocated_size < requested_size) {
        allocated_size *= 2;
    }

    if (allocated_size > UINT32_MAX) {
        allocated_size = UINT32_MAX;
    }

    UtlAllocator *allocator = arena->_allocator;

    UtlArenaAllocatorBlock *new_block =
        allocator->alloc(allocator, sizeof(UtlArenaAllocatorBlock));
    if (!new_block) {
        return false;
    }

    new_block->data = allocator->alloc(allocator, allocated_size);
    if (!new_block->data) {
        allocator->free(allocator, new_block);
        return false;
    }
    new_block->size = 0;
    new_block->capacity = allocated_size;
    new_block->next = NULL;

    if (!arena->blocks) {
        arena->blocks = new_block;
    } else {
        arena->last_block->next = new_block;
    }
    arena->last_block = new_block;
    arena->current_block = new_block;

    return true;
}

void *utlarena_alloc(UtlArenaAllocator *arena, size_t size) {
    if (size == 0) {
        return NULL;
    }

    size_t requested_size = size + sizeof(size_t);

    if (!arena->blocks && !_utlarena_alloc_block(arena, requested_size)) {
        return NULL;
    }

    while (_utl_block_bytes_left(arena->current_block) < requested_size) {
        arena->current_block = arena->current_block->next;
        if (!arena->current_block) {
            if (!_utlarena_alloc_block(arena, requested_size)) {
                return NULL;
            }
            break;
        }
    }

    UtlArenaAllocatorBlock *block = arena->current_block;
    size_t inc = _utl_alignment_loss(block->size, _UTL_MAX_ALIGNMENT);
    void *buf = (uint8_t *)block->data + block->size + inc;
    block->size += requested_size + inc;

    *((size_t *)buf) = size;
    buf = (size_t *)buf + 1;

    return buf;
}

static inline size_t _utlarena_get_buf_size(void *buf) {
    return *((size_t *)buf - 1);
}

static inline bool _utlarena_is_last_allocated(UtlArenaAllocator *arena,
                                               void *buf) {
    size_t buf_size = _utlarena_get_buf_size(buf);
    UtlArenaAllocatorBlock *block = arena->current_block;
    uint8_t *prev = (uint8_t *)block->data + block->size - buf_size;
    return prev == (uint8_t *)buf;
}

void *utlarena_remap(UtlArenaAllocator *arena, void *buf, size_t new_size) {
    if (!buf || !arena->blocks) {
        return utlarena_alloc(arena, new_size);
    }

    size_t old_size = _utlarena_get_buf_size(buf);
    if (old_size >= new_size) {
        return buf;
    }

    UtlArenaAllocatorBlock *block = arena->current_block;
    uint32_t bytes_left = block->capacity - block->size + old_size;
    void *new_arr;
    if (_utlarena_is_last_allocated(arena, buf) && bytes_left >= new_size) {
        utlarena_free(arena, buf);
        new_arr = utlarena_alloc(arena, new_size);
    } else {
        new_arr = utlarena_alloc(arena, new_size);
        if (new_arr) {
            memcpy(new_arr, buf, old_size);
        }
    }
    return new_arr;
}

void utlarena_free(UtlArenaAllocator *arena, void *buf) {
    if (_utlarena_is_last_allocated(arena, buf)) {
        UtlArenaAllocatorBlock *block = arena->current_block;
        size_t buf_size = _utlarena_get_buf_size(buf);
        block->size -= buf_size + sizeof(size_t);
    }
}

#endif  // UTLARENA_IMPLEMENTATION

#endif  // UTLARENA_H
