#include "arglist.h"
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

ArgList *arglistCreate(size_t size)
{
    ArgList *new;
    size_t i;

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

    for (i = 0; i < size; i++) {
        new->data[i].type = ARGVAL_TYPE_NONE;
    }

    return new;
}

void arglistClear(ArgList *arglist)
{
    size_t i;

    if (!arglist) {
        STAMP();
        error("arglist is NULL");
        return;
    }

    for (i = 0; i < arglist->size; i++) {
        if (arglist->data[i].type == ARGVAL_TYPE_STRING) {
            free(arglist->data[i].value.s);
        }
        arglist->data[i].type = ARGVAL_TYPE_NONE;
    }
}

void arglistFree(ArgList *arglist)
{
    size_t i;

    if (!arglist) {
        STAMP();
        error("arglist is NULL");
        return;
    }

    arglistClear(arglist);

    for (i = 0; i < arglist->size; i++) {
        if (arglist->data[i].type == ARGVAL_TYPE_STRING) {
            free(arglist->data[i].value.s);
        }
    }

    free(arglist->data);
    free(arglist);
}

ArgVal argValGetFromString(const char *str)
{
    ArgVal ret;
    const char *beg, *end; /* Marks beginning and end of the value */

    /* Locate where the value begins and ends */
    beg = str;
    end = str + strlen(str) - 1;
    while (isspace(*beg))
        beg++;
    while (isspace(*end))
        end--;

    /* Parse the value */
    if (*beg == '"' && *end == '"') {
        ret.type = ARGVAL_TYPE_STRING;

        /* Create a buffer for a string value */
        if (!(ret.value.s = malloc((end - beg) * sizeof *ret.value.s))) {
            info("memory error");
            ret.type = ARGVAL_TYPE_NONE;
            return ret;
        }

        /* Store the string in-between the double quotes */
        strncpy(ret.value.s, beg + 1, end - beg);
        ret.value.s[end - beg - 1] = '\0';
    } else {
        /* If format matches a number, treat it as number.
         * Otherwise fallback to string. */
        const char *i;
        int periods;

        /* Determine the value type */
        ret.type = ARGVAL_TYPE_NONE;
        for (i = beg, periods = 0; i <= end; i++) {
            if ((*i != '.' && !isdigit(*i)) || (*i == '.' && ++periods != 1)) {
                ret.type = ARGVAL_TYPE_STRING;
                break;
            }
        }
        if (ret.type == ARGVAL_TYPE_NONE) {
            ret.type = ARGVAL_TYPE_FLOAT;
        }

        switch (ret.type) {
            case ARGVAL_TYPE_STRING:
                /* Create a buffer for a string value */
                if (!(ret.value.s = malloc((end - beg + 2) * sizeof *ret.value.s))) {
                    info("memory error");
                    ret.type = ARGVAL_TYPE_NONE;
                    return ret;
                }

                /* Store the string */
                strncpy(ret.value.s, beg, end - beg + 2);
                ret.value.s[end - beg + 1] = '\0';
                break;
            case ARGVAL_TYPE_FLOAT:
                ret.value.f = atof(beg);
                break;
            default:
                STAMP();
                error("unexpected ret.type %d", ret.type);
                break;
        }
    }
    
    /* All strings processed by this function
     * come directly from an INI file, so therefore
     * they are not "temporary". */
    ret.is_temporary = false;

    return ret;
}
