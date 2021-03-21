/** @file
 * Functions for communicating with the user/developer.
 */

#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>
#include <stdarg.h>

/** This function is used for normal communication with
 * the user in case something goes wrong that is
 * independent of the program itself (such as memory
 * allocation failure, invalid user input, etc.).
 */
static void info(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    fputs("iniget: ", stderr);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);

    va_end(ap);
}


/** This function is used for diagnostic messages about
 * internal runtime errors that are caused by erroneous
 * operation of the program. These should be debug-only
 * and the user of the finished product should never have
 * to see these.
 */
static void error(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);

    fprintf(stderr, "[ERROR] (%s:%d) ", __FILE__, __LINE__);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);

    va_end(ap);
}

#endif
