/** @file
 * Structures and functions related to queries.
 */

#ifndef QUERY_H
#define QUERY_H

#include "stack.h"
#include "dataset.h"
#include "arglist.h"
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
    /** Addition '+' */
    OP_ADD = -1,
    /** Subtraction '-' */
    OP_SUB = -2,
    /** Multiplication '*' */
    OP_MUL = -3,
    /** Division '/' */
    OP_DIV = -4,
    /** Modulus '%%' */
    OP_MOD = -5,
    /** Left parenthesis '(' */
    OP_LPR = -6,
    /** Right parenthesis ')' */
    OP_RPR = -7,
    /** Exponentiation '^' */
    OP_POW = -8,
    /** This is not an actual OpCode, it is used as a constant
     *  for determining the array size required to fit all
     *  operator types. */
    OP_COUNT = 9
};

/** Numerical representation of operator associativity. */
enum OpAssoc
{
    /** Fully associative operators (like addition, multiplication). */
    OP_ASSOC_ANY,
    /** Left-associative operators (like division). */
    OP_ASSOC_LEFT,
    /** Right-associative operators (like exponentiation). */
    OP_ASSOC_RIGHT,
    /** Non-applicable (for parentheses). */
    OP_ASSOC_NA
};

/** Every line in an INI file should fall under one of these categories. */
enum IniLineType
{
    /** A section name in format "[alphanum_name]" */
    INI_LINE_SECTION,

    /** A key/value pair in format "key = value" */
    INI_LINE_VALUE,

    /** A blank line or a comment starting with ';' */
    INI_LINE_BLANK,

    /** An erroneous line */
    INI_LINE_ERROR,

    /** Internal error while parsing the line */
    INI_LINE_INTERROR
};


/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

/** @cond */
typedef struct Query Query;
typedef enum OpAssoc OpAssoc;
typedef enum IniLineType IniLineType;
typedef struct IniToken IniToken;
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

    /** A reserved space for populating with values referenced by @ref data,
     * with the same indexing (see @ref ArgList for more details).
     */
    ArgList *args;

    /** Operation stack in postfix format to remove all ambiguity.
     *
     * The stack consists of two types of numbers:
     * - indices to @ref data, which represent values
     * - operators (negative values by convention, see enum OpCode)
     */
    Stack *op_stack;
};

/** Holds complete information about a single (valid) line of an INI file. */
struct IniToken
{
    /** The type of the stored information. */
    IniLineType type;

    /** The stored information. */
    union {
        /** INI [section] name. */
        char *section;

        /** INI key/value pair. */
        struct {
            /** The key component of the pair. */
            char *key;

            /** The value component of the pair. */
            ArgVal value;
        } value;
    } token;
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
 * @param[in] str Input string to be parsed. The string may
 * be modified during the function execution, but by the end
 * it will be restored to its original form.
 *
 * @returns
 * - size of @p tokens_ptr - success
 * - -1 - memory error (malloc)
 * - -2 - invalid query
 * - -3 - internal error
 */
int tokenizeQueryString(Stack **tokens_ptr, DataSet **set_ptr, char *str);

/** Converts an infix stack into postfix stack.
 *
 * Input format:
 * A stack of integers. Each integer is treated as operand
 * if >=0, and as an operator if <0. Information about operator
 * precedence/associativity is fetched from global variables
 * @ref opPrec and @ref opAssoc.
 *
 * This function does not do any extensive safety checking, it expects
 * a valid infix input.
 *
 * @param[out] postfix_ptr Address of the stack.
 * @param[in] infix Array of tokens in infix order.
 *
 * @returns
 * - 0 - success
 * - 1 - memory error (malloc)
 * - 2 - internal error
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
 * - 1 - memory error (malloc/realloc)
 * - 2 - internal error
 * - 3 - illegal operation (e.g. multiplying strings)
 * - 4 - value not found in file
 */
int runQueries(FILE *file, const Query **queries, size_t qcount);

/** Utility function for fetching a new line into a buffer.
 *
 * The problem with built-in functions like @c fgets is that
 * they don't support dynamically-sized buffers and
 * concatenating several different buffers is a tedious
 * process. This function avoids the problem by increasing
 * the size of the buffer if necessary. This strategy has
 * the advantage of keeping the number of allocations to
 * a minimum, while having no restriction on the length of
 * the input line with all safety checks in place.
 *
 * @param[inout] file The input stream to read from.
 * @param[out] buf_ptr Pointer to buffer to populate with the new line.
 * The location of the buffer may be overwritten by realloc.
 * @param[inout] bufsize The size of the @p buf buffer. If
 * during function execution the original @p bufsize proves
 * insufficient to contain the entire line, @p buf is
 * reallocated to a bigger size and this variable is updated
 * accordingly.
 *
 * @returns
 * - 0      - success, newline reached
 * - @c EOF - success, end of file reached
 * - 1      - memory error (realloc)
 * - 2      - internal error
 */
int getLine(FILE *file, char **buf_ptr, size_t *bufsize);

/** Validates an INI file line and extracts information from it.
 *
 * @param line The line to interpret.
 *
 * @returns
 * An @ref IniToken object. If a syntax error is found in
 * the ini file, the object's type will be @ref INI_LINE_ERROR.
 * In case of internal errors, it will be @ref INI_LINE_INTERROR.
 */
IniToken iniExtractFromLine(const char *line);

/** Computes a list of queries and prints the results in order.
 *
 * This function assumes each query's @ref Query::args has already
 * been populated with all necessary values.
 *
 * @param[in] queries The list of queries to compute.
 * @param[in] qcount The number of elements in @p queries.
 *
 * @returns
 * - 0 - success
 * - 1 - memory error
 * - 2 - internal error
 * - 3 - illegal operation (e.g. multiplying strings)
 */
int printQueries(const Query **queries, size_t qcount);

#endif /* QUERY_H */
