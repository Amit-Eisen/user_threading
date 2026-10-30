CC      := gcc
CFLAGS  := -Wall -Wextra -g -mno-red-zone -Isrc
BUILD   := build

.PHONY: all demo test clean

all: demo

demo: $(BUILD)/demo

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/thread.o: src/thread.c src/thread.h | $(BUILD)
	$(CC) $(CFLAGS) -c src/thread.c -o $@

$(BUILD)/demo: examples/main.c $(BUILD)/thread.o src/thread.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ examples/main.c $(BUILD)/thread.o

$(BUILD)/test_%: tests/test_%.c $(BUILD)/thread.o src/thread.h tests/check.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(BUILD)/thread.o

TEST_BINS := $(BUILD)/test_errors $(BUILD)/test_capacity $(BUILD)/test_block $(BUILD)/test_sleep $(BUILD)/test_exit

test: $(TEST_BINS)
	@set -e; for t in $(TEST_BINS); do echo "== $$t =="; ./$$t; done

clean:
	rm -rf $(BUILD)
