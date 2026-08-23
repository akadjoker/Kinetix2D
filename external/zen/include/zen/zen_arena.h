#ifndef ZEN_ARENA_H
#define ZEN_ARENA_H

#include <stddef.h>
#include <stdint.h>

/*
** zen_arena.h — Pool allocator (inspired by Box2D's b2BlockAllocator).
**
** Manages 14 size buckets (16..640 bytes). Allocs <= 640 are O(1)
** free-list pops; frees are O(1) pushes. Larger allocs fall through
** to malloc/free.
**
** Provides Reallocate() that avoids copy when old/new fit same bucket.
*/

namespace zen
{

static const int kArenaChunkSize = 16 * 1024;       /* 16 KB per chunk */
static const int kArenaMaxBlockSize = 640;
static const int kArenaNumBuckets = 14;
static const int kArenaChunkArrayIncrement = 128;

struct ArenaBlock
{
    ArenaBlock *next;
};

struct ArenaChunk
{
    int block_size;
    ArenaBlock *blocks;
};

struct Arena
{
    ArenaChunk *chunks;
    int chunk_count;
    int chunk_capacity;

    ArenaBlock *free_lists[kArenaNumBuckets];

    size_t bytes_allocated;
    size_t bytes_reserved;

    static int s_block_sizes[kArenaNumBuckets];
    static uint8_t s_size_to_bucket[kArenaMaxBlockSize + 1];
    static bool s_lookup_initialized;
};

void arena_init(Arena *a);
void arena_destroy(Arena *a);

void *arena_alloc(Arena *a, size_t size);
void  arena_free(Arena *a, void *p, size_t size);
void *arena_realloc(Arena *a, void *p, size_t old_size, size_t new_size);

} /* namespace zen */

#endif /* ZEN_ARENA_H */
