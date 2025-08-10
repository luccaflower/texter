#ifndef EDITOR
#define EDITOR

#include <time.h>
struct EditorContext
{
    struct BumpAlloc* bmp;
    int cx, cy;
    int rx;
    int row_offset, col_offset;
    int screenrows;
    int screencols;
    int n_rows;
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
file_open(struct EditorContext* ctx, struct BumpAlloc* arena, char* filename);
void
set_status(struct EditorContext* ctx, const char* fmt, ...);
void
refresh_ui(struct EditorContext* ctx);
char
read_input(void);
void
handle_input(struct EditorContext* ctx, char c);
#endif // !EDITOR
