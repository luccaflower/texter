#ifndef LIBCW
#define LIBCW

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
void
unix_error(const char* msg);
void
Tcgetattr(int __fd, struct termios* tios);
void
Tcsetattr(int __fd, int optional, struct termios* tios);
void*
Malloc(size_t size);
void*
Calloc(size_t count, size_t size);
void*
Realloc(void* ptr, size_t size);
FILE*
Fopen(char* file, char* opts);
#endif // !LIBCW
