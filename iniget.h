#ifndef INIGET_H
#define INIGET_H

#include "stack.h"

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
    OP_MOD = -5,
};


/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

typedef struct Query Query;
typedef struct Data Data;


/********************************************************
 *                     DATA TYPES                       *
 ********************************************************/

/** A single INI location of a piece of data.
 *
 * This struct holds an "address" in the INI format.
 * It is used to reference a single value within a file.
 * The type or existence of this value is not known until
 * access is attempted.
 */
struct Data
{
    /** The name of the section (excluding brackets). */
    char *section;
    /** The name of the key. */
    char *key;
};

/** Holds a single query to be executed on a file.
 *
 * The raw query string input by the user is first
 * preprocessed, and the more concise form is stored
 * inside this struct.
 */
struct Query
{
    /** An ordered set of all data required to compute the query. */
    Data *data;
    /** Operation stack in postfix format to remove all ambiguity.
     *
     * The stack consists of two types of numbers:
     * - indices to @ref data, which represent values
     * - operators (negative values by convention, see @ref OpCode)
     */
    int *op_stack;
};

#endif
