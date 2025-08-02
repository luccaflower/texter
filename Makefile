CFLAGS=-O0 -g -Wall -Werror
PROG = texter
OBJ = texter.o \
	  util.o \
	  mem.o \
	  abuf.o

$(PROG): $(OBJ)

.PHONY: clean
clean:
	rm -rf texter
