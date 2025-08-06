#ifndef GAP_BUFFER
#define GAP_BUFFER
#include <stdlib.h>

struct GapBuffer
{
    size_t size;
    size_t cur_beg;
    size_t cur_end;
    char* buf;
};

struct GapBuffer*
Gap_new(char* buf);

char*
Gap_str(struct GapBuffer* gap);

char*
Gap_substr(struct GapBuffer* gap, int from, int to);

void
Gap_insert_str(struct GapBuffer* gap, char* buf);

void
Gap_insert_chr(struct GapBuffer* gap, char c);

void
Gap_mov(struct GapBuffer* gap, int steps);

void
Gap_del(struct GapBuffer* gap, int chars);
#endif // !GAP_BUFFER
