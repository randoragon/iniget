#include "query.h"
#include "arglist.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>
#include <limits.h>

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
        size_t i;

        /* Pass-through 1: tokenize, validate and build DataSet */
        if (tokenizeQueryString(&tokens_infix, &set, str_cpy) < 0) {
            free(new);
            free(str_cpy);
            return 3;
        }

        printf("INFIX:   ");
        for (i = 0; i < tokens_infix->size; i++) {
            printf("%d, ", tokens_infix->data[i]);
        }
        putchar('\n');

        /* Pass-through 2: convert infix to postfix */
        if (infixPostfix(&tokens_postfix, tokens_infix)) {
            STAMP();
            error("infixPostfix failed");
            stackFree(tokens_infix);
            free(new);
            free(str_cpy);
            return 2;
        }

        printf("POSTFIX: ");
        for (i = 0; i < tokens_postfix->size; i++) {
            printf("%d, ", tokens_postfix->data[i]);
        }
        putchar('\n');

        /* Store the results in new query */
        new->data = set;
        new->op_stack = tokens_postfix;

        stackFree(tokens_infix);
    }

    /* Create an adequately-sized arglist */
    if (!(new->args = arglistCreate(new->data->size))) {
        info("memory error");
        free(new);
        free(str_cpy);
        return 1;
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
    } cur_tok, last_tok, prev; /* Current and last token type. Note that "token"
                                * here does not directly translate into the
                                * "token" that will be encoded on the tokens_ptr
                                * stack. Some extra values have been added
                                * (BEGIN, END, SPACE) to make parsing easier.
                                * prev is a mostly unimportant variable used only
                                * to cache cur_tok before reading characters to
                                * see if cur_tok has changed. */

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
    /* Implementation follows the Shunting-Yard algorithm,
     * the logic was carefully copied from here:
     * https://en.wikipedia.org/wiki/Shunting-yard_algorithm#The_algorithm_in_detail
     */

    Stack *new; /* Final output stack */
    Stack *ops; /* Supplementary stack for operators */
    int *i;     /* For iterating through infix stack */

    if (!infix) {
        STAMP();
        error("infix is NULL");
        return 2;
    }

    /* Alloc & init stacks */
    if (!(new = stackCreate())) {
        return 1;
    }
    if (!(ops = stackCreate())) {
        return 1;
    }

    /* Temporary macros to do less typing */
#define CLEANUP() do {      \
            stackFree(new); \
            stackFree(ops); \
        } while (0)
#define SPUSH(X, Y) do {                                    \
            if ((err = stackPush((X), (Y)))) {              \
                CLEANUP();                                  \
                if (err == 1) {                             \
                    return 1;                               \
                } else if (err == STACK_INTERNAL_ERROR) {   \
                    return 2;                               \
                }                                           \
            }                                               \
        } while (0)

    i = infix->data;
    while ((unsigned int)(i - infix->data) < infix->size) {
        int tok = *i; /* current token */
        int err;      /* for storing some error codes */

        if (tok >= 0) {
            /* Number token */
            SPUSH(new, tok);
        } else {
            int prec;      /* operator precedence value */
            OpAssoc assoc; /* operator associativity type */
            int top;       /* token at the top of ops stack */

            switch (tok) {
                case OP_ADD: case OP_SUB: /* fallthrough */
                case OP_MUL: case OP_DIV: /* fallthrough */
                case OP_MOD: case OP_POW: /* fallthrough */

                    /* Cache properties of current token */
                    prec  = opPrec[-tok];
                    assoc = opAssoc[-tok];

                    /* Store the top ops stack operator */
                    top = stackPeek(ops);
                    if (top == STACK_INTERNAL_ERROR) {
                        STAMP();
                        error("stackPeek failed");
                        CLEANUP();
                        return 2;
                    }

                    while (top != STACK_EMPTY
                            && (opPrec[top] > prec || (opPrec[top] == prec && assoc == OP_ASSOC_LEFT))
                            && top != OP_LPR) {
                        /* Pop top operator and add it to output (no error checking here, because
                         * we know from first call to stackPeek that stack is neither NULL nor empty). */
                        stackPop(ops);
                        SPUSH(new, top);

                        /* Fetch next element */
                        top = stackPeek(ops);
                    }

                    SPUSH(ops, tok);

                    break;
                case OP_LPR:
                    SPUSH(ops, tok);
                    break;
                case OP_RPR:
                    
                    /* Store the top ops stack operator */
                    top = stackPeek(ops);
                    if (top == STACK_INTERNAL_ERROR) {
                        STAMP();
                        error("stackPeek failed");
                        CLEANUP();
                        return 2;
                    }

                    while (top != OP_LPR) {

                        /* Pop top operator and add it to output (no error checking here, because
                         * we know from first call to stackPeek that stack is neither NULL nor empty). */
                        stackPop(ops);
                        SPUSH(new, top);

                        /* Fetch next element */
                        top = stackPeek(ops);
                    }

                    /* Discard the left parenthesis */
                    stackPop(ops);

                    break;
                default:
                    STAMP();
                    error("unmatched token (%d)", tok);
                    CLEANUP();
                    return 2;
            }
        }
        i++;
    }

    /* Pop any remaining tokens to new stack */
    while (ops->size > 0) {
        int err;
        if ((err = stackPush(new, stackPop(ops)))) {
            CLEANUP();
            if (err == 1) {
                return 1;
            } else if (err == STACK_INTERNAL_ERROR) {
                return 2;
            }
        }
    }

    /* Export the final stack */
    *postfix_ptr = new;

    /* Cleanup */
    stackFree(ops);
