#include "error.h"
#include <stdio.h>
#include <stdarg.h>

void info(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    fputs("iniget: ", stderr);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);

    va_end(ap);
}


void error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    fputs("[ERROR] ", stderr);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);

    va_end(ap);
}
