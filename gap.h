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

void
Gap_insert(struct GapBuffer* gap, char* buf);

void
Gap_mov(struct GapBuffer* gap, int steps);
#endif // !GAP_BUFFER