#undef CLEANUP
#undef SPUSH

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
    arglistFree(query->args);
    stackFree(query->op_stack);
    free(query);
}

int runQueries(FILE *file, const Query **queries, size_t qcount)
{
    char  *line;    /* Stores an entire line from file */
    size_t lsize;   /* Remembers the line size */
    bool   eof;     /* True if EOF was read */
    char  *section; /* Remembers the current section */
    size_t ssize;   /* Remembers the section size */
    size_t i;

    /* Allocate initial line and section buffers */
    lsize = 256; /* Arbitrary non-zero initial size */
    if (!(line = malloc(lsize * sizeof *line))) {
        info("memory error");
        return 1;
    }
    ssize = 256; /* Arbitrary non-zero initial size */
    if (!(section = malloc(ssize * sizeof *section))) {
        info("memory error");
        free(line);
        return 1;
    }
    section[0] = '\0'; /* Initialize section to none ("global scope") */

    /* Reset all query args to BLANK */
    for (i = 0; i < qcount; i++) {
        arglistClear(queries[i]->args);
    }

    /* Temporary convenience macro */
#define CLEANUP() do { \
                    free(line); \
                    free(section); \
                } while (0)

    eof = false;
    do {
        IniToken tok;

        /* Fetch next line */
        switch (getLine(file, &line, &lsize)) {
            case 0:
                break;
            case 1:
                return 1;
            case 2:
                STAMP();
                error("getLine internal error");
                CLEANUP();
                return 2;
            case EOF:
                eof = true;
                break;
            default:
                STAMP();
                error("unmatched return code of getLine");
                CLEANUP();
                return 2;
        }

        /* Parse INI line */
        tok = iniExtractFromLine(line);
        switch (tok.type) {
            case INI_LINE_ERROR:
                free(line);
                return 1;
            case INI_LINE_INTERROR:
                STAMP();
                error("iniExtractFromLine internal error");
                CLEANUP();
                return 2;
            case INI_LINE_SECTION:
                /* Update section string */
                if (ssize < strlen(tok.token.section) + 1) {
                    ssize *= 2;
                    if (!(section = realloc(section, ssize * sizeof *section))) {
                        info("memory error");
                        CLEANUP();
                        return 1;
                    }
                }
                strcpy(section, tok.token.section);
                free(tok.token.section);
                break;
            case INI_LINE_VALUE:
                /* Populate matched query parameters with value */
                for (i = 0; i < qcount; i++) {
                    size_t j;
                    for (j = 0; j < queries[i]->data->size; j++) {
                        /* Cache deeply nested variables */
                        char *sec = queries[i]->data->data[j].section;
                        char *key = queries[i]->data->data[j].key;

                        /* Copy in-file value into all matched indices in arglists */
                        if (strcmp(section, sec) == 0
                                && strcmp(tok.token.value.key, key) == 0
                                && queries[i]->args->data[j].type == ARGVAL_TYPE_NONE) {

                            queries[i]->args->data[j] = tok.token.value.value;

                            /* If the value type is string, deepcopy it */
                            if (tok.token.value.value.type == ARGVAL_TYPE_STRING) {
                                char *buf;
                                if (!(buf = malloc((strlen(tok.token.value.value.value.s) + 1) * sizeof *buf))) {
                                    info("memory error");
                                    CLEANUP();
                                    return 1;
                                }
                                strcpy(buf, tok.token.value.value.value.s);
                                queries[i]->args->data[j].value.s = buf;
                            }
                        }
                    }
                }
                free(tok.token.value.key);
                if (tok.token.value.value.type == ARGVAL_TYPE_STRING) {
                    free(tok.token.value.value.value.s);
                }
                break;
            case INI_LINE_BLANK:
                /* Gracefully skip */
                break;
            default:
                STAMP();
                error("unmatched IniLineType %d", tok.type);
                CLEANUP();
        }

    } while (!eof);

    /* Cleanup */
    free(line);
    free(section);
