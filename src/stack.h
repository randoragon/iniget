/** @file
 * Simple stack implementations.
 */

#ifndef STACK_H
#define STACK_H

#include "arglist.h"
#include <stdlib.h>
#include <limits.h>

/********************************************************
 *                     CONSTANTS                        *
 ********************************************************/

/** Returned by @ref stackPop and @ref stackPeek if stack is empty. */
#define STACK_EMPTY (INT_MIN)

/** Returned by some functions in case of an internal error. */
#define STACK_INTERNAL_ERROR (INT_MIN + 1)

/** The initial capacity of the stack (will dynamically increase if needed). */
#define STACK_INIT_CAPACITY 16

/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

/** @cond */
typedef struct Stack Stack;
typedef struct ValStack ValStack;
/** @endcond */


/********************************************************
 *                     DATA TYPES                       *
 ********************************************************/

/** A simple stack of ints implemented as an array. */
struct Stack
{
    /** Array of data. */
    int *data;

    /** Number of elements on the stack. */
    size_t size;

    /** Current max number of elements.
     *
     * The initial capacity is stored in @ref STACK_INIT_CAPACITY.
     * As more elements are pushed onto the stack, the capacity
     * will increase, but never decrease.*/
    size_t capacity;
};

/** A simple stack of @ref ArgVal implemented as an array. */
struct ValStack
{
    /** Array of data. */
    ArgVal *data;

    /** Number of elements on the stack. */
    size_t size;

    /** Current max number of elements.
     *
     * The initial capacity is stored in @ref STACK_INIT_CAPACITY.
     * As more elements are pushed onto the stack, the capacity
     * will increase, but never decrease.*/
    size_t capacity;
};


/********************************************************
 *                     FUNCTIONS                        *
 ********************************************************/

/** Allocates a new stack and returns its address.
 *
 * @returns
 * - valid address - success
 * - @c NULL - failure (malloc)
 */
Stack *stackCreate();

/** Pushes a new value onto a stack.
 *
 * Note that pushing @ref STACK_EMPTY is illegal.
 *
 * @returns
 * - 0 - success
 * - 1 - failure (realloc)
 * - @ref STACK_INTERNAL_ERROR - internal error
 */
int stackPush(Stack *stack, int val);

/** Returns a stack's top value and removes it.
 *
 * @returns
 * - @ref STACK_EMPTY - stack is empty
 * - @ref STACK_INTERNAL_ERROR - internal error
 * - anything else - success
 */
int stackPop(Stack *stack);

/** Returns a stack's top value without removing it.
 *
 * @returns
 * - @ref STACK_EMPTY - stack is empty
 * - @ref STACK_INTERNAL_ERROR - internal error
 * - anything else - success
 */
int stackPeek(const Stack *stack);

/** Frees all memory owned by the stack. */
void stackFree(Stack *stack);

/** Allocates a new valstack and returns its address.
 *
 * @returns
 * - valid address - success
 * - @c NULL - failure (malloc)
 */
ValStack *valstackCreate();

/** Pushes a new value onto a valstack.
 *
 * @returns
 * - 0 - success
 * - 1 - failure (realloc)
 * - @ref STACK_INTERNAL_ERROR - internal error
 */
int valstackPush(ValStack *valstack, ArgVal val);

/** Returns a valstack's top value and removes it.
 *
 * @returns
 * - if @ref ArgVal::type is @ref ARGVAL_TYPE_NONE
 *      - if @ref ArgVal::value's @c f is 0 - stack is empty
 *      - if @ref ArgVal::value's @c f is not 0 - internal error
 * - anything else - success
 */
ArgVal valstackPop(ValStack *valstack);

/** Returns a valstack's top value without removing it.
 *
 * @returns
 * - if @ref ArgVal::type is @ref ARGVAL_TYPE_NONE
 *      - if @ref ArgVal::value's @c f is 0 - stack is empty
 *      - if @ref ArgVal::value's @c f is not 0 - internal error
 * - anything else - success
 */
ArgVal valstackPeek(const ValStack *valstack);

/** Empties the stack of all elements */
void valstackClear(ValStack *vstack);

/** Frees all memory owned by the valstack. */
void valstackFree(ValStack *valstack);

#endif /* STACK_H */
