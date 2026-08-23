#include "zen_arena.h"
#include <cstdlib>
#include <cstring>
#include <cassert>

namespace zen
{

/* Block sizes: 14 buckets covering 16..640 bytes */
int Arena::s_block_sizes[kArenaNumBuckets] = {
    16, 32, 64, 96, 128, 160, 192, 224, 256, 320, 384, 448, 512, 640
};
uint8_t Arena::s_size_to_bucket[kArenaMaxBlockSize + 1];
bool Arena::s_lookup_initialized = false;

void arena_init(Arena *a)
{
    a->chunk_count = 0;
    a->chunk_capacity = kArenaChunkArrayIncrement;
    a->chunks = (ArenaChunk *)malloc(sizeof(ArenaChunk) * a->chunk_capacity);
    memset(a->chunks, 0, sizeof(ArenaChunk) * a->chunk_capacity);

    for (int i = 0; i < kArenaNumBuckets; i++)
        a->free_lists[i] = nullptr;

    a->bytes_allocated = 0;
    a->bytes_reserved = 0;

    if (!Arena::s_lookup_initialized)
    {
        int j = 0;
        for (int i = 1; i <= kArenaMaxBlockSize; i++)
        {
            assert(j < kArenaNumBuckets);
            if (i <= Arena::s_block_sizes[j])
                Arena::s_size_to_bucket[i] = (uint8_t)j;
            else
            {
                ++j;
                Arena::s_size_to_bucket[i] = (uint8_t)j;
            }
        }
        Arena::s_lookup_initialized = true;
    }
}

void arena_destroy(Arena *a)
{
    for (int i = 0; i < a->chunk_count; i++)
        free(a->chunks[i].blocks);
    free(a->chunks);
    a->chunks = nullptr;
    a->chunk_count = 0;
    a->chunk_capacity = 0;
}

void *arena_alloc(Arena *a, size_t size)
{
    if (size == 0) return nullptr;

    /* Large allocation — fallback to malloc */
    if (size > (size_t)kArenaMaxBlockSize)
    {
        a->bytes_allocated += size;
        return malloc(size);
    }

    int index = Arena::s_size_to_bucket[size];
    int block_size = Arena::s_block_sizes[index];

    /* Fast path: pop from free-list */
    if (a->free_lists[index])
    {
        ArenaBlock *block = a->free_lists[index];
        a->free_lists[index] = block->next;
        a->bytes_allocated += block_size;
        return block;
    }

    /* Grow chunk array if needed */
    if (a->chunk_count == a->chunk_capacity)
    {
        ArenaChunk *old = a->chunks;
        a->chunk_capacity += kArenaChunkArrayIncrement;
        a->chunks = (ArenaChunk *)malloc(sizeof(ArenaChunk) * a->chunk_capacity);
        memcpy(a->chunks, old, sizeof(ArenaChunk) * a->chunk_count);
        memset(a->chunks + a->chunk_count, 0,
               sizeof(ArenaChunk) * kArenaChunkArrayIncrement);
        free(old);
    }

    /* Allocate a new chunk and carve it into blocks */
    ArenaChunk *chunk = &a->chunks[a->chunk_count++];
    chunk->blocks = (ArenaBlock *)malloc(kArenaChunkSize);
    chunk->block_size = block_size;
    a->bytes_reserved += kArenaChunkSize;

    int block_count = kArenaChunkSize / block_size;
    char *base = (char *)chunk->blocks;

    /* Build free-list from blocks [1..n-1] */
    for (int i = 0; i < block_count - 1; i++)
    {
        ArenaBlock *b = (ArenaBlock *)(base + block_size * i);
        ArenaBlock *next = (ArenaBlock *)(base + block_size * (i + 1));
        b->next = next;
    }
    ArenaBlock *last = (ArenaBlock *)(base + block_size * (block_count - 1));
    last->next = nullptr;

    /* Return first block, rest goes to free-list */
    a->free_lists[index] = chunk->blocks->next;
    a->bytes_allocated += block_size;
    return chunk->blocks;
}

void arena_free(Arena *a, void *p, size_t size)
{
    if (!p || size == 0) return;

    /* Large allocation — fallback to free */
    if (size > (size_t)kArenaMaxBlockSize)
    {
        a->bytes_allocated -= size;
        free(p);
        return;
    }

    int index = Arena::s_size_to_bucket[size];
    int block_size = Arena::s_block_sizes[index];
    a->bytes_allocated -= block_size;

    /* Push onto free-list */
    ArenaBlock *block = (ArenaBlock *)p;
    block->next = a->free_lists[index];
    a->free_lists[index] = block;
}

void *arena_realloc(Arena *a, void *p, size_t old_size, size_t new_size)
{
    if (new_size == 0)
    {
        arena_free(a, p, old_size);
        return nullptr;
    }
    if (!p || old_size == 0)
        return arena_alloc(a, new_size);

    /* Both fit in arena — check if same bucket */
    if (old_size <= (size_t)kArenaMaxBlockSize && new_size <= (size_t)kArenaMaxBlockSize)
    {
        int old_idx = Arena::s_size_to_bucket[old_size];
        int new_idx = Arena::s_size_to_bucket[new_size];
        if (old_idx == new_idx)
            return p; /* Same bucket — no work needed! */

        /* Different bucket: alloc new, copy, free old */
        void *np = arena_alloc(a, new_size);
        memcpy(np, p, old_size < new_size ? old_size : new_size);
        arena_free(a, p, old_size);
        return np;
    }

    /* Transitioning between arena and malloc, or both large */
    if (old_size > (size_t)kArenaMaxBlockSize && new_size > (size_t)kArenaMaxBlockSize)
    {
        /* Both large — use realloc directly */
        a->bytes_allocated += new_size - old_size;
        return realloc(p, new_size);
    }

    /* Mixed: one in arena, one in malloc */
    void *np = arena_alloc(a, new_size);
    memcpy(np, p, old_size < new_size ? old_size : new_size);
    arena_free(a, p, old_size);
    return np;
}

} /* namespace zen */
