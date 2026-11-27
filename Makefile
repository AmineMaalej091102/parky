CC := gcc
CFLAGS := -std=c99 -Wall -Wextra
TARGET := parky
SOURCES := $(wildcard src/*.c)
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK_LIBS = $(shell pkg-config --libs gtk+-2.0)
TEST_TARGET := tests/test_suite
TEST_SOURCES := tests/test_suite.c src/storage.c src/admin.c src/agent.c src/citoyen.c
BENCH_TARGET := tests/benchmark
BENCH_SOURCES := tests/benchmark.c src/storage.c

.PHONY: all run test bench clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -Iinclude $(GTK_CFLAGS) $(SOURCES) $(GTK_LIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

$(TEST_TARGET): $(TEST_SOURCES)
	$(CC) $(CFLAGS) -Iinclude $(TEST_SOURCES) -o $(TEST_TARGET)

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(BENCH_TARGET): $(BENCH_SOURCES)
	$(CC) $(CFLAGS) -Iinclude $(BENCH_SOURCES) -o $(BENCH_TARGET)

bench: $(BENCH_TARGET)
	./$(BENCH_TARGET)

clean:
	rm -f $(TARGET) $(TARGET).o src/*.o $(TEST_TARGET) $(BENCH_TARGET)
