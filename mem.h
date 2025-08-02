#ifndef MEM_MODULE
#define MEM_MODULE

#include <stdlib.h>
#define KILOBYTES(i) ((i) * 1024)
#define MEGABYTES(i) (KILOBYTES((i)) * 1024)

struct BumpAlloc
{
    size_t max;
    size_t allocated;
    void* brk;
};

struct BumpAlloc*
Bump_new(size_t capacity);
void*
Bump_alloc(struct BumpAlloc* bump, size_t size);

struct PoolFreeList
{
    struct PoolFreeList* next;
};
struct MemoryPool
{
    size_t max_count;
    size_t allocated;
    size_t block_size;
    void* brk;
    struct PoolFreeList* freelist;
};
struct MemoryPool*
Pool_new(size_t capacity, size_t block_size);
void*
Pool_alloc(struct MemoryPool* pool);
void
Pool_free(struct MemoryPool* pool, void* ptr);

#endif // !MEM_MODULE
