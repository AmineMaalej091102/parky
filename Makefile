CC := gcc
CFLAGS := -Wall -Wextra
TARGET := parky
SOURCES := $(wildcard src/*.c)
GTK_CFLAGS := $(shell pkg-config --cflags gtk+-2.0)
GTK_LIBS := $(shell pkg-config --libs gtk+-2.0)

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -Iinclude $(GTK_CFLAGS) $(SOURCES) $(GTK_LIBS) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).o src/*.o
