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
        Stack *tokens_infix, *tokens_postfix;
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

int tokenizeQueryString(Stack **tokens_ptr, DataSet **set_ptr, const char *str)
{
    DataSet *set;
    Stack *tokens;
    const char *i, *j; /* i reads char-by-char, j marks the beginning of a currently read token */
    Stack *parens; /* stack for catching unmatched parentheses */
    enum { NONE, VAL, OP, LPR, RPR } cur_tok, last_tok; /* Current and last token type.
                                                      * Parentheses are considered a
                                                      * separate types here for convenience. */
    bool is_same; /* True if last_tok and cur_tok reference the same token,
                   * for example in the middle of reading a long VAL name.
                   * False otherwise. This is used to catch errors based
                   * on invalid consecutive tokens. */

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
    i = j = str;
    cur_tok = last_tok = NONE;
    is_same = true;
    while (*i) {
        char c = *i;

        /* Read the current token */
        if (isalnum(c) || c == '_' || c == '.') {
            cur_tok = VAL;
        } else if (strchr("+-*/^", c)) {
            cur_tok = OP;
        } else if (c == '(') {
            int err;
            cur_tok = LPR;
            err = stackPush(parens, '(');
            if (err == 1) {
                info("memory error");
                CLEANUP();
                return -1;
            } else if (err == STACK_INTERNAL_ERROR) {
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
                    error("stackPop internal error (%d)", elem);
                    return -3;
                }
            }
        } else if (isspace(c)) {
            cur_tok = NONE;
            is_same = false;
        } else {
            info("illegal character '%c' in query", c);
            CLEANUP();
            return -2;
        }

        /* Catch all illegal pairs of adjacent tokens */
        if (!is_same && last_tok != NONE && cur_tok != NONE) {
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
            }
        }

        /* If last_tok has been read in its entirety, tokenize it */
        if (!is_same) {
            char tmp, *period;
            int idx, err;

            switch (last_tok) {
                case VAL:
                    /* We use an ugly trick to help with parsing:
                     * Store *i in tmp, then set *i to NULL to
                     * treat j as a substring. After everything
                     * set *i back to tmp.  */
                    tmp = *i;
                    period = strchr(j, '.');
                    if (period == NULL) {
                        /* the value is in the INI "global scope", outside of all sections */
                        idx = datasetAdd(set, "", j);
                    } else {
                        *period = '\0';
                        idx = datasetAdd(set, j, period + 1);
                        *period = '.';
                    }

                    /* Handle datasetAdd errors */
                    if (idx == -1) {
                        info("memory error");
                        CLEANUP();
                        return -1;
                    } else {
                        error("datasetAdd internal error (%d)", idx);
                        CLEANUP();
                        return -2;
                    }

                    /* Finish the "ugly trick" */
                    memcpy((void*)i, &tmp, sizeof *i);

                    /* Push the result */
                    err = stackPush(tokens, idx);
                    if (err == 1) {
                        info("memory error");
                        return -1;
                    } else if (err == STACK_INTERNAL_ERROR) {
                        error("stackPush internal error");
                        return -3;
                    }
                    break;
                case OP:
                    switch (*j) {
                        case '+': idx = OP_ADD; break;
                        case '-': idx = OP_SUB; break;
                        case '*': idx = OP_MUL; break;
                        case '/': idx = OP_DIV; break;
                        case '%': idx = OP_MOD; break;
                        case '^': idx = OP_POW; break;
                        default:
                            error("failed to match operator '%c'", *j);
                            CLEANUP();
                            return -2;
                    }

                    /* Push the result */
                    err = stackPush(tokens, idx);
                    if (err == 1) {
                        info("memory error");
                        return -1;
                    } else if (err == STACK_INTERNAL_ERROR) {
                        error("stackPush internal error");
                        return -3;
                    }
                    break;
                case NONE: 
                    if (j != str) {
                        /* last_tok should only ever be equal to NONE
                         * at the first iteration of the loop. */
                        error("last_tok set to NONE");
                        CLEANUP();
                        return -2;
                    }
                    break;
                default:
                    error("failed to match last_tok value in switch");
                    CLEANUP();
                    return -2;
            }
        }

        ++i;
    }

    /* Check for unmatched parenteses */
    if (parens->size != 0) {
        info("invalid query (unmatched parentheses)");
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

int runQueries(FILE *file, const Query **queries, size_t qcount)
{
    /* TODO */
    return 0;
}
