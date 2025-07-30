#include "mem.h"
#include "util.h"
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define TEXTER_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)
#define CLEAR_SCREEN ("\x1b[2J")
#define RESET_CURSOR ("\x1b[H")
#define BLINK_CURSOR ("\x1b[?25h")
#define ERASE_LINE ("\x1b[K")

struct GlobalState
{
    struct termios orig_termios;
};

static struct GlobalState G;

struct EdRow
{
    int size;
    char* buf;
};

struct EditorContext
{
    int cx, cy;
    int rows;
    int cols;
    int n_rows;
    struct EdRow* erow;
};

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

/***** append buffer *****/
struct Abuf
{
    char* b;
    size_t len;
};

void
Abuf_append(struct Abuf* ab, const char* s, size_t len)
{
    char* new = Realloc(ab->b, ab->len + len);
    memcpy(&new[ab->len], s, len);
    ab->b = new;
    ab->len += len;
}

void
Abuf_free(struct Abuf* ab)
{
    free(ab->b);
}

/***** input *****/

enum EditorKey
{
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
read_key(void)
{
    int nread;
    char c;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) {
            unix_error("read");
        }
    }
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

void
handle_cursor_mov(struct EditorContext* ctx, int key)
{
    switch (key) {
        case LEFT:
            ctx->cx--;
            break;
        case RIGHT:
            ctx->cx++;
            break;
        case UP:
            ctx->cy--;
            break;
        case DOWN:
            ctx->cy++;
            break;
        case HOME:
            ctx->cy = 0;
            break;
        case END:
            ctx->cy = ctx->rows - 1;
            break;
        case PG_UP:
            ctx->cy -= ctx->rows;
            break;
        case PG_DWN:
            ctx->cy += ctx->rows;
            break;
            if (ctx->cx < 0)
                ctx->cx = 0;
            else if (ctx->cx > ctx->cols)
                ctx->cx = ctx->cols;
            else if (ctx->cy < 0)
                ctx->cy = 0;
            else if (ctx->cy >= ctx->rows)
                ctx->cy = ctx->rows - 1;
    }
}

void
handle_input(struct EditorContext* ctx, int key)
{
    switch (key) {
        case LEFT:
        case RIGHT:
        case UP:
        case DOWN:
        case PG_DWN:
        case PG_UP:
        case END:
        case HOME:
        case DEL:
            handle_cursor_mov(ctx, key);
            break;
        case CTRL_KEY('q'):
            exit(0);
    }
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
append_row(struct EditorContext* ctx, char* s, size_t len)
{
    ctx->erow = realloc(ctx->erow, sizeof(*ctx->erow) * (ctx->n_rows + 1));
    int at = ctx->n_rows;
    ctx->erow[at].size = len;
    ctx->erow[at].buf = Malloc(len + 1);
    memcpy(ctx->erow[at].buf, s, len);
    ctx->erow[at].buf[len] = '\0';
    ctx->n_rows++;
}
void
draw_rows(struct EditorContext* ctx, struct Abuf* ab)
{
    for (unsigned y = 0; y < ctx->rows; y++) {
        if (y >= ctx->n_rows) {
            if (ctx->n_rows == 0 && y == (ctx->rows / 3)) {
                const char welcome[] =
                  "Tutorial text-editor -- version " TEXTER_VERSION;
                size_t welcome_len = sizeof(welcome);
                int padding = (ctx->cols - welcome_len) / 2;
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
            int len = ctx->erow[y].size;
            if (len > ctx->cols) {
                len = ctx->cols;
            }
            Abuf_append(ab, ctx->erow[y].buf, len);
        }
        Abuf_append(ab, ERASE_LINE, sizeof(ERASE_LINE));
        if (y < ctx->rows - 1) {
            Abuf_append(ab, "\r\n", 2);
        }
    }
}

void
place_cursor(struct EditorContext* ctx, struct Abuf* ab)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", ctx->cy + 1, ctx->cx + 1);
    Abuf_append(ab, buf, strlen(buf));
}

void
refresh_ui(struct EditorContext* ctx)
{
    struct Abuf ab = { .b = NULL, .len = 0 };
    Abuf_append(&ab, RESET_CURSOR, sizeof(RESET_CURSOR));
    draw_rows(ctx, &ab);
    place_cursor(ctx, &ab);
    Abuf_append(&ab, BLINK_CURSOR, sizeof(BLINK_CURSOR));
    write(STDOUT_FILENO, ab.b, ab.len);
    Abuf_free(&ab);
}

void
init_editor(struct EditorContext* ctx)
{
    ctx->cx = 0;
    ctx->cy = 0;
    ctx->n_rows = 0;
    ctx->erow = NULL;
    if (window_size(&ctx->rows, &ctx->cols) == -1) {
        unix_error("init window");
    }
}

/***** file i/o *****/
void
file_open(struct EditorContext* ctx, struct BumpAlloc* arena, char* filename)
{
    FILE* fd = Fopen(filename, "r");
    char* line = NULL;
    size_t line_cap = 0;
    size_t line_len;
    while ((line_len = getline(&line, &line_cap, fd)) != -1) {
        while (line_len > 0 &&
               (line[line_len - 1] == '\n' || line[line_len - 1] == '\r')) {
            line_len--;
        }
        append_row(ctx, line, line_len);
    }
}

/***** main *****/
int
main(int argc, char* argv[])
{
    struct BumpAlloc* arena = Bump_new(MEGABYTES((size_t)2));
    struct EditorContext* ctx = Bump_alloc(arena, sizeof(*ctx));
    enable_raw_mode();
    init_editor(ctx);
    if (argc > 1) {
        file_open(ctx, arena, argv[1]);
    }
    atexit(disable_raw_mode);

    while (1) {
        refresh_ui(ctx);
        int key = read_key();
        handle_input(ctx, key);
    }
    return EXIT_SUCCESS;
}
