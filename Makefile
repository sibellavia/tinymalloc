# compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -O0 -fprofile-arcs -ftest-coverage
LDFLAGS = -pthread -fprofile-arcs -ftest-coverage

# source files and object files
SRC = tinymalloc.c
OBJ = $(SRC:.c=.o)
TEST_SRC = test_tinymalloc.c
TEST_OBJ = $(TEST_SRC:.c=.o)

# targets
all: libtinymalloc.a test_tinymalloc

libtinymalloc.a: $(OBJ)
	ar rcs $@ $^

test_tinymalloc: $(TEST_OBJ) libtinymalloc.a
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) -L. -ltinymalloc $(LDFLAGS)

# pattern rule for object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# test target
test: test_tinymalloc
	./test_tinymalloc

# coverage target
coverage: test
	gcov $(SRC)
	lcov --capture --directory . --output-file coverage.info
	genhtml coverage.info --output-directory out
	@echo "Coverage report generated in 'out' directory"

# clean target
clean:
	rm -f *.o *.a test_tinymalloc *.gcno *.gcda *.gcov coverage.info
	rm -rf out

.PHONY: all test clean coverage
