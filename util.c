#include "util.h"
#include <unistd.h>
void
unix_error(const char* msg)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    perror(msg);
    exit(1);
}

void
Tcgetattr(int __fd, struct termios* tios)
{
    if (tcgetattr(__fd, tios) < 0) {
        unix_error("tcgetattr");
    }
}

void
Tcsetattr(int __fd, int optional, struct termios* tios)
{

    if (tcsetattr(__fd, optional, tios) < 0) {
        unix_error("tcsetattr");
    }
}

void*
Malloc(size_t size)
{
    void* ptr = malloc(size);
    if (!ptr) {
        unix_error("malloc");
    }
    return ptr;
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
        unix_error("realloc");
    }
    return new_ptr;
}
FILE*
Fopen(char* file, char* opts)
{
    FILE* fd = fopen(file, opts);
    if (!fd) {
        unix_error("fopen");
    }
    return fd;
}
