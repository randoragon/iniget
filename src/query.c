#include "query.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

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
    char *str_cpy;

    if (!query_ptr) {
        STAMP();
        error("query_ptr is NULL");
        return 2;
    }

    /* Allocate a new query */
    if (!(new = malloc(sizeof *new))) {
        info("memory error");
        return 1;
    }

    /* Create a temporary copy of str.
     * This is necessary, because tokenizeQueryString
     * needs a mutable string.
     */
    if (!(str_cpy = malloc((strlen(str) + 1) * sizeof *str))) {
        info("memory error");
        free(new);
        return 1;
    }
    strcpy(str_cpy, str);

    /* Construct new query from string */
    {
        Stack *tokens_infix, *tokens_postfix;
        DataSet *set;

        /* Pass-through 1: tokenize, validate and build DataSet */
        if (tokenizeQueryString(&tokens_infix, &set, str_cpy) < 0) {
            free(new);
            free(str_cpy);
            return 3;
        }

        /* Pass-through 2: convert infix to postfix */
        if (infixPostfix(&tokens_postfix, tokens_infix)) {
            STAMP();
            error("infixPostfix failed");
            free(tokens_infix);
            free(new);
            free(str_cpy);
            return 2;
        }

        /* Store the results in new query */
        new->data = set;
        new->op_stack = tokens_postfix;

        stackFree(tokens_infix);
    }

    /* Output the new query */
    *query_ptr = new;

    /* Cleanup */
    free(str_cpy);

    return 0;
}