#undef CLEANUP

    /* All queries' arglists are populated, so
     * run computations and print the results. */
    return printQueries(queries, qcount);
}

int getLine(FILE *file, char **buf_ptr, size_t *bufsize)
{
    size_t pos; /* Current position in the buffer */
    int c;      /* Last read character from file */

    if (!file || !buf_ptr || !*buf_ptr || !bufsize) {
        STAMP();
        error("one of getLine parameters is NULL");
        return 2;
    }

    /* Read character-by-character until newline */
    pos = 0;
    c = fgetc(file);
    while (c != EOF && c != '\n') {

        /* Enlarge buffer if needed */
        if (pos == *bufsize - 1) {
            *bufsize *= 2;
            if (!(*buf_ptr = realloc(*buf_ptr, *bufsize * sizeof **buf_ptr))) {
                info("memory error");
                return 1;
            }
        }

        (*buf_ptr)[pos++] = c;
        c = fgetc(file);
    }

    /* Terminate the string */
    (*buf_ptr)[pos] = '\0';

    return (c == EOF)? EOF : 0;
}

IniToken iniExtractFromLine(const char *line)
{
    IniToken ret;
    const char *i, *j; /* i iterates forward, j marks the beginning of a token */

    i = j = line;

    /* Skip whitespace */
    while (isspace(*i))
        i++;

    if (*i == '[') {
        ret.type = INI_LINE_SECTION;
        
        /* Scan for the end of section */
        j = ++i;
        while (*i && *i != ']') {
            if (!isalnum(*i) && *i != '_') {
                info("error found in file (illegal character '%c' in section name)", *i);
                ret.type = INI_LINE_ERROR;
                return ret;
            }
            i++;
        }
        if (*i != ']') {
            info("error found in file (no closing bracket after section name)");
            ret.type = INI_LINE_ERROR;
            return ret;
        }
        
        /* Store the section name in a new buffer */
        if (!(ret.token.section = malloc((i - j + 1) * sizeof *ret.token.section))) {
            info("memory error");
            ret.type = INI_LINE_ERROR;
            return ret;
        }
        strncpy(ret.token.section, j, i - j + 1);
        ret.token.section[i - j] = '\0';
    } else if (isalnum(*i) || *i == '_') {
        char *val; /* Storage for the value part */

        ret.type = INI_LINE_VALUE;

        /* Find end of the key part */
        j = i;
        while (*i && !isspace(*i) && *i != '=')
            i++;
        if (!*i) {
            info("error found in file (no value after key name)");
            ret.type = INI_LINE_ERROR;
            return ret;
        }

        /* Store the key part in a new buffer */
        if (!(ret.token.value.key = malloc((i - j + 1) * sizeof *ret.token.value.key))) {
            info("memory error");
            ret.type = INI_LINE_ERROR;
            return ret;
        }
        strncpy(ret.token.value.key, j, i - j + 1);
        ret.token.value.key[i - j] = '\0';

        /* Search for '=' delimiter */
        while (*i && *i != '=')
            i++;
        if (*i != '=') {
            info("error found in file (no value after key name)");
            ret.type = INI_LINE_ERROR;
            return ret;
        }

        /* Skip whitespace */
        ++i;
        while (*i && isspace(*i))
            i++;
        if (!*i) {
            info("error found in file (no value after key name)");
            ret.type = INI_LINE_ERROR;
            return ret;
        }

        /* Find end of the value part */
        j = i;
        while (*i++)
            ;
        
        /* Store the value part in a new buffer */
        if (!(val = malloc((i - j + 1) * sizeof *val))) {
            info("memory error");
            ret.type = INI_LINE_ERROR;
            return ret;
        }
        strncpy(val, j, i - j + 1);
        val[i - j] = '\0';

        /* Determine type of the value */
        ret.token.value.value = argValGetFromString(val);
        if (ret.token.value.value.type == ARGVAL_TYPE_NONE) {
            ret.type = INI_LINE_ERROR;
        }
        free(val);
    } else if (*i == ';' || !*i) {
        ret.type = INI_LINE_BLANK;
    } else {
        error("error extracting ArgVal from \"%s\"\n", line);
        ret.type = INI_LINE_INTERROR;
    }

    return ret;
}

