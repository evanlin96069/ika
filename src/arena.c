#include "arena.h"

#include <stdlib.h>
#include <string.h>

Allocator default_allocator = {
    .malloc = malloc,
    .free = free,
};

void arena_init(Arena* arena, size_t first_block_size, Allocator* allocator) {
    memset(arena, 0, sizeof(Arena));
    if (!allocator)
        allocator = &default_allocator;

    arena->allocator = allocator;
    arena->first_block_size = first_block_size;
}

void arena_deinit(Arena* arena) {
    ArenaBlock* curr = arena->blocks;
    while (curr) {
        ArenaBlock* temp = curr;
        curr = curr->next;
        arena->allocator->free(temp->data);
        arena->allocator->free(temp);
    }

    memset(arena, 0, sizeof(Arena));
}

void arena_reset(Arena* arena) {
    ArenaBlock* curr = arena->blocks;
    while (curr) {
        curr->size = 0;
        curr = curr->next;
    }
    arena->current_block = arena->blocks;
}

static inline size_t alignment_loss(size_t bytes_allocated, size_t alignment) {
    size_t offset = bytes_allocated & (alignment - 1);
    if (offset == 0)
        return 0;
    return alignment - offset;
}

static inline size_t block_bytes_left(ArenaBlock* blk) {
    size_t inc = alignment_loss(blk->size, ARENA_ALIGNMENT);
    return blk->capacity - (blk->size + inc);
}

static void alloc_new_block(Arena* arena, size_t requested_size) {
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

    if (allocated_size > UINT32_MAX)
        allocated_size = UINT32_MAX;

    ArenaBlock* blk = arena->allocator->malloc(sizeof(ArenaBlock));
    blk->data = arena->allocator->malloc(allocated_size);
    blk->size = 0;
    blk->capacity = allocated_size;
    blk->next = NULL;

    if (!arena->blocks) {
        arena->blocks = blk;
    } else {
        arena->last_block->next = blk;
    }
    arena->last_block = blk;
    arena->current_block = blk;
}

void* arena_alloc(Arena* arena, size_t size) {
    if (size == 0)
        return NULL;

    if (!arena->blocks)
        alloc_new_block(arena, size);

    while (block_bytes_left(arena->current_block) < size) {
        arena->current_block = arena->current_block->next;
        if (!arena->current_block) {
            alloc_new_block(arena, size);
            break;
        }
    }

    ArenaBlock* blk = arena->current_block;
    size_t inc = alignment_loss(blk->size, ARENA_ALIGNMENT);
    void* out = (uint8_t*)blk->data + blk->size + inc;
    blk->size += size + inc;
    return out;
}

void* arena_realloc(Arena* arena, void* ptr, size_t old_size, size_t size) {
    if (old_size >= size)
        return ptr;

    if (!ptr || !arena->blocks)
        return arena_alloc(arena, size);

    ArenaBlock* blk = arena->current_block;
    uint8_t* prev = (uint8_t*)blk->data + blk->size - old_size;
    uint32_t bytes_left = blk->capacity - blk->size + old_size;
    void* new_arr;
    if (prev == (uint8_t*)ptr && bytes_left >= size) {
        blk->size -= old_size;
        new_arr = arena_alloc(arena, size);
    } else {
        new_arr = arena_alloc(arena, size);
        memcpy(new_arr, ptr, old_size);
    }
    return new_arr;
}
