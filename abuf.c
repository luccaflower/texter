#include "abuf.h"
#include <assert.h>
#include <string.h>

void
Abuf_init(struct Abuf* ab, size_t capacity)
{
    ab->len = 0;
    ab->capacity = capacity;
    ab->cursor = ab->buf;
}

void
Abuf_append(struct Abuf* ab, const char* s, size_t len)
{
    assert(ab->len + len <= ab->capacity);
    memcpy(ab->cursor, s, len);
    ab->cursor += len;
    ab->len += len;
}

void
Abuf_reset(struct Abuf* ab)
{
    ab->cursor = ab->buf;
    ab->buf[0] = '\0';
    ab->len = 0;
}
