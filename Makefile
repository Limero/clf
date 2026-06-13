CC = cc
CFLAGS = -std=c99 -Wall -Werror -Wextra -pedantic -Wno-unused-parameter -g -O0
DEFINES = -D_XOPEN_SOURCE -D_DEFAULT_SOURCE

TARGET = clf
SOURCES = main.c
ALL_SRCS = $(wildcard *.c)
HEADERS = $(wildcard *.h) $(wildcard include/*.h)

.PHONY: all clean install debug release test lint

all: $(TARGET)

$(TARGET): $(SOURCES) $(ALL_SRCS) $(HEADERS)
	$(CC) $(CFLAGS) $(DEFINES) -o $(TARGET) $(SOURCES)

clean:
	@rm -f $(TARGET) $(TARGET)-test

install: $(TARGET)
	cp $(TARGET) ~/.local/bin/

debug: CFLAGS += -DDEBUG -O0 -fsanitize=address,undefined
debug: $(TARGET)

release: CFLAGS = -std=c99 -Wall -Werror -Wextra -pedantic -Wno-unused-parameter -O3 -flto -DNDEBUG
release: $(TARGET)

test: test-all.c
	$(CC) $(CFLAGS) -Wno-unused-variable -Wno-unused-function $(DEFINES) -o $(TARGET)-test test-all.c && time ./$(TARGET)-test

lint:
	clang-format -i $(ALL_SRCS) $(HEADERS)

help:
	@echo "Available targets:"
	@echo "  all         - Build the file manager (default)"
	@echo "  clean       - Remove built files"
	@echo "  install     - Install to ~/.local/bin"
	@echo "  debug       - Build with debug symbols and address sanitizer"
	@echo "  release     - Build optimized release version"
	@echo "  test        - Run all tests
	@echo "  lint        - Format all source files with clang-format
	@echo "  help        - Show this help message"
