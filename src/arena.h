#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdint.h>

#define ARENA_ALIGNMENT 16

typedef struct Allocator {
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);
} Allocator;

typedef struct ArenaBlock {
    uint32_t size;
    uint32_t capacity;
    void* data;
    struct ArenaBlock* next;
} ArenaBlock;

typedef struct Arena {
    ArenaBlock* blocks;
    ArenaBlock* current_block;
    ArenaBlock* last_block;
    Allocator* allocator;
    uint32_t first_block_size;
} Arena;

void arena_init(Arena* arena, size_t first_block_size, Allocator* allocator);
void arena_deinit(Arena* arena);
void arena_reset(Arena* arena);

void* arena_alloc(Arena* arena, size_t size);
void* arena_realloc(Arena* arena, void* ptr, size_t old_size, size_t size);

#endif
