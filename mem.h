#ifndef MEM_MODULE
#define MEM_MODULE

#include <stdlib.h>
#define KILOBYTES(i) ((i) * 1024)
#define MEGABYTES(i) (KILOBYTES((i)) * 1024)

typedef struct BumpAlloc BumpAlloc;
BumpAlloc*
Bump_new(size_t capacity);
void*
Bump_alloc(BumpAlloc* bump, size_t size);

typedef struct MemoryPool MemoryPool;
MemoryPool*
Pool_new(size_t capacity, size_t block_size);
void*
Pool_alloc(MemoryPool* pool);
void
Pool_free(MemoryPool* pool, void* ptr);

#endif // !MEM_MODULE
