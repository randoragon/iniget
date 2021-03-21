#include "stack.h"
#include "error.h"
#include <stdlib.h>


Stack *stackCreate()
{
    Stack *new;

    if (!(new = malloc(sizeof *new))) {
        info("memory error");
        return NULL;
    }

    new->capacity = STACK_INIT_CAPACITY;
    if (!(new->data = malloc(new->capacity * sizeof *new->data))) {
        info("memory error");
        free(new);
        return NULL;
    }

    new->size = 0;

    return new;
}

int stackPush(Stack *stack, int val)
{
    if (!stack) {
        error("stack is NULL");
        return STACK_INTERNAL_ERROR;
    }
    if (val == STACK_EMPTY) {
        error("val cannot be equal to STACK_EMPTY");
        return STACK_INTERNAL_ERROR;
    }

    /* Increase capacity, if needed */
    if (stack->size == stack->capacity) {
        stack->capacity *= 2;
        if (!(stack->data = realloc(stack->data, stack->capacity * sizeof *stack->data))) {
            info("memory error");
            return 1;
        }
    }

    /* Push the new element */
    stack->data[stack->size++] = val;

    return 0;
}

int stackPop(Stack *stack)
{
    if (!stack) {
        error("stack is NULL");
        return STACK_INTERNAL_ERROR;
    }

    if (stack->size == 0) {
        return STACK_EMPTY;
    }

    return stack->data[--stack->size];
}

int stackPeek(const Stack *stack)
{
    if (!stack) {
        error("stack is NULL");
        return STACK_INTERNAL_ERROR;
    }

    if (stack->size == 0) {
        return STACK_EMPTY;
    }

    return stack->data[stack->size - 1];
}

void stackFree(Stack *stack)
{
    if (!stack) {
        error("stack is NULL");
        return;
    }

    free(stack->data);
    free(stack);
}
