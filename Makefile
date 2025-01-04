CC = gcc
CFLAGS = -Iinclude
SRCDIR = src
OBJDIR = obj
BINDIR = bin
TARGET = xboxcontrollerlib

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJ)
    @mkdir -p $(BINDIR)
    $(CC) -o $@ $^

$(OBJDIR)/%.o: $(SRCDIR)/%.c
    @mkdir -p $(OBJDIR)
    $(CC) $(CFLAGS) -c -o $@ $<

install: $(BINDIR)/$(TARGET)
    install -m 0755 $(BINDIR)/$(TARGET) /usr/local/bin/
    install -m 0644 include/xboxcontrollerlib.h /usr/local/include/

clean:
    rm -rf $(OBJDIR) $(BINDIR)

.PHONY: all clean install