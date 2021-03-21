/** @file
 * Functions for communicating with the user/developer.
 */

#ifndef ERROR_H
#define ERROR_H

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

#endif
