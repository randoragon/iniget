#include "stack.h"
#include "error.h"
#include "arglist.h"
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
        STAMP();
        error("stack is NULL");
        return STACK_INTERNAL_ERROR;
    }
    if (val == STACK_EMPTY) {
        STAMP();
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
        STAMP();
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
        STAMP();
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
        STAMP();
        error("stack is NULL");
        return;
    }

    free(stack->data);
    free(stack);
}

ValStack *valstackCreate()
{
    ValStack *new;

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

int valstackPush(ValStack *vstack, ArgVal val)
{
    if (!vstack) {
        STAMP();
        error("vstack is NULL");
        return STACK_INTERNAL_ERROR;
    }

    /* Increase capacity, if needed */
    if (vstack->size == vstack->capacity) {
        vstack->capacity *= 2;
        if (!(vstack->data = realloc(vstack->data, vstack->capacity * sizeof *vstack->data))) {
            info("memory error");
            return 1;
        }
    }

    /* Push the new element */
    vstack->data[vstack->size++] = val;

    return 0;
}

ArgVal valstackPop(ValStack *vstack)
{
    ArgVal ret;

    if (!vstack) {
        STAMP();
        error("vstack is NULL");
        ret.type = ARGVAL_TYPE_NONE;
        ret.value.f = 1;
        return ret;
    }

    if (vstack->size == 0) {
        ret.type = ARGVAL_TYPE_NONE;
        ret.value.f = 0;
        return ret;
    }

    ret = vstack->data[--vstack->size];

    return ret;
}

ArgVal valstackPeek(const ValStack *vstack)
{
    ArgVal ret;

    if (!vstack) {
        STAMP();
        error("vstack is NULL");
        ret.type = ARGVAL_TYPE_NONE;
        ret.value.f = 1;
        return ret;
    }

    if (vstack->size == 0) {
        ret.type = ARGVAL_TYPE_NONE;
        ret.value.f = 0;
        return ret;
    }

    ret = vstack->data[vstack->size - 1];

    return ret;
}

void valstackClear(ValStack *vstack)
{
    size_t i;

    if (!vstack) {
        STAMP();
        error("vstack is NULL");
        return;
    }

    for (i = 0; i < vstack->size; i++) {
        if (vstack->data[i].type == ARGVAL_TYPE_STRING) {
            free(vstack->data[i].value.s);
        }
    }

    vstack->size = 0;
}

void valstackFree(ValStack *vstack)
{
    if (!vstack) {
        STAMP();
        error("vstack is NULL");
        return;
    }

    valstackClear(vstack);

    free(vstack->data);
    free(vstack);
}
