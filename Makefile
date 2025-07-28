CFLAGS=-O0 -g -Wall -Werror

texter: texter.c

.PHONY: clean
clean:
	rm -rf texter
