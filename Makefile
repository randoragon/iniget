CC = cc
CFLAGS = -std=c89 -pedantic -Wall
LDFLAGS =
OBJS = stack.o iniget.o
OUT = iniget
PREFIX = /usr/local

all: main

main: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(OUT)

%.o: %.c
	$(CC) -c $(CFLAGS) $^ -o $@

clean:
	rm -f *.o

debug: CFLAGS += -g -Og
debug: clean all

install: CFLAGS += -O3
install: clean all
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f -- $(OUT) ${DESTDIR}${PREFIX}/bin
	@chmod 755 -- ${DESTDIR}${PREFIX}/bin/$(OUT)
