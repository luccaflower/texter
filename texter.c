#include "texter.h"
#include "abuf.h"
#include "gap.h"
#include "mem.h"
#include "util.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#define TEXTER_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)
#define CLEAR_SCREEN ("\x1b[2J")
#define RESET_CURSOR ("\x1b[H")
#define BLINK_CURSOR ("\x1b[?25h")
#define ERASE_LINE ("\x1b[K")
#define TABWIDTH (4)

char*
prompt(struct EditorContext* ctx, char* prompt);

int
window_size(ssize_t* rows, ssize_t* cols)
{
    const char* lines = getenv("LINES");
    const char* columns = getenv("COLUMNS");
    int advance;
    if (lines && columns) {
        sscanf(lines, "%zd%n", rows, &advance);
        sscanf(columns, "%zd%n", cols, &advance);
        return 0;
    } else {
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
            return -1;
        } else {
            *cols = ws.ws_col;
            *rows = ws.ws_row;
            return 0;
        }
    }
}

void
del_row(struct EditorContext* ctx, unsigned at)
{
    if (at >= ctx->n_rows) {
        return;
    }
    free(ctx->lines[at]);
    memmove(&ctx->lines[at],
            &ctx->lines[at + 1],
            sizeof(*ctx->lines) * (ctx->n_rows - at - 1));
    ctx->n_rows--;
    ctx->dirty++;
}

void
insert_row(struct EditorContext* ctx, unsigned at, char* s)
{
    if (at > ctx->n_rows) {
        return;
    }
    ctx->lines = Realloc(ctx->lines, sizeof(*ctx->lines) * (ctx->n_rows + 1));
    memmove(&ctx->lines[at + 1],
            &ctx->lines[at],
            sizeof(*ctx->lines) * (ctx->n_rows - at));
    ctx->lines[at] = Gap_new(s);

    ctx->n_rows++;
    ctx->dirty++;
}

int
row_cx_to_rx(struct GapBuffer* row, struct BumpAlloc scratch, int cx)
{
    int rx = 0;
    char* buf = Bump_alloc(&scratch, cx + 1);
    Gap_substr(row, 0, cx, buf);
    for (int i = 0; i < cx; i++) {
        if (buf[i] == '\t') {
            rx += TABWIDTH - (rx % TABWIDTH);
        } else {
            rx++;
        }
    }
    return rx;
}

void
editor_scroll(struct EditorContext* ctx)
{
    ctx->rx = 0;
    if (ctx->cy < ctx->n_rows) {
        ctx->rx = row_cx_to_rx(ctx->lines[ctx->cy], *ctx->bmp, ctx->cx);
    }
    if (ctx->cy < ctx->row_offset) {
        ctx->row_offset = ctx->cy;
    } else if (ctx->cy >= ctx->row_offset + ctx->screenrows) {
        ctx->row_offset = ctx->cy - ctx->screenrows + 1;
    }
    if (ctx->rx < ctx->col_offset) {
        ctx->col_offset = ctx->rx;
    } else if (ctx->rx >= ctx->col_offset + ctx->screencols) {
        ctx->col_offset = ctx->rx + ctx->screencols + 1;
    }
}

