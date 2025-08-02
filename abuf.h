#ifndef ABUF
#define ABUF
#include <stdlib.h>
struct Abuf
{
    char* cursor;
    size_t len;
    size_t capacity;
    char buf[];
};

void
Abuf_init(struct Abuf* ab, size_t capacity);

void
Abuf_append(struct Abuf* ab, const char* s, size_t len);

void
Abuf_reset(struct Abuf* ab);

#endif
