#include "query.h"
#include "error.h"
#include <stdlib.h>

/* Define operator associativity */
const OpAssoc opAssoc[OP_COUNT] = {
    /* padding */ OP_ASSOC_NA,
    /* OP_ADD  */ OP_ASSOC_ANY,
    /* OP_SUB  */ OP_ASSOC_ANY,
    /* OP_MUL  */ OP_ASSOC_LEFT,
    /* OP_DIV  */ OP_ASSOC_LEFT,
    /* OP_MOD  */ OP_ASSOC_LEFT,
    /* OP_LPR  */ OP_ASSOC_NA,
    /* OP_RPR  */ OP_ASSOC_NA,
    /* OP_POW  */ OP_ASSOC_RIGHT
};

/* Define operator precedence */
const int opPrec[OP_COUNT] = {
    /* padding */ OP_ASSOC_NA,
    /* OP_ADD: */ 1,
    /* OP_SUB  */ 1,
    /* OP_MUL  */ 2,
    /* OP_DIV  */ 2,
    /* OP_MOD  */ 2,
    /* OP_LPR  */ -1, /* non-applicable */
    /* OP_RPR  */ -1, /* non-applicable */
    /* OP_POW  */ 3
};

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
        int *tokens_infix;
        Stack *tokens_postfix;
        DataSet *set;

        /* Pass-through 1: tokenize, validate and build DataSet */
        if (tokenizeQueryString(&tokens_infix, &set, str) < 0) {
            info("invalid query");
            free(new);
            return 3;
        }

        /* Pass-through 2: convert infix to postfix */
        if (infixPostfix(&tokens_postfix, tokens_infix)) {
            error("infixPostfix failed");
            free(tokens_infix);
            free(new);
            return 2;
        }

        /* Store the results in new query */
        new->data = set;
        new->op_stack = tokens_postfix;

        free(tokens_infix);
    }

    /* Output the new query */
    *query_ptr = new;

    return 0;
}

int tokenizeQueryString(int **tokens_ptr, DataSet **set_ptr, const char *str)
{
    /* TODO */
    return 0;
}

int infixPostfix(Stack **postfix_ptr, const int *infix)
{
    /* TODO */
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