void
draw_rows(struct EditorContext* ctx, struct Abuf* ab)
{
    for (unsigned y = 0; y < ctx->screenrows; y++) {
        unsigned filerow = y + ctx->row_offset;
        if (filerow >= ctx->n_rows) {
            if (ctx->n_rows == 0 && y == (ctx->screenrows / 3)) {
                const char welcome[] =
                  "Tutorial text-editor -- version " TEXTER_VERSION;
                size_t welcome_len = sizeof(welcome);
                int padding = (ctx->screencols - welcome_len) / 2;
                if (padding) {
                    Abuf_append(ab, "~", 1);
                    padding--;
                }
                while (padding) {
                    Abuf_append(ab, " ", 1);
                    padding--;
                }
                Abuf_append(ab, welcome, welcome_len);
            } else {
                Abuf_append(ab, "~", 1);
            }
        } else {
            struct BumpAlloc bmp = *ctx->bmp;
            struct GapBuffer* gap = ctx->lines[filerow];
            unsigned len = gap->size - ctx->col_offset;
            if (gap->size < ctx->col_offset) {
                len = 0;
            } else {
                len = gap->size - ctx->col_offset;
            }
            if (len > ctx->screencols) {
                len = ctx->screencols;
            }
            char* to_render = Bump_alloc(&bmp, ctx->screencols + 1);
            Gap_substr(gap,
                       ctx->col_offset,
                       ctx->col_offset + ctx->screencols,
                       to_render);
            int tabs = 0;
            for (size_t i = 0; to_render[i]; i++) {
                if (to_render[i] == '\t') {
                    tabs++;
                }
            }
            int tab_chars = tabs * (TABWIDTH - 1);
            // a render string only needs to live as long as a single interation
            char* render = Bump_alloc(&bmp, strlen(to_render) + tab_chars + 1);
            int i = 0;
            for (int j = 0; to_render[j]; j++) {
                if (to_render[j] == '\t') {
                    i +=
                      snprintf(render + i, TABWIDTH + 1, "%*c", TABWIDTH, ' ');
                } else {
                    render[i] = to_render[j];
                    i++;
                }
            }
            Abuf_append(ab, render, i);
        }
        Abuf_append(ab, ERASE_LINE, sizeof(ERASE_LINE));
        Abuf_append(ab, "\r\n", 2);
    }
}

void
place_cursor(struct EditorContext* ctx, struct Abuf* ab)
{
    char buf[64];
    snprintf(buf,
             sizeof(buf),
             "\x1b[%zd;%zdH",
             (ctx->cy - ctx->row_offset) + 1,
             (ctx->rx - ctx->col_offset) + 1);
    Abuf_append(ab, buf, strlen(buf));
}

void
draw_status_bar(struct EditorContext* ctx, struct Abuf* ab)
{
    Abuf_append(ab, "\x1b[7m", 4);
    char status[80], rstatus[80];
    char* filename = ctx->filename ? ctx->filename : "[No Name]";
    unsigned len = snprintf(status,
                            sizeof(status),
                            "%.20s - %zd lines %s",
                            filename,
                            ctx->n_rows,
                            ctx->dirty ? "(modified)" : "");
    unsigned rlen =
      snprintf(rstatus, sizeof(rstatus), "%zd/%zd", ctx->cy + 1, ctx->n_rows);
    if (len > ctx->screencols) {
        len = ctx->screencols;
    }
    Abuf_append(ab, status, len);
    for (; len < ctx->screencols; len++) {
        if (ctx->screencols - len == rlen) {
            Abuf_append(ab, rstatus, rlen);
            break;
        }
        Abuf_append(ab, " ", 1);
    }
    Abuf_append(ab, "\x1b[m", 3);
    Abuf_append(ab, "\r\n", 2);
}

void
draw_status_msg(struct EditorContext* ctx, struct Abuf* ab)
{
    Abuf_append(ab, "\x1b[K", 3);
    unsigned msglen = strlen(ctx->status_msg);
    if (msglen > ctx->screencols) {
        msglen = ctx->screencols;
    }
    if (msglen && time(NULL) - ctx->status_time < 5) {
        Abuf_append(ab, ctx->status_msg, msglen);
    }
}

void
set_status(struct EditorContext* ctx, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(ctx->status_msg, sizeof(ctx->status_msg), fmt, ap);
    va_end(ap);
    ctx->status_time = time(NULL);
}

void
refresh_ui(struct EditorContext* ctx)
{
    struct Abuf* ab = ctx->ab;
    editor_scroll(ctx);
    Abuf_append(ab, RESET_CURSOR, sizeof(RESET_CURSOR));
    draw_rows(ctx, ab);
    draw_status_bar(ctx, ab);
    draw_status_msg(ctx, ab);
    place_cursor(ctx, ab);
    Abuf_append(ab, BLINK_CURSOR, sizeof(BLINK_CURSOR));
    write(STDOUT_FILENO, ab->buf, ab->len);
    Abuf_reset(ab);
}

