/** @file
 * Structures and functions related to queries.
 */

#ifndef QUERY_H
#define QUERY_H

#include "stack.h"
#include "dataset.h"
#include <stdlib.h>
#include <limits.h>

/********************************************************
 *                     CONSTANTS                        *
 ********************************************************/

/** Encodes an arithmetic operation as an integer.
 *
 * These values are used to denote operations in
 * @ref Query::op_stack.
 */
enum OpCode
{
    OP_ADD = -1,
    OP_SUB = -2,
    OP_MUL = -3,
    OP_DIV = -4,
    OP_MOD = -5
};

/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

/** @cond */
typedef struct Query Query;
/** @endcond */


/********************************************************
 *                     STRUCTURES                       *
 ********************************************************/

/** Holds a single query to be executed on a file.
 *
 * The raw query string input by the user is first
 * preprocessed, and the more concise form is stored
 * inside this struct.
 */
struct Query
{
    /** An ordered set of all section/value pairs relevant to the query. */
    DataSet *data;

    /** Operation stack in postfix format to remove all ambiguity.
     *
     * The stack consists of two types of numbers:
     * - indices to @ref data, which represent values
     * - operators (negative values by convention, see enum OpCode)
     */
    Stack *op_stack;
};

/********************************************************
 *                     FUNCTIONS                        *
 ********************************************************/

/** Creates a new @ref Query structure out of a user-input query string.
 *
 * Once an output query is obtained through @p query_ptr, it must be
 * manually freed with @ref queryFree.
 *
 * @param query_ptr Address of the query.
 * @param str Input string to be parsed.
 *
 * @returns
 * - 0 - success
 * - 1 - memory error (malloc)
 * - 2 - internal error (@p query_ptr is @c NULL)
 * - 3 - @p str has invalid format
 */
int parseQueryString(Query **query_ptr, const char *str);

/** Validates a query string and returns the number of distinct values present within.
 *
 * If validation fails, an appropriate message is printed for
 * the user and the return value is <0. Note that a valid query
 * does necessarily not have to contain any value. For example,
 * "2 * 4" is a completely valid query which would simply produce 8,
 * even though in the context of INI files it's pretty useless.
 *
 * @returns
 * - @c >=0 - success
 * - @c <0 - failure
 */
int validateQueryString(const char *str);

/** Frees all memory owned by a query. */
void queryFree(Query *query);

#endif /* QUERY_H */
