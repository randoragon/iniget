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
PREFIX = /usr/local/bin

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
	@mkdir -p -- ${DESTDIR}${PREFIX}/
	cp -f -- $(TARGET) ${DESTDIR}${PREFIX}/
	@chmod 755 -- ${DESTDIR}${PREFIX}/$(TARGET)
