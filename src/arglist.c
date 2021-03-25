#include "arglist.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>

ArgList *arglistCreate(size_t size)
{
    ArgList *new;

    if (!(new = malloc(sizeof *new))) {
        info("memory error");
        return NULL;
    }

    new->size = size;
    if (!(new->data = malloc(new->size * sizeof *new->data))) {
        info("memory error");
        free(new);
        return NULL;
    }

    return new;
}

void arglistFree(ArgList *arglist)
{
    if (!arglist) {
        STAMP();
        error("arglist is NULL");
        return;
    }

    free(arglist->data);
    free(arglist);
}
