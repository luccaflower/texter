#ifndef EDITOR
#define EDITOR

#include <sys/types.h>
#include <time.h>
struct EditorContext
{
    struct BumpAlloc* bmp;
    ssize_t cx, cy;
    ssize_t rx;
    ssize_t row_offset, col_offset;
    ssize_t screenrows;
    ssize_t screencols;
    ssize_t n_rows;
    int dirty;
    char status_msg[80];
    time_t status_time;
    struct GapBuffer* gap;
    struct GapBuffer** lines;
    char* filename;
    struct Abuf* ab;
};

void
init_editor(struct EditorContext* ctx, char* filename, struct BumpAlloc* bmp);
void
file_open(struct EditorContext* ctx, char* filename);
void
set_status(struct EditorContext* ctx, const char* fmt, ...);
void
refresh_ui(struct EditorContext* ctx);
char
read_input(void);
void
handle_input(struct EditorContext* ctx, char c);
#endif // !EDITOR