int tokenizeQueryString(Stack **tokens_ptr, DataSet **set_ptr, char *str)
{
    DataSet *set;
    Stack *tokens;
    char *i, *j, *k; /* i - Reads char-by-char.
                      * j - Marks the beginning of a currently read token.
                      * k - Marks the beginning of the last token that was
                      *     parsed and added to the dataset. */
    Stack *parens; /* stack for catching unmatched parentheses */
    enum {
        BEGIN, END, /* the start and end of the string */
        VAL, OP,    /* operands and operators */
        LPR, RPR,   /* left and right parentheses */
        SPACE       /* any whitespace character */
    } cur_tok, last_tok; /* Current and last token type. Note that "token"
                          * here does not directly translate into the
                          * "token" that will be encoded on the tokens_ptr
                          * stack. Some extra values have been added
                          * (BEGIN, END, SPACE) to make parsing easier. */

    /* Initialize needed structures */
    if (!(parens = stackCreate())) {
        info("memory error");
        return -1;
    }
    if (!(set = datasetCreate())) {
        info("memory error");
        stackFree(parens);
        return -1;
    }
    if (!(tokens = stackCreate())) {
        info("memory error");
        datasetFree(set);
        stackFree(parens);
        return -1;
    }

    /* Helper macro to make code less redundant */
#define CLEANUP() do { \
                    stackFree(parens); \
                    stackFree(tokens); \
                    datasetFree(set); \
                } while (0)

    /* Validate, extract and tokenize str */
    i = j = str, k = NULL;
    cur_tok = last_tok = BEGIN;
    while (1) {
        char c = *i; /* The current character */
        bool cur_changed; /* true iff cur_tok changed its value in the current iteration */
        int prev; /* Caches cur_tok to help determine the value of cur_changed */

        /* Read the current token */
        cur_changed = false;
        prev = cur_tok;
        if (isalnum(c) || c == '_' || c == '.') {
            cur_tok = VAL;
        } else if (strchr("+-*/^", c)) {
            cur_tok = (c == '\0')? END : OP;
        } else if (c == '(') {
            int err;
            cur_tok = LPR;
            err = stackPush(parens, '(');
            if (err == 1) {
                info("memory error");
                CLEANUP();
                return -1;
            } else if (err == STACK_INTERNAL_ERROR) {
                STAMP();
                error("stackPush internal error");
                CLEANUP();
                return -3;
            }
        } else if (c == ')') {
            int elem;
            cur_tok = RPR;
            elem = stackPop(parens);
            if (elem != '(') {
                CLEANUP();
                if (elem == STACK_EMPTY) {
                    info("invalid query (unbalanced parentheses)");
                    return -2;
                } else {
                    STAMP();
                    error("stackPop internal error (%d)", elem);
                    return -3;
                }
            }
        } else if (isspace(c)) {
            cur_tok = SPACE;
        } else {
            info("illegal character '%c' in query", c);
            CLEANUP();
            return -2;
        }
        cur_changed = (cur_tok != prev || strchr("+-*/^()", c)); /* single-character tokens are a guaranteed token change */
        
        /* If last_tok has been read in its entirety, parse it */
        if (cur_changed && j != k) {
            char tmp, *period;
            int idx, err;
            bool is_parsed = false;

            switch (last_tok) {
                case BEGIN:
                    /* Do nothing */
                    break;
                case VAL:
                    /* We use an ugly trick to help with parsing:
                     * Store *i in tmp, then set *i to NULL to
                     * treat j as a substring. After everything
                     * set *i back to tmp.  */
                    tmp = *i;
                    memcpy(i, "", sizeof *i);
                    period = strchr(j, '.');
                    if (period == NULL) {
                        /* the value is in the INI "global scope", outside of all sections */
                        idx = datasetAdd(set, "", j);
                    } else {
                        /* Run some more error checks */
                        if (*(period + 1) == '\0') {
                            info("invalid query (blank key part in operand name)");
                            CLEANUP();
                            return -2;
                        }
                        if (strchr(period + 1, '.')) {
                            info("invalid query (more than 1 period in operand name)");
                            CLEANUP();
                            return -2;
                        }
                        *period = '\0';
                        idx = datasetAdd(set, j, period + 1);
                        *period = '.';
                    }

                    /* Handle datasetAdd errors */
                    if (idx < 0) {
                        if (idx == -1) {
                            info("memory error");
                            CLEANUP();
                            return -1;
                        } else {
                            STAMP();
                            error("datasetAdd internal error (%d)", idx);
                            CLEANUP();
                            return -2;
                        }
                    }

                    /* Finish the "ugly trick" */
                    memcpy(i, &tmp, sizeof *i);
                    is_parsed = true;

                    /* Push the result */
                    err = stackPush(tokens, idx);
                    if (err == 1) {
                        info("memory error");
                        return -1;
                    } else if (err == STACK_INTERNAL_ERROR) {
                        STAMP();
                        error("stackPush internal error");
                        return -3;
                    }
                    break;
                case OP: case LPR: case RPR:
                    switch (*j) {
                        case '+': idx = OP_ADD; break;
                        case '-': idx = OP_SUB; break;
                        case '*': idx = OP_MUL; break;
                        case '/': idx = OP_DIV; break;
                        case '%': idx = OP_MOD; break;
                        case '^': idx = OP_POW; break;
                        case '(': idx = OP_LPR; break;
                        case ')': idx = OP_RPR; break;
                        default:
                            STAMP();
                            error("failed to match operator '%c'", *j);
                            CLEANUP();
                            return -2;
                    }

                    is_parsed = true;
                    break;
                case END: case SPACE:
                    /* last_tok should never be equal to END or SPACE */
                    STAMP();
                    error("last_tok set to END");
                    CLEANUP();
                    return -2;
                default:
                    STAMP();
                    error("failed to match last_tok value in switch");
                    CLEANUP();
                    return -2;
            }

            /* If the token was correctly identified */
            if (is_parsed) {

                /* Push the new token */
                err = stackPush(tokens, idx);
                if (err == 1) {
                    info("memory error");
                    return -1;
                } else if (err == STACK_INTERNAL_ERROR) {
                    STAMP();
                    error("stackPush internal error");
                    return -3;
                }

                /* Update k */
                k = j;
            }
        }

        /* If a new token was found */
        if (cur_changed && cur_tok != SPACE) {

            /* Catch illegal pairs of adjacent tokens */
            if (last_tok == VAL && cur_tok == VAL) {
                info("invalid query (missing operator between two operands)");
                CLEANUP();
                return -2;
            } else if (last_tok == OP && cur_tok == OP) {
                info("invalid query (missing operand between two operators)");
                CLEANUP();
                return -2;
            } else if (last_tok == OP && cur_tok == RPR) {
                info("invalid query (missing operand before closing parenthesis)");
                CLEANUP();
                return -2;
            } else if (last_tok == LPR && cur_tok == OP) {
                info("invalid query (missing operand before operator)");
                CLEANUP();
                return -2;
            } else if (last_tok == LPR && cur_tok == RPR) {
                info("invalid query (missing expression inside parentheses)");
                CLEANUP();
                return -2;
            } else if (last_tok == RPR && cur_tok == LPR) {
                info("invalid query (missing operator between two parentheses blocks)");
                CLEANUP();
                return -2;
            } else if (last_tok == VAL && cur_tok == LPR) {
                info("invalid query (missing operator between operand and opening parenthesis)");
                CLEANUP();
                return -2;
            } else if (last_tok == RPR && cur_tok == VAL) {
                info("invalid query (missing operator between closing parenthesis and operand)");
                CLEANUP();
                return -2;
            } else if (last_tok == BEGIN && cur_tok == OP) {
                info("invalid query (missing operand before operator)");
                CLEANUP();
                return -2;
            } else if (last_tok == OP && cur_tok == END) {
                info("invalid query (missing operand after operator)");
                CLEANUP();
                return -2;
            }

            /* Update last token */
            last_tok = cur_tok;
            j = i;
        }

        /* Fetch next character / break out if finished */
        if (*i++ == '\0') {
            break;
        }
    }

    /* Check for unbalanced parenteses */
    if (parens->size != 0) {
        info("invalid query (unbalanced parentheses)");
        CLEANUP();
        return -2;
    }

    /* Export results */
    *tokens_ptr = tokens;
    *set_ptr = set;

    /* Cleanup */
    stackFree(parens);
#undef CLEANUP

    return tokens->size;
}

int infixPostfix(Stack **postfix_ptr, const Stack *infix)
{
    Stack *new;

    if (!(new = stackCreate())) {
        info("memory error");
        return 1;
    }

    /* TODO */

    *postfix_ptr = new;
    return 0;
}

void queryFree(Query *query)
{
    if (!query) {
        STAMP();
        error("query is NULL");
        return;
    }

    datasetFree(query->data);
    stackFree(query->op_stack);
    free(query);
}

int runQueries(FILE *file, const Query **queries, size_t qcount)
{
    /* TODO */
    return 0;
}