void
init_editor(struct EditorContext* ctx, char* filename, struct BumpAlloc* bmp)
{
    ctx->bmp = bmp;
    ctx->cx = 0;
    ctx->cy = 0;
    ctx->rx = 0;
    ctx->row_offset = 0;
    ctx->col_offset = 0;
    ctx->n_rows = 0;
    ctx->lines = NULL;
    ctx->filename = filename;
    ctx->dirty = 0;
    ctx->status_msg[0] = '\0';
    ctx->status_time = 0;
    if (window_size(&ctx->screenrows, &ctx->screencols) == -1) {
        unix_error("init window");
    }
    size_t capacity = (ctx->screenrows + 2) * (ctx->screencols + 2);
    struct Abuf* ab = Bump_alloc(ctx->bmp, sizeof(*ab) + capacity);
    Abuf_init(ab, capacity);
    ctx->ab = ab;
    ctx->screenrows -= 2;
}

/***** file i/o *****/

void
save_buf(struct EditorContext* ctx)
{
    struct BumpAlloc scratch = *ctx->bmp;
    if (!ctx->filename) {
        ctx->filename = prompt(ctx, "Save as: %s");
    }

    int totlen = 0;
    for (unsigned i = 0; i < ctx->n_rows; i++) {
        totlen += ctx->lines[i]->size + 1;
    }
    int len = totlen;
    char* buf = Bump_alloc(&scratch, totlen);
    char* p = buf;
    for (unsigned i = 0; i < ctx->n_rows; i++) {
        struct GapBuffer* gap = ctx->lines[i];
        char* cpy = Bump_alloc(&scratch, gap->size + 1);
        Gap_str(gap, cpy);
        memcpy(p, cpy, gap->size);
        p += gap->size;
        *p = '\n';
        p++;
    }
    int fd = open(ctx->filename, (O_RDWR | O_CREAT), 0644);
    if (fd == -1) {
        return;
    }
    if (ftruncate(fd, len) != -1) {
        if (write(fd, buf, len) == len) {
            set_status(ctx, "%d bytes written to disk", len);
            ctx->dirty = 0;
        } else {
            set_status(ctx,
                       "failed to write some or all of buffer: %s",
                       strerror(errno));
        }
    }
    close(fd);
}

void
file_open(struct EditorContext* ctx, char* filename)
{
    FILE* fd = fopen(filename, "r");
    if (!fd) {
        return;
    }
    char* line = NULL;
    size_t line_cap = 0;
    int line_len;
    struct GapBuffer* buf = Gap_new("");
    while ((line_len = getline(&line, &line_cap, fd)) != -1) {
        Gap_insert_str(buf, line);
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
            line[line_len] = '\0';
        }
        insert_row(ctx, ctx->n_rows, line);
    }
    ctx->gap = buf;
    Gap_mov(buf, -buf->size);
    ctx->dirty = 0;
}
/***** input *****/

enum EditorKey
{
    BACKSPACE = 127,
    UP = 1000,
    DOWN,
    LEFT,
    RIGHT,
    HOME,
    END,
    PG_UP,
    PG_DWN,
    DEL
};

int
char_to_key(char c)
{
    if (c == '\x1b') {
        char seq[3];
        if (read(STDIN_FILENO, seq, 1) != 1)
            return '\x1b';
        if (read(STDIN_FILENO, seq + 1, 1) != 1)
            return '\x1b';
        if (seq[0] == '[') {
            if (seq[1] >= '0' && seq[1] <= '9') {
                if (read(STDIN_FILENO, seq + 2, 1) != 1)
                    return '\x1b';
                if (seq[2] == '~') {
                    switch (seq[1]) {
                        case '1':
                            return HOME;
                        case '3':
                            return DEL;
                        case '4':
                            return END;
                        case '5':
                            return PG_UP;
                        case '6':
                            return PG_DWN;
                        case '7':
                            return HOME;
                        case '8':
                            return END;
                        default:
                            return '\x1b';
                    }
                } else {
                    return '\x1b';
                }
            } else {
                switch (seq[1]) {
                    case 'A':
                        return UP;
                    case 'B':
                        return DOWN;
                    case 'C':
                        return RIGHT;
                    case 'D':
                        return LEFT;
                    case 'H':
                        return HOME;
                    case 'F':
                        return END;
                    default:
                        return '\x1b';
                }
            }
        } else if (seq[0] == 'O') {
            switch (seq[1]) {
                case 'H':
                    return HOME;
                case 'F':
                    return END;
                default:
                    return '\x1b';
            }
        } else {
            return '\x1b';
        }
    } else {
        return c;
    }
}

