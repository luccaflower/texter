#include "abuf.h"
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

struct GlobalState
{
    struct termios orig_termios;
};

static struct GlobalState G;

struct EdRow
{
    int size;
    int r_size;
    char* buf;
    char* render;
};

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
    struct EdRow* erow;
    char* filename;
    struct Abuf* ab;
};

char*
prompt(struct EditorContext* ctx, char* prompt);

/**** terminal *****/

void
disable_raw_mode(void)
{
    Tcsetattr(STDIN_FILENO, TCSAFLUSH, &G.orig_termios);
}

void
enable_raw_mode(void)
{
    struct termios raw;
    Tcgetattr(STDIN_FILENO, &G.orig_termios);
    cfmakeraw(&raw);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 1;
    Tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

/***** ui ******/
int
window_size(int* rows, int* cols)
{
    const char* lines = getenv("LINES");
    const char* columns = getenv("COLUMNS");
    int advance;
    if (lines && columns) {
        sscanf(lines, "%d%n", rows, &advance);
        sscanf(columns, "%d%n", cols, &advance);
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
update_row(struct EdRow* row)
{
    int tabs = 0;
    for (int i = 0; i < row->size; i++) {
        if (row->buf[i] == '\t') {
            tabs++;
        }
    }
    free(row->render);
    int tab_chars = tabs * TABWIDTH;
    row->render = Malloc(row->size + tab_chars + 1);
    int i = 0;
    for (int j = 0; j < row->size; j++) {
        if (row->buf[j] == '\t') {
            i += snprintf(row->render + i, TABWIDTH + 1, "%*c", TABWIDTH, ' ');
        } else {
            row->render[i] = row->buf[j];
            i++;
        }
    }
    row->r_size = i;
}

void
del_row(struct EditorContext* ctx, int at)
{
    if (at < 0 || at >= ctx->n_rows)
        return;
    free(ctx->erow[at].buf);
    free(ctx->erow[at].render);
    memmove(&ctx->erow[at],
            &ctx->erow[at + 1],
            sizeof(*ctx->erow) * (ctx->n_rows - at - 1));
    ctx->n_rows--;
    ctx->dirty++;
}

void
insert_char_into_row(struct EdRow* row, int at, int c)
{
    if (at < 0 || at > row->size) {
        at = row->size;
    }
    row->buf = realloc(row->buf, row->size + 2);
    memmove(&row->buf[at + 1], &row->buf[at], row->size - at + 1);
    row->size++;
    row->buf[at] = c;
    update_row(row);
}

void
append_string_to_row(struct EdRow* row, char* s, size_t len)
{
    row->buf = realloc(row->buf, row->size + len);
    memcpy(&row->buf[row->size], s, len + 1);
    row->size += len;
    row->buf[row->size] = '\0';
    update_row(row);
}

void
del_char_from_row(struct EdRow* row, int at)
{
    if (at < 0 || at >= row->size) {
        return;
    }
    memmove(&row->buf[at], &row->buf[at + 1], row->size - at);
    row->size--;
    update_row(row);
}

void
insert_row(struct EditorContext* ctx, int at, char* s, size_t len)
{
    if (at < 0 || at > ctx->n_rows) {
        return;
    }
    ctx->erow = realloc(ctx->erow, sizeof(*ctx->erow) * (ctx->n_rows + 1));
    memmove(&ctx->erow[at + 1],
            &ctx->erow[at],
            sizeof(*ctx->erow) * (ctx->n_rows - at));

    ctx->erow[at].size = len;
    ctx->erow[at].buf = Malloc(len + 1);
    memcpy(ctx->erow[at].buf, s, len);
    ctx->erow[at].buf[len] = '\0';

    ctx->erow[at].r_size = 0;
    ctx->erow[at].render = NULL;
    update_row(&ctx->erow[at]);

    ctx->n_rows++;
    ctx->dirty++;
}

int
row_cx_to_rx(struct EdRow* row, int cx)
{
    int rx = 0;
    for (int i = 0; i < cx; i++) {
        if (row->buf[i] == '\t') {
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
        ctx->rx = row_cx_to_rx(&ctx->erow[ctx->cy], ctx->cx);
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
        int filerow = y + ctx->row_offset;
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
            int len = ctx->erow[filerow].r_size - ctx->col_offset;
            if (len < 0) {
                len = 0;
            }
            if (len > ctx->screencols) {
                len = ctx->screencols;
            }
            Abuf_append(ab, &ctx->erow[filerow].render[ctx->col_offset], len);
        }
        Abuf_append(ab, ERASE_LINE, sizeof(ERASE_LINE));
        Abuf_append(ab, "\r\n", 2);
    }
}

void
place_cursor(struct EditorContext* ctx, struct Abuf* ab)
{
    char buf[32];
    snprintf(buf,
             sizeof(buf),
             "\x1b[%d;%dH",
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
    int len = snprintf(status,
                       sizeof(status),
                       "%.20s - %d lines %s",
                       filename,
                       ctx->n_rows,
                       ctx->dirty ? "(modified)" : "");
    int rlen =
      snprintf(rstatus, sizeof(rstatus), "%d/%d", ctx->cy + 1, ctx->n_rows);
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
    int msglen = strlen(ctx->status_msg);
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
    ctx->erow = NULL;
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

char*
editor_buf_to_str(struct EditorContext* ctx, int* len)
{
    int totlen = 0;
    for (int i = 0; i < ctx->n_rows; i++) {
        totlen += ctx->erow[i].size + 1;
    }
    *len = totlen;
    char* buf = Malloc(totlen);
    char* p = buf;
    for (int i = 0; i < ctx->n_rows; i++) {
        memcpy(p, ctx->erow[i].buf, ctx->erow[i].size);
        p += ctx->erow[i].size;
        *p = '\n';
        p++;
    }
    return buf;
}

void
save_buf(struct EditorContext* ctx)
{
    if (!ctx->filename) {
        ctx->filename = prompt(ctx, "Save as: %s");
    }

    int len;
    char* buf = editor_buf_to_str(ctx, &len);
    int fd = open(ctx->filename, (O_RDWR | O_CREAT), 0644);
    if (fd != -1) {
        if (ftruncate(fd, len) != -1) {
            if (write(fd, buf, len) == len) {
                set_status(ctx, "%d bytes written to disk", len);
                ctx->dirty = 0;
            } else {
                set_status(ctx,
                           "failed to write some or all of buffer: %s",
                           strerror(errno));
            }
            goto close_file;
        } else {
            goto close_file;
        }
    } else {
        goto free_buffer;
    }

close_file:
    close(fd);
free_buffer:
    free(buf);
}

void
file_open(struct EditorContext* ctx, struct BumpAlloc* arena, char* filename)
{
    FILE* fd = fopen(filename, "r");
    if (!fd) {
        return;
    }
    char* line = NULL;
    size_t line_cap = 0;
    size_t line_len;
    while ((line_len = getline(&line, &line_cap, fd)) != -1) {
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
        }
        insert_row(ctx, ctx->n_rows, line, line_len);
    }
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
    struct EdRow* row = (ctx->cy >= ctx->n_rows) ? NULL : &ctx->erow[ctx->cy];
    switch (key) {
        case LEFT:
            if (ctx->cx > 0) {
                ctx->cx--;
            } else if (ctx->cy > 0) {
                ctx->cy--;
                ctx->cx = ctx->erow[ctx->cy].size;
            }
            break;
        case RIGHT:
            if (row && ctx->cx < row->size) {
                ctx->cx++;
            } else if (row && ctx->cx >= row->size) {
                ctx->cy++;
                ctx->cx = 0;
            }
            break;
        case UP:
            if (ctx->cy > 0) {
                ctx->cy--;
            }
            break;
        case DOWN:
            if (ctx->cy < ctx->n_rows) {
                ctx->cy++;
            }
            break;
        case HOME:
            ctx->cy = 0;
            break;
        case END:
            ctx->cy = ctx->n_rows - 1;
            break;
        case PG_UP:
            ctx->cy -= ctx->screenrows;
            if (ctx->cy < 0) {
                ctx->cy = 0;
            }
            break;
        case PG_DWN:
            ctx->cy += ctx->screenrows;
            if (ctx->cy > ctx->n_rows) {
                ctx->cy = ctx->n_rows;
            }
            break;
    }
    row = (ctx->cy >= ctx->n_rows) ? NULL : &ctx->erow[ctx->cy];
    int rowlen = row ? row->size : 0;
    if (ctx->cx > rowlen) {
        ctx->cx = rowlen;
    }
}

void
enter_char(struct EditorContext* ctx, char c)
{
    if (ctx->cy == ctx->n_rows) {
        insert_row(ctx, ctx->n_rows, "", 0);
    }
    insert_char_into_row(&ctx->erow[ctx->cy], ctx->cx, c);
    ctx->cx++;
    ctx->dirty++;
}

void
enter_newline(struct EditorContext* ctx)
{
    if (ctx->cx == 0) {
        insert_row(ctx, ctx->cy, "", 0);
    } else {
        struct EdRow* row = &ctx->erow[ctx->cy];
        insert_row(ctx, ctx->cy + 1, &row->buf[ctx->cx], row->size - ctx->cx);
        row = &ctx->erow[ctx->cy];
        row->size = ctx->cx;
        row->buf[row->size] = '\0';
        update_row(row);
    }
    ctx->cy++;
    ctx->cx = 0;
}

void
del_char(struct EditorContext* ctx)
{
    if (ctx->cy == ctx->n_rows) {
        return;
    }
    if (ctx->cx == 0 && ctx->cy == 0) {
        return;
    }

    struct EdRow* row = &ctx->erow[ctx->cy];
    if (ctx->cx > 0) {
        del_char_from_row(row, ctx->cx - 1);
        ctx->cx--;
        ctx->dirty++;
    } else {
        ctx->cx = ctx->erow[ctx->cy - 1].size;
        append_string_to_row(&ctx->erow[ctx->cy - 1], row->buf, row->size);
        del_row(ctx, ctx->cy);
        ctx->cy--;
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
        case DEL:
            handle_cursor_mov(ctx, RIGHT);
            // fall through
        case BACKSPACE:
        case CTRL_KEY('h'):
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

/***** main *****/
int
main(int argc, char* argv[])
{
    struct BumpAlloc* bmp = Bump_new(MEGABYTES((size_t)2));
    struct EditorContext* ctx = Bump_alloc(bmp, sizeof(*ctx));
    enable_raw_mode();
    atexit(disable_raw_mode);
    // argv is a NULL-terminated array, so this is fine
    init_editor(ctx, argv[1], bmp);
    if (argc > 1) {
        file_open(ctx, bmp, argv[1]);
    }
    set_status(ctx, "HELP: Ctrl-S to save | Ctrl-Q to quit");

    while (1) {
        refresh_ui(ctx);
        char c = read_input();
        handle_input(ctx, c);
    }
    return EXIT_SUCCESS;
}
