CC ?= cc
CFLAGS ?= -O3 -DNDEBUG -std=c11 -Wall -Wextra -Wpedantic -Wshadow -Wconversion -pthread
CPPFLAGS ?= -Iinclude -Ithird_party/xxhash -D_POSIX_C_SOURCE=200809L
LDFLAGS ?= -pthread
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DOCDIR ?= $(PREFIX)/share/doc/verdiff
BUILD_DIR ?= build
OBJDIR := $(BUILD_DIR)/obj
BIN_DIR := $(BUILD_DIR)/bin
INSTALL ?= install

TARGET := verdiff
SRC := \
	src/arena.c \
	src/progress.c \
	src/path_tree.c \
	src/main.c \
	src/index.c \
	src/scanner.c \
	src/planner.c \
	src/thread_pool.c \
	src/hash.c \
	src/compare.c \
	src/output.c \
	third_party/xxhash/xxhash.c
OBJ := $(patsubst %.c,$(OBJDIR)/%.o,$(SRC))
TARGET_PATH := $(BIN_DIR)/$(TARGET)

.PHONY: all clean test bench bench-compare install uninstall

all: $(TARGET_PATH)

$(TARGET_PATH): $(OBJ)
	mkdir -p $(BIN_DIR)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET_PATH)
	sh tests/smoke.sh

bench: $(TARGET_PATH)
	sh bench/run_benchmark.sh

bench-compare: $(TARGET_PATH)
	sh bench/compare_tools.sh

clean:
	rm -rf $(BUILD_DIR)

install: $(TARGET_PATH)
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -d $(DESTDIR)$(DOCDIR)
	$(INSTALL) -m 755 $(TARGET_PATH) $(DESTDIR)$(BINDIR)/$(TARGET)
	$(INSTALL) -m 644 README.md $(DESTDIR)$(DOCDIR)/README.md
	$(INSTALL) -m 644 LICENSE $(DESTDIR)$(DOCDIR)/LICENSE
	$(INSTALL) -m 644 third_party/xxhash/LICENSE $(DESTDIR)$(DOCDIR)/THIRD_PARTY_XXHASH_LICENSE

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/$(TARGET)
	rm -rf $(DESTDIR)$(DOCDIR)
