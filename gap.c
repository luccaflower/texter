
#include "gap.h"
#include "util.h"
#include <string.h>
#define GAP_SIZE (16)

struct GapBuffer*
Gap_new(char* buf)
{
    size_t sz = strlen(buf);
    struct GapBuffer* gap = Malloc(sizeof(*gap));
    gap->size = sz;
    gap->cur_beg = 0;
    gap->cur_end = GAP_SIZE;
    gap->buf = Malloc(GAP_SIZE + sz + 1);
    memset(gap->buf, 0, GAP_SIZE);
    memcpy(gap->buf + GAP_SIZE, buf, sz);
    gap->buf[sz + GAP_SIZE] = '\0';
    return gap;
}

void
Gap_str(struct GapBuffer* gap, char* out)
{
    memcpy(out, gap->buf, gap->cur_beg);
    memcpy(
      &out[gap->cur_beg], &gap->buf[gap->cur_end], gap->size - gap->cur_beg);
    out[gap->size] = '\0';
}

void
Gap_substr(struct GapBuffer* gap, int from, int to, char* buf)
{
    if (from >= to || from < 0) {
        char* buf = Malloc(1);
        buf[0] = '\0';
    }
    if (to > gap->size) {
        to = gap->size;
    }
    size_t len = to - from;
    if (from + len <= gap->cur_beg) {
        memcpy(buf + from, gap->buf, len);
        buf[len] = '\0';
    } else if (from < gap->cur_beg) {
        memcpy(buf, gap->buf + from, gap->cur_beg - from);
        memcpy(buf + gap->cur_beg - from,
               gap->buf + gap->cur_end,
               len - (gap->cur_beg - from));
        buf[len] = '\0';
    } else {
        memcpy(buf, gap->buf + gap->cur_end + (from - gap->cur_beg), len);
        buf[len] = '\0';
    }
}

void
Gap_insert_str(struct GapBuffer* gap, char* buf)
{
    size_t len = strlen(buf);
    size_t gap_len = gap->cur_end - gap->cur_beg;
    if (len > gap_len) {
        gap->buf = Realloc(gap->buf, gap->size + len + GAP_SIZE + 1);
        char* from = &gap->buf[gap->cur_end];
        char* to = &gap->buf[gap->cur_beg + GAP_SIZE + len];
        size_t movsize = gap->size - gap->cur_beg + 1;
        memmove(to, from, movsize);
        memcpy(gap->buf + gap->cur_beg, buf, len);
        gap->cur_beg += len;
        gap->cur_end = gap->cur_beg + GAP_SIZE;
        gap->size += len;
    } else {
        memcpy(gap->buf + gap->cur_beg, buf, len);
        gap->cur_beg += len;
        gap->size += len;
    }
}

void
Gap_insert_chr(struct GapBuffer* gap, char c)
{

    if (!(gap->cur_end - gap->cur_beg)) {
        gap->buf = Realloc(gap->buf, gap->size + GAP_SIZE + 1);
        char* from = &gap->buf[gap->cur_end];
        char* to = &gap->buf[gap->cur_end + GAP_SIZE];
        memmove(to, from, gap->size - gap->cur_beg + 1);
        gap->cur_end += GAP_SIZE;
    }
    gap->buf[gap->cur_beg] = c;
    gap->cur_beg++;
    gap->size++;
}

void
Gap_mov(struct GapBuffer* gap, int steps)
{
    if (steps > 0) {
        if (gap->cur_beg + steps > gap->size) {
            steps = gap->size - gap->cur_beg;
        }
        char* from = &gap->buf[gap->cur_end];
        char* to = &gap->buf[gap->cur_beg];
        memmove(to, from, steps);
    } else {
        if (gap->cur_beg < -steps) {
            steps = -gap->cur_beg;
        }
        char* from = &gap->buf[gap->cur_beg + steps];
        char* to = &gap->buf[gap->cur_end + steps];
        memmove(to, from, -steps);
    }
    gap->cur_beg += steps;
    gap->cur_end += steps;
}

void
Gap_del(struct GapBuffer* gap, int steps)
{
    if (gap->cur_beg >= gap->size) {
        return;
    }
    if (steps > gap->size - gap->cur_beg) {
        steps = gap->size - gap->cur_beg;
    }
    gap->size -= steps;
    gap->cur_end += steps;
}