int printQueries(const Query **queries, size_t qcount)
{
    ValStack *vstack; /* evaluation stack */
    size_t i, j;

    /* Debug print */
    {
        size_t i;
        for (i = 0; i < qcount; i++) {
            size_t j;

            printf("-> query #%lu\n", i);
            for (j = 0; j < queries[i]->data->size; j++) {
                printf("\tsection: \"%s\"\n\tkey: \"%s\"\n", queries[i]->data->data[j].section, queries[i]->data->data[j].key);
                switch (queries[i]->args->data[j].type) {
                    case ARGVAL_TYPE_FLOAT:
                        printf("\ttype: FLOAT\n\tvalue: %f\n", queries[i]->args->data[j].value.f);
                        break;
                    case ARGVAL_TYPE_STRING:
                        printf("\ttype: STRING\n\tvalue: \"%s\"\n", queries[i]->args->data[j].value.s);
                        break;
                    default:
                        printf("\ttype: ERROR\n\tvalue: ERROR\n");
                        break;
                }
                putchar('\n');
            }
        }
    }

    if (!(vstack = valstackCreate())) {
        info("memory error");
        return 1;
    }

    for (i = 0; i < qcount; i++) {
        const Query *const query = queries[i]; /* shortcut */
        const Stack *const op_stack = query->op_stack; /* shortcut */
        ArgVal result;

        for (j = 0; j < op_stack->size; j++) {
            int idx, err;

            idx = op_stack->data[j];

            if (idx >= 0) {
                ArgVal val;

                if (idx >= (int)query->data->size) {
                    STAMP();
                    error("op_stack index (%d) out of DataSet range (%d)", idx, query->data->size);
                    valstackFree(vstack);
                    return 2;
                }

                /* Push value onto the vstack */
                val = query->args->data[idx];
                if ((err = valstackPush(vstack, val))) {
                    STAMP();
                    error("failed to push value onto vstack");
                    valstackFree(vstack);
                    return (err == 1)? 1 : 2;
                }
            } else {
                /* Pop 2 operands and push operation result */
                ArgVal i1, i2, i3;

                i2 = valstackPop(vstack);
                if (i2.type == ARGVAL_TYPE_NONE) {
                    STAMP();
                    error("failed to pop from vstack (%.0d)", i2.value.f);
                    valstackFree(vstack);
                    return 2;
                }

                i1 = valstackPop(vstack);
                if (i1.type == ARGVAL_TYPE_NONE) {
                    STAMP();
                    error("failed to pop from vstack (%.0d)", i1.value.f);
                    valstackFree(vstack);
                    return 2;
                }

                /* Perform operation */
                if (i1.type == ARGVAL_TYPE_FLOAT && i2.type == ARGVAL_TYPE_FLOAT) {
                    i3.type = ARGVAL_TYPE_FLOAT;
                    switch (idx) {
                        case OP_ADD:
                            i3.value.f = i1.value.f + i2.value.f;
                            break;
                        case OP_SUB:
                            i3.value.f = i1.value.f - i2.value.f;
                            break;
                        case OP_MUL:
                            i3.value.f = i1.value.f * i2.value.f;
                            break;
                        case OP_DIV:
                            i3.value.f = i1.value.f / i2.value.f;
                            break;
                        case OP_MOD:
                            i3.value.f = fmod(i1.value.f, i2.value.f);
                            break;
                        case OP_POW:
                            i3.value.f = pow(i1.value.f, i2.value.f);
                            break;
                        default:
                            STAMP();
                            error("unmatched operator index (%d)", idx);
                            valstackFree(vstack);
                            return 2;
                    }
                } else if (i1.type == ARGVAL_TYPE_STRING && i2.type == ARGVAL_TYPE_STRING) {
                    size_t s1, s3;
                    i3.type = ARGVAL_TYPE_STRING;
                    switch (idx) {
                        case OP_ADD:
                            s1 = strlen(i1.value.s);
                            s3 = s1 + strlen(i2.value.s);
                            if (!(i3.value.s = malloc((s3 + 1) * sizeof *i3.value.s))) {
                                info("memory error");
                                valstackFree(vstack);
                                return 1;
                            }
                            strcpy(i3.value.s, i1.value.s);
                            strcpy(i3.value.s + s1, i2.value.s);
                            i3.is_temporary = true;
                            break;
                        case OP_SUB: case OP_MUL: case OP_DIV: /* fallthrough */
                        case OP_MOD: case OP_POW:
                            info("illegal operation on two STRING values");
                            valstackFree(vstack);
                            return 3;
                        default:
                            STAMP();
                            error("unmatched operator index (%d)", idx);
                            valstackFree(vstack);
                            return 2;
                    }
                } else if ((i1.type == ARGVAL_TYPE_STRING && i2.type == ARGVAL_TYPE_FLOAT)
                        || (i1.type == ARGVAL_TYPE_FLOAT && i2.type == ARGVAL_TYPE_STRING)) {
                    
                    size_t s1, s3;
                    const char *str;
                    double num_f;
                    size_t num, k;

                    switch (idx) {
                        case OP_MUL:
                            /* Support Python-like string multiplication */
                            i3.type = ARGVAL_TYPE_STRING;
                            i3.is_temporary = true;

                            if (i1.type == ARGVAL_TYPE_STRING) {
                                str = i1.value.s;
                                num_f = i2.value.f;
                            } else {
                                str = i2.value.s;
                                num_f = i1.value.f;
                            }

                            s1 = strlen(str);

                            /* Prevent integer overflow */
                            if (i2.value.f > (double)ULONG_MAX) {
                                info("cannot multiply STRING by %.0g (factor too large)", i2.value.f);
                                valstackFree(vstack);
                                return 3;
                            } else if (i2.value.f > (double)ULONG_MAX / s1 - 1) {
                                info("cannot multiply STRING by %.0g (resulting string too long)", i2.value.f);
                                valstackFree(vstack);
                                return 3;
                            }
                            num = (size_t)num_f;

                            s3 = s1 * num;

                            if (!(i3.value.s = malloc((s3 + 1) * sizeof *i3.value.s))) {
                                info("memory error");
                                valstackFree(vstack);
                                return 1;
                            }
                            for (k = 0; k < num; k++) {
                                strcpy(i3.value.s + (k * s1), str);
                            }
                            break;
                        case OP_ADD: case OP_SUB: case OP_DIV: /* fallthrough */
                        case OP_MOD: case OP_POW:
                            info("illegal operation on STRING and FLOAT");
                            valstackFree(vstack);
                            return 3;
                        default:
                            STAMP();
                            error("unmatched operator index (%d)", idx);
                            valstackFree(vstack);
                            return 2;
                    }
                } else {
                    info("illegal operation involving %s and %s",
                            (i1.type == ARGVAL_TYPE_STRING)? "STRING" : "FLOAT",
                            (i2.type == ARGVAL_TYPE_STRING)? "STRING" : "FLOAT");
                    valstackFree(vstack);
                    return 3;
                }

                /* Free temporary strings */
                if (i1.type == ARGVAL_TYPE_STRING && i1.is_temporary) {
                    free(i1.value.s);
                }
                if (i2.type == ARGVAL_TYPE_STRING && i2.is_temporary) {
                    free(i2.value.s);
                }

                /* Push the new value */
                if (valstackPush(vstack, i3)) {
                    info("memory error");
                    valstackFree(vstack);
                    return 1;
                }
            }
        }

        /* Result is on the top of the stack */
        result = valstackPop(vstack);
        switch (result.type) {
            case ARGVAL_TYPE_STRING:
                printf("%s\n", result.value.s);
                free(result.value.s);
                break;
            case ARGVAL_TYPE_FLOAT:
                printf("%g\n", result.value.f);
                break;
            default:
                STAMP();
                error("query result has invalid type %d", result.type);
                valstackFree(vstack);
                return 2;
        }

        /* Clear the stack */
        valstackClear(vstack);
    }

    /* Cleanup */
    valstackFree(vstack);

    return 0;
}
