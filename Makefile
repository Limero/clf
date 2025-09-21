CC = cc
CFLAGS = -std=c99 -Wall -Werror -Wextra -pedantic -Wno-unused-parameter -g -O0
DEFINES = -D_XOPEN_SOURCE -D_DEFAULT_SOURCE

TARGET = clf
SOURCES = main.c

.PHONY: all clean install debug release test

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(DEFINES) -o $(TARGET) $(SOURCES)

clean:
	@rm -f $(TARGET)

install: $(TARGET)
	cp $(TARGET) ~/.local/bin/

debug: CFLAGS += -DDEBUG -O0 -fsanitize=address,undefined
debug: $(TARGET)

release: CFLAGS = -std=c99 -Wall -Werror -Wextra -pedantic -Wno-unused-parameter -O3 -flto -DNDEBUG
release: $(TARGET)

test: test-all.c
	$(CC) $(CFLAGS) -std=c11 -Wno-unused-variable $(DEFINES) -o $(TARGET)-test test-all.c && ./$(TARGET)-test

help:
	@echo "Available targets:"
	@echo "  all         - Build the file manager (default)"
	@echo "  clean       - Remove built files"
	@echo "  install     - Install to ~/.local/bin"
	@echo "  debug       - Build with debug symbols and address sanitizer"
	@echo "  release     - Build optimized release version"
	@echo "  help        - Show this help message"
