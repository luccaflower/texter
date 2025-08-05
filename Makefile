CFLAGS= -O0 -g -Wall -Werror
TEST = test
PROG = texter
OBJ =  	  util.o \
	  mem.o \
	  abuf.o \
	  gap.o

$(PROG): $(PROG).o $(OBJ)

$(TEST): LDLIBS += -lcheck
$(TEST): $(TEST).o $(OBJ)

.PHONY: clean check
check: $(TEST) 
	./test
clean:
	rm -rf *.o texter test
