# Compiler and linker options
CC = cc
LD = cc
CFLAGS = -std=c89 -pedantic -Wall -Wextra
LDFLAGS = -lm


# Directories and files configuration
SRCDIR = src
OBJDIR = obj
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.c=.o))
TARGET = iniget
DESTDIR =
PREFIX = /usr/local
MANPREFIX = $(PREFIX)/share/man

.PHONY: all dirs main clean debug install

all: dirs main

dirs:
	mkdir -p -- $(SRCDIR) $(OBJDIR)

main: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(TARGET)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	rm -f -- $(OBJS)

debug: CFLAGS += -g -Og
debug: clean all

install: CFLAGS += -O3
install: clean all
	@mkdir -p -- $(DESTDIR)$(PREFIX)/bin
	cp -f -- $(TARGET) $(DESTDIR)$(PREFIX)/bin
	@chmod 755 -- $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	@mkdir -p -- $(DESTDIR)$(MANPREFIX)/man1
	cp -- iniget.1 $(DESTDIR)$(MANPREFIX)/man1/iniget.1
	@chmod 644 $(DESTDIR)$(MANPREFIX)/man1/iniget.1
