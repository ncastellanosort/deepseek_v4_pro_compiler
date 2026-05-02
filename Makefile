CC = gcc
CFLAGS = -Wall -Wextra -g -std=c11
SRCDIR = src
BUILDDIR = build
TARGET = $(BUILDDIR)/compilador

SRCS = $(SRCDIR)/main.c $(SRCDIR)/lexer.c $(SRCDIR)/parser.c $(SRCDIR)/codegen.c $(SRCDIR)/ast.c
OBJS = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -o $@ $^

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

test: $(TARGET)
	./$(TARGET) examples/test.mat

clean:
	rm -rf $(BUILDDIR)
