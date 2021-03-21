#include "query.h"
#include "error.h"
#include <stdlib.h>


int parseQueryString(Query **query_ptr, const char *str)
{
    Query *new;

    if (!query_ptr) {
        error("query_ptr is NULL");
        return 2;
    }

    /* Allocate a new query */
    if (!(new = malloc(sizeof *new))) {
        info("memory error");
        return 1;
    }

    /* Construct new query from string */
    {
        /* To maximize efficiency, only a single pass-through
         * is used to validate and extract information from
         * the query string, at the cost of making the code
         * somewhat more convoluted.
         *
         * To simplify things, here is a brief explanation
         * of the parsing strategy and some variables:
         * 
         * - The string is read character-by-character.
         * - A stack is used to store indices of open parentheses
         *   to check for unbalanced ones.
         */
        Data *data;
        size_t data_size;
        char *p, *q;
        Stack *p_l;

        if ((p_l = stackCreate()) == NULL) {
            info("memory error");
            return 1;
        }

        /* Allocate data to be able to fit all distinct values */
        data_size = 16;
        if (!(data = malloc(data_size * sizeof *data))) {
            info("memory error");
            return 1;
        }
        
        /* TODO */
    }

    /* Output the new query */
    *query_ptr = new;

    return 0;
}

int validateQueryString(const char *str)
{
    return 0;
}

void queryFree(Query *query)
{
    if (!query) {
        error("query is NULL");
        return;
    }

    free(query->data);
    free(query);
}