char
read_input(void)
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            unix_error("read");
        }
    }
    return c;
}

char*
prompt(struct EditorContext* ctx, char* prompt)
{
    size_t bufsize = 128;
    char* buf = Calloc(1, bufsize);
    size_t buflen = 0;
    while (1) {
        set_status(ctx, prompt, buf);
        refresh_ui(ctx);
        int c = read_input();
        if (c == '\x1b') {
            set_status(ctx, "");
            free(buf);
            return NULL;
        } else if (c == '\r') {
            if (buflen != 0) {
                set_status(ctx, "");
                return buf;
            }
        } else if (!iscntrl(c) && c < 128) {
            if (buflen == bufsize - 1) {
                bufsize *= 2;
                buf = Realloc(buf, bufsize);
            }
            buf[buflen++] = c;
            buf[buflen] = '\0';
        }
    }
}

void
handle_cursor_mov(struct EditorContext* ctx, int key)
{
    struct GapBuffer* gap =
      (ctx->cy >= ctx->n_rows) ? NULL : ctx->lines[ctx->cy];
    switch (key) {
        case LEFT:
            Gap_mov(ctx->gap, -1);
            if (gap && ctx->cx > 0) {
                ctx->cx--;
                Gap_mov(gap, -1);
            } else if (ctx->cy > 0) {
                ctx->cy--;
                struct GapBuffer* prev = ctx->lines[ctx->cy];
                ctx->cx = prev->size;
                Gap_mov(prev, prev->size);
            }
            break;
        case RIGHT:
            Gap_mov(ctx->gap, +1);
            if (gap && ctx->cx < gap->size) {
                ctx->cx++;
                Gap_mov(gap, 1);
            } else if (gap && ctx->cx >= gap->size) {
                ctx->cy++;
                if (ctx->cy < ctx->n_rows) {
                    struct GapBuffer* next = ctx->lines[ctx->cy];
                    Gap_mov(next, -next->size);
                }
                ctx->cx = 0;
            }
            break;
        case UP:
            if (ctx->cy > 0) {
                ctx->cy--;
                struct GapBuffer* prev = ctx->lines[ctx->cy];
                Gap_mov(prev, ctx->cx - prev->cur_beg);
            }
            Gap_prevline(ctx->gap);
            break;
        case DOWN:
            if (ctx->cy < ctx->n_rows) {
                ctx->cy++;
                if (ctx->cy < ctx->n_rows) {
                    struct GapBuffer* next = ctx->lines[ctx->cy];
                    Gap_mov(next, ctx->cx - next->cur_beg);
                } else {
                    ctx->cx = 0;
                }
            }
            Gap_nextline(ctx->gap);
            break;
        case HOME:
            ctx->cy = 0;
            struct GapBuffer* first = ctx->lines[ctx->cy];
            Gap_mov(first, ctx->cx - first->cur_beg);
            Gap_mov(ctx->gap, -ctx->gap->size);
            break;
        case END:
            ctx->cy = ctx->n_rows - 1;
            {
                struct GapBuffer* last = ctx->lines[ctx->cy];
                Gap_mov(last, ctx->cx - last->cur_beg);
            }
            Gap_mov(ctx->gap, ctx->gap->size);
            break;
        case PG_UP:
            if (ctx->cy < ctx->screenrows) {
                ctx->cy = 0;
            } else {
                ctx->cy -= ctx->screenrows;
            }
            struct GapBuffer* up = ctx->lines[ctx->cy];
            Gap_mov(up, ctx->cx - up->cur_beg);
            for (ssize_t i = 0; i < ctx->screenrows; i++) {
                Gap_prevline(ctx->gap);
            }
            break;
        case PG_DWN:
            ctx->cy += ctx->screenrows;
            if (ctx->cy > ctx->n_rows) {
                ctx->cy = ctx->n_rows;
            }
            {
                struct GapBuffer* down = ctx->lines[ctx->cy];
                Gap_mov(down, ctx->cx - down->cur_beg);
            }
            for (ssize_t i = 0; i < ctx->screenrows; i++) {
                Gap_nextline(ctx->gap);
            }
            break;
    }
    gap = (ctx->cy >= ctx->n_rows) ? NULL : ctx->lines[ctx->cy];
    unsigned rowlen = gap ? gap->size : 0;
    if (ctx->cx > rowlen) {
        ctx->cx = rowlen;
    }
}

