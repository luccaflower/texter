#include "mem.h"
#include "util.h"
#include <assert.h>
#include <string.h>

struct BumpAlloc*
Bump_new(size_t capacity)
{
    struct BumpAlloc* arena = Calloc(1, sizeof(struct BumpAlloc) + capacity);
    arena->brk = (void*)(arena + 1);
    arena->max = capacity;
    arena->allocated = 0;
    return arena;
}

void*
Bump_alloc(struct BumpAlloc* arena, size_t size)
{
    size = (size + 7) & ~7; // align to 8 bytes
    assert(arena->allocated + size < arena->max);
    void* ptr = arena->brk;
    arena->brk += size;
    arena->allocated += size;
    return ptr;
}

// fixed-size memory pool
struct MemoryPool*
Pool_new(size_t capacity, size_t block_size)
{
    // make it a multiple of 8 for alignment
    // this also guarantees that the block will be a least 8 bytes
    // which is enough to hold a pointer to the next node in the freelist
    block_size = (block_size + 7) & ~7;
    struct MemoryPool* pool = Calloc(1, sizeof(*pool) + capacity * block_size);
    pool->max_count = capacity;
    pool->block_size = block_size;
    pool->allocated = 0;
    pool->brk = (void*)(pool + 1);
    pool->freelist = NULL;
    return pool;
}

void*
Pool_alloc(struct MemoryPool* pool)
{
    if (pool->freelist) {
        struct PoolFreeList* free = pool->freelist;
        pool->freelist = free->next;
        memset(free, 0, sizeof(*free));
        return free;
    } else if (pool->allocated < pool->max_count) {
        void* ptr = pool->brk;
        pool->brk += pool->block_size;
        pool->allocated++;
        return ptr;
    } else {
        return NULL;
    }
}

void
Pool_free(struct MemoryPool* pool, void* ptr)
{
    struct PoolFreeList* free = ptr;
    free->next = pool->freelist;
}
