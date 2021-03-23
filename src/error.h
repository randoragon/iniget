/** @file
 * Functions for communicating with the user/developer.
 */

#ifndef ERROR_H
#define ERROR_H

#include <stdio.h>

/** Prints a diagnostic message about the current
 * location in code (file, LOC). The intended use
 * is right before the @ref error function.
 */
#define STAMP() \
    do { \
        fprintf(stderr, "(%s:%d) ", __FILE__, __LINE__); \
    } while (0)

/** Print informational message for the user.
 *
 * This function is used for normal communication with
 * the user in case something goes wrong that is
 * independent of the program itself (such as memory
 * allocation failure, invalid user input, etc.).
 */
void info(const char *fmt, ...);

/** Print error message for the developer.
 *
 * This function is used for diagnostic messages about
 * internal runtime errors that are caused by erroneous
 * operation of the program. These should be debug-only
 * and the user of the finished product should never have
 * to see these.
 */
void error(const char *fmt, ...);

#endif /* ERROR_H */