void
enter_char(struct EditorContext* ctx, char c)
{
    if (ctx->cy == ctx->n_rows) {
        insert_row(ctx, ctx->n_rows, "");
    }
    Gap_insert_chr(ctx->lines[ctx->cy], c);
    Gap_insert_chr(ctx->gap, c);
    ctx->cx++;
    ctx->dirty++;
}

void
enter_newline(struct EditorContext* ctx)
{
    struct BumpAlloc scratch = *ctx->bmp;
    if (ctx->cx == 0) {
        insert_row(ctx, ctx->cy, "");
    } else {
        struct GapBuffer* gap = ctx->lines[ctx->cy];
        char* buf = Bump_alloc(&scratch, gap->size - ctx->cx + 1);
        Gap_substr(gap, ctx->cx, gap->size, buf);
        insert_row(ctx, ctx->cy + 1, buf);
        Gap_del(gap, gap->size);
    }
    Gap_insert_chr(ctx->gap, '\n');
    ctx->cy++;
    ctx->cx = 0;
}

void
del_char(struct EditorContext* ctx)
{
    if (ctx->cy == ctx->n_rows) {
        return;
    }
    if (ctx->gap->cur_beg == ctx->gap->size) {
        return;
    }

    struct GapBuffer* curr = ctx->lines[ctx->cy];
    if (ctx->cy == ctx->n_rows - 1 && ctx->cx == curr->size) {
        return;
    } else if (ctx->cx < curr->size) {
        Gap_del(curr, 1);
        Gap_del(ctx->gap, 1);
        ctx->dirty++;
    } else {
        if (ctx->cy > 1) {
            Gap_del(ctx->gap, 1);
        }
        struct BumpAlloc scratch = *ctx->bmp;
        struct GapBuffer* next = ctx->lines[ctx->cy + 1];
        char* buf = Bump_alloc(&scratch, next->size + 1);
        Gap_str(next, buf);
        Gap_insert_str(curr, buf);
        Gap_mov(curr, -strlen(buf));
        del_row(ctx, ctx->cy + 1);
        ctx->dirty++;
    }
}
void
handle_input(struct EditorContext* ctx, char c)
{
    int key = char_to_key(c);
    switch (key) {
        case LEFT:
        case RIGHT:
        case UP:
        case DOWN:
        case PG_DWN:
        case PG_UP:
        case END:
        case HOME:
            handle_cursor_mov(ctx, key);
            break;
        case '\r':
            enter_newline(ctx);
            break;
        case CTRL_KEY('s'):
            save_buf(ctx);
            break;
        case BACKSPACE:
        case CTRL_KEY('h'):
            if (!ctx->cx && !ctx->cy) {
                break;
            }
            handle_cursor_mov(ctx, LEFT);
            // fall through
        case DEL:
            del_char(ctx);
            break;
        case CTRL_KEY('q'):
            exit(0);
            break;
        case CTRL_KEY('l'):
        case '\x1b':
            break;
        default:
            enter_char(ctx, c);
            break;
    }
}
