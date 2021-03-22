/** @file
 * Structures and functions related to queries.
 */

#ifndef QUERY_H
#define QUERY_H

#include "stack.h"
#include "dataset.h"
#include <stdio.h>
#include <stdlib.h>


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
    OP_ADD = -1, /*  +  */
    OP_SUB = -2, /*  -  */
    OP_MUL = -3, /*  *  */
    OP_DIV = -4, /*  /  */
    OP_MOD = -5, /*  %  */
    OP_LPR = -6, /*  (  */
    OP_RPR = -7, /*  )  */
    OP_POW = -8, /*  ^  */
    OP_COUNT = 9 /* the total number of operator types */
};

/** Numerical representation of operator associativity. */
enum OpAssoc
{
    OP_ASSOC_ANY,
    OP_ASSOC_LEFT,
    OP_ASSOC_RIGHT,
    OP_ASSOC_NA      /* non-applicable (for parentheses) */
};


/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

/** @cond */
typedef struct Query Query;
typedef enum OpAssoc OpAssoc;
/** @endcond */


/********************************************************
 *                     VARIABLES                        *
 ********************************************************/

/** Stores associativity rules for each operator type.
 *
 * The array is indexed by negating an OpCode, i.e.
 * the associativity of addition is stored in @c opAssoc[-OP_ADD]
 * and so on. See @ref OpCode for a full list.
 */
extern const OpAssoc opAssoc[OP_COUNT];

/** Stores precedence of each operator type.
 *
 * The values are encoded as ints, greater values mean
 * higher precedence. The array is indexed by negating an
 * OpCode, i.e. the precedence of multiplication is stored
 * in @c opPrec[-OP_MUL].
 */
extern const int opPrec[OP_COUNT];


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
 * @param[out] query_ptr Address of the query.
 * @param[in] str Input string to be parsed.
 *
 * @returns
 * - 0 - success
 * - 1 - memory error (malloc)
 * - 2 - internal error (@p query_ptr is @c NULL)
 * - 3 - @p str has invalid format
 */
int parseQueryString(Query **query_ptr, const char *str);

/** Parses a query string into integer tokens.
 *
 * This function passes through @p str, accumulates all
 * distinct section/key pairs into a DataSet, and in
 * parallel builds an array of integers, where each integer
 * is a special token. The resulting array of tokens is still
 * in infix notation.
 *
 * Each token is either a non-negative value which is an
 * index to a DataSet pair, or a negative value denoting
 * an operator (for all negative values see @ref OpCode).
 *
 * @param[out] tokens_ptr Address of the tokens stack.
 * @param[out] set_ptr Address of the dataset.
 * @param[in] str Input string to be parsed.
 *
 * @returns
 * - size of @p tokens_ptr - success
 * - -1 - memory error (malloc)
 * - -2 - invalid query
 * - -3 - internal error
 */
int tokenizeQueryString(Stack **tokens_ptr, DataSet **set_ptr, const char *str);

/** Converts an infix stack into postfix stack.
 *
 * Input format:
 * A stack of integers. Each integer is treated as operand
 * if >=0, and as an operator if <0. Information about operator
 * precedence/associativity is fetched from global variables
 * @ref opPrec and @ref opAsoc.
 *
 * This function does not do any extensive safety checking, it expects
 * a valid infix input.
 *
 * @param[out] postfix_ptr Address of the stack.
 * @param[in] infix Array of tokens in infix order.
 *
 * @returns
 * - 0 - success
 * - 1 - internal error
 */
int infixPostfix(Stack **postfix_ptr, const Stack *infix);

/** Frees all memory owned by a query. */
void queryFree(Query *query);

/** Runs a list of queries on a single INI file.
 *
 * This function is the core of the iniget program,
 * it does a single pass-through on an input stream
 * and evaluates all queries you feed it. At the end,
 * it will print the result of each query in order,
 * one query per line. If any query fails to be evaluated,
 * they all fail and nothing gets printed on stdout
 * (you'll still get an appropriate stderr message).
 *
 * @param[inout] file The file to run the queries on.
 * @param[in] queries An ordered list of queries to run.
 * @param[in] qcount The number of elements in @p queries.
 *
 * @returns
 * - 0 - success
 */
int runQueries(FILE *file, const Query **queries, size_t qcount);

#endif /* QUERY_H */
