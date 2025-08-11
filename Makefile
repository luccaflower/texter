CFLAGS= -O0 -g -Wall -Wextra -Werror
TEST = test
PROG = main
OBJ =  	  texter.o \
	  util.o \
	  mem.o \
	  abuf.o \
	  gap.o

texter: $(PROG).o $(OBJ)

$(TEST): LDLIBS += -lcheck
$(TEST): $(TEST).o $(OBJ)

.PHONY: clean check
check: $(TEST) 
	./test
clean:
	rm -rf *.o texter test
