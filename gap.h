#ifndef GAP_BUFFER
#define GAP_BUFFER
#include <stdlib.h>

struct GapBuffer
{
    ssize_t size;
    ssize_t cur_beg;
    ssize_t cur_end;
    char* buf;
};

struct GapBuffer*
Gap_new(char* buf);

void
Gap_str(struct GapBuffer* gap, char* out);

void
Gap_substr(struct GapBuffer* gap, int from, int to, char* out);

void
Gap_insert_str(struct GapBuffer* gap, char* buf);

void
Gap_insert_chr(struct GapBuffer* gap, char c);

void
Gap_mov(struct GapBuffer* gap, int steps);

void
Gap_del(struct GapBuffer* gap, int chars);

void
Gap_nextline(struct GapBuffer* gap);

void
Gap_prevline(struct GapBuffer* gap);

void
Gap_window(struct GapBuffer* gap, int back, int front, int width, char* window);
#endif // !GAP_BUFFER
