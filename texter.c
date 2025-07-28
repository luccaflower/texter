#include <assert.h>
#include <errno.h>
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

struct EditorContext
{
    int cx, cy;
    int rows;
    int cols;
};

/**** libc wrappers *****/
void
unix_error(const char* msg)
{
    write(STDOUT_FILENO, CLEAR_SCREEN, 4);
    write(STDOUT_FILENO, RESET_CURSOR, 3);
    perror(msg);
    exit(1);
}

void
Tcgetattr(int __fd, struct termios* tios)
{
    if (tcgetattr(STDIN_FILENO, tios) < 0) {
        unix_error("tcgetattr");
    }
}

void
Tcsetattr(int __fd, int optional, struct termios* tios)
{

    if (tcsetattr(STDIN_FILENO, optional, tios) < 0) {
        unix_error("tcsetattr");
    }
}

void*
Calloc(size_t count, size_t size)
{
    void* ptr = calloc(count, size);
    if (!ptr) {
        unix_error("calloc");
    }
    return ptr;
}

void*
Realloc(void* ptr, size_t size)
{
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr) {
        unix_error("Realloc");
    }
    return new_ptr;
}
/**** mem *****/
struct Arena
{
    size_t max;
    size_t allocated;
    void* base;
};

struct Arena*
Arena_new(size_t capacity)
{
    struct Arena* arena = Calloc(1, sizeof(struct Arena) + capacity);
    arena->base = (void*)(arena + 1);
    arena->max = capacity;
    arena->allocated = 0;
    return arena;
}

void*
Arena_alloc(struct Arena* arena, size_t size)
{
    size = (size + 7) & ~7; // align to 8 bytes
    assert(arena->allocated + size < arena->max);
    void* ptr = arena->base;
    arena->base += size;
    arena->allocated += size;
    return ptr;
}

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

char
read_key(void)
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

void
handle_input(struct EditorContext* ctx, char c)
{
    switch (c) {
        case 'h':
            ctx->cx--;
            if (ctx->cx < 0)
                ctx->cx = 0;
            break;
        case 'l':
            ctx->cx++;
            if (ctx->cx > ctx->cols)
                ctx->cx = ctx->cols;
            break;
        case 'k':
            ctx->cy--;
            if (ctx->cy < 0)
                ctx->cy = 0;
            break;
        case 'j':
            ctx->cy++;
            if (ctx->cy > ctx->rows)
                ctx->cy = ctx->rows;
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
draw_rows(struct EditorContext* ctx, struct Abuf* ab)
{
    for (unsigned y = 0; y < ctx->rows; y++) {
        if (y == (ctx->rows / 3)) {
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

    if (window_size(&ctx->rows, &ctx->cols) == -1) {
        unix_error("init window");
    }
}

/***** main *****/
int
main(int argc, char* argv[])
{
    struct Arena* arena = Arena_new(4096);
    struct EditorContext* ctx = Arena_alloc(arena, sizeof(*ctx));
    enable_raw_mode();
    init_editor(ctx);
    atexit(disable_raw_mode);

    while (1) {
        refresh_ui(ctx);
        char c = read_key();
        handle_input(ctx, c);
    }
    return EXIT_SUCCESS;
}
