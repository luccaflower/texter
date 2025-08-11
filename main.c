
#include "gap.h"
#include "mem.h"
#include "texter.h"
#include "util.h"
#include <unistd.h>

struct GlobalState
{
    struct termios orig_termios;
};

static struct GlobalState G;

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
        file_open(ctx, argv[1]);
    } else {
        ctx->gap = Gap_new("");
    }
    set_status(ctx, "HELP: Ctrl-S to save | Ctrl-Q to quit");

    while (1) {
        refresh_ui(ctx);
        char c = read_input();
        handle_input(ctx, c);
    }
    return EXIT_SUCCESS;
}
