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
    /* OP_ADD  */ OP_ASSOC_LEFT,
    /* OP_SUB  */ OP_ASSOC_LEFT,
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
            stackFree(tokens_infix);
            free(new);
            free(str_cpy);
            return 2;
        }

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
    Stack *tokens; /* Stack for the tokenized output */
    Stack *parens; /* Stack for catching unbalanced parentheses */
    DataSet *set;  /* DataSet of output section/key pairs */
    char *i, *j;   /* Iterators: i scouts ahead, j remembers beginning of token */
    enum {
        BEGIN,  /* beginning of str */
        VALUE,  /* a brace-enclosed {operand} */
        OP,     /* an operator */
        LPR,    /* left parenthesis */
        RPR    /* right parenthesis */
    } cur_tok, last_tok; /* the type of the current and last read token */

    /* Allocate necessary space */
    if (!(tokens = stackCreate())) {
        info("memory error");
        return -1;
    }
    if (!(parens = stackCreate())) {
        info("memory error");
        stackFree(tokens);
        return -1;
    }
    if (!(set = datasetCreate())) {
        info("memory error");
        stackFree(tokens);
        stackFree(parens);
        return -1;
    }

    /* Temporary convenience macros to keep code cleaner */
#define CLEANUP() do {              \
                stackFree(tokens);  \
                stackFree(parens);  \
                datasetFree(set);   \
            } while (0)
#define SPUSH(S, X) do { \
                switch (stackPush((S), (X))) {              \
                    case 0: break;                          \
                    case 1: CLEANUP(); return -1;           \
                    case STACK_INTERNAL_ERROR:              \
                        STAMP();                            \
                        error("stackPush internal error");  \
                        return -3;                          \
                    default:                                \
                        STAMP();                            \
                        error("unmatched return code");     \
                        return -3;                          \
                }                                           \
            } while (0)

    /* Parse all tokens */
    cur_tok = BEGIN;
    i = str;
    while (*i) {

        /* Skip to the beginning of next token */
        while (isspace(*i))
            i++;

        /* Determine and parse cur_token */
        last_tok = cur_tok;
        if (*i == '{') {
            char *period;      /* for finding the period separator later */
            char *sec, *key;   /* buffers for section and key names */
            size_t ssec, skey; /* size of sec and key buffers */
            int idx;           /* dataset index of a new value */

            cur_tok = VALUE;

            /* Catch syntax errors */
            switch (last_tok) {
                case BEGIN: case LPR: case OP:
                    /* Gracefully break */
                    break;
                case VALUE: case RPR:
                    /* Implicit multiplication */
                    SPUSH(tokens, OP_MUL);
                    break;
                default:
                    STAMP();
                    error("invalid last_tok %d", last_tok);
                    CLEANUP();
                    return -3;
            }

            /* Locate the value and period within */
            j = i++;
            period = NULL;
            while (*i && *i != '}') {
                if (*i == '.') {
                    if (!period) {
                        period = i;
                    } else {
                        info("invalid query (illegal period spotted at pos %ld)", i - str + 1);
                        CLEANUP();
                        return -2;
                    }
                } else if (!isalnum(*i) && *i != '-' && *i != '_') {
                    info("invalid query (illegal character '%c' at pos %ld)", *i, i - str + 1);
                    CLEANUP();
                    return -2;
                }
                i++;
            }
            if (!*i) {
                info("invalid query (non-terminated brace at pos %ld)", j - str + 1);
                CLEANUP();
                return -2;
            }

            /* Validate the enclosed string */
            ssec = period ? period - j : 1;
            skey = period ? i - period : i - j;
            if (i - j <= 1) {
                info("invalid query (empty braces at pos %ld)", j - str + 1);
                CLEANUP();
                return -2;
            }
            if ((period ? i - period : i - j) <= 1) {
                info("invalid query (key name missing at pos %ld)", i - str);
                CLEANUP();
                return -2;
            }

            /* Extract section and key subcomponents */
            if (!(sec = malloc(ssec * sizeof *sec))) {
                info("memory error");
                CLEANUP();
                return -1;
            }
            if (!(key = malloc(skey * sizeof *key))) {
                info("memory error");
                free(sec);
                CLEANUP();
                return -1;
            }
            strncpy(sec, j + 1, ssec);
            strncpy(key, period ? period + 1 : j + 1, skey);
            sec[ssec - 1] = '\0';
            key[skey - 1] = '\0';

            /* Get index in dataset */
            if ((idx = datasetAdd(set, sec, key)) < 0) {
                free(sec);
                free(key);
                switch (idx) {
                    case -1:
                        CLEANUP();
                        return -1;
                    case DATASET_INTERNAL_ERROR:
                        STAMP();
                        error("datasetAdd internal error");
                        CLEANUP();
                        return -3;
                    default:
                        STAMP();
                        error("unmatched return code");
                        CLEANUP();
                        return -3;
                }
            }

            /* Cleanup */
            free(sec);
            free(key);

            /* Push index to the tokens stack */
            SPUSH(tokens, idx);

            i++;

        } else if (*i == '(') {
            cur_tok = LPR;

            /* Catch syntax errors */
            switch (last_tok) {
                case BEGIN: case OP: case LPR:
                    /* Gracefully break */
                    break;
                case VALUE: case RPR:
                    /* Implicit multiplication */
                    SPUSH(tokens, OP_MUL);
                    break;
                default:
                    STAMP();
                    error("invalid last_tok %d", last_tok);
                    CLEANUP();
                    return -3;
            }

            SPUSH(parens, '('); /* the exact value here doesn't matter */
            SPUSH(tokens, OP_LPR);

            i++;
        } else if (*i == ')') {
            cur_tok = RPR;

            /* Catch syntax errors */
            switch (last_tok) {
                case VALUE: case RPR: case BEGIN:
                    /* Gracefully break.
                     * BEGIN is an error, but it will be caught
                     * below by popping the parens stack. */
                    break;
                case OP:
                    info("invalid query (missing operand between operator and closing parenthesis at pos %ld)", i - str + 1);
                    CLEANUP();
                    return -3;
                case LPR:
                    info("invalid query (missing expression inside parentheses at pos %ld)", i - str + 1);
                    CLEANUP();
                    return -3;
                default:
                    STAMP();
                    error("invalid last_tok %d", last_tok);
                    CLEANUP();
                    return -3;
            }

            /* Find matching open parenthesis */
            switch (stackPop(parens)) {
                case STACK_EMPTY:
                    info("invalid query (unbalanced parentheses)");
                    CLEANUP();
                    return -2;
                case STACK_INTERNAL_ERROR:
                    STAMP();
                    error("stackPop internal error");
                    CLEANUP();
                    return -3;
                default:
                    /* Gracefully break */
                    break;
            }

            SPUSH(tokens, OP_RPR);

            i++;
        } else if (strchr("+-*/%^", *i)) {
            cur_tok = OP;

            /* Catch syntax errors */
            switch (last_tok) {
                case VALUE: case RPR:
                    /* Gracefully break */
                    break;
                case BEGIN:
                    info("invalid query (missing operand before operator at pos %ld)", i - str + 1);
                    CLEANUP();
                    return -3;
                case OP:
                    info("invalid query (missing operand between two operators at pos %ld)", i - str + 1);
                    CLEANUP();
                    return -3;
                case LPR:
                    info("invalid query (missing operand between opening parenthesis and operator at pos %ld)", i - str + 1);
                    CLEANUP();
                    return -3;
                default:
                    STAMP();
                    error("invalid last_tok %d", last_tok);
                    CLEANUP();
                    return -3;
            }

            switch (*i) {
                case '+': SPUSH(tokens, OP_ADD); break;
                case '-': SPUSH(tokens, OP_SUB); break;
                case '*': SPUSH(tokens, OP_MUL); break;
                case '/': SPUSH(tokens, OP_DIV); break;
                case '%': SPUSH(tokens, OP_MOD); break;
                case '^': SPUSH(tokens, OP_POW); break;
                default:
                    STAMP();
                    error("unmatched character '%c'", *i);
                    CLEANUP();
                    return -3;
            }

            i++;
        } else {
            info("invalid query (illegal character '%c')", *i);
            CLEANUP();
            return -2;
        }
    }

    /* Export results */
    *tokens_ptr = tokens;
    *set_ptr = set;

    /* Cleanup */
#undef CLEANUP
#undef SPUSH
    stackFree(parens);

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
                            && (opPrec[-top] > prec || (opPrec[-top] == prec && assoc == OP_ASSOC_LEFT))
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
    size_t matches; /* The number of yet-to-be-found section/value pairs */
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

    /* Reset all query args to BLANK and count expected matches*/
    matches = 0;
    for (i = 0; i < qcount; i++) {
        arglistClear(queries[i]->args);
        matches += queries[i]->args->size;
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

                            matches--;
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

    /* Report not found values */
    if (matches) {
        info("failed to find the following values:");
        for (i = 0; i < qcount; i++) {
            size_t j;
            for (j = 0; j < queries[i]->args->size; j++) {
                const Data *const data = queries[i]->data->data + j; /* cache */

                if (queries[i]->args->data[j].type == ARGVAL_TYPE_NONE) {
                    fprintf(stderr, "->\t%s%s%s\n", data->section, (*data->section)? "." : "", data->key);
                }
            }
        }
        CLEANUP();
        return 4;
    }

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
    } else if (isalnum(*i) || *i == '_' || *i == '-') {
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
        STAMP();
        error("error extracting ArgVal from \"%s\"\n", line);
        ret.type = INI_LINE_INTERROR;
    }

    return ret;
}

int printQueries(const Query **queries, size_t qcount)
{
    ValStack *vstack; /* evaluation stack */
    size_t i, j;

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
                            info("illegal operation on two strings");
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
                                info("cannot multiply a string by %.0g (factor too large)", i2.value.f);
                                valstackFree(vstack);
                                return 3;
                            } else if (i2.value.f > (double)ULONG_MAX / s1 - 1) {
                                info("cannot multiply a string by %.0g (resulting string too long)", i2.value.f);
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
                            i3.value.s[s3] = '\0';
                            break;
                        case OP_ADD: case OP_SUB: case OP_DIV: /* fallthrough */
                        case OP_MOD: case OP_POW:
                            info("illegal operation on a string and a number");
                            valstackFree(vstack);
                            return 3;
                        default:
                            STAMP();
                            error("unmatched operator index (%d)", idx);
                            valstackFree(vstack);
                            return 2;
                    }
                } else {
                    info("illegal operation involving a %s and a %s",
                            (i1.type == ARGVAL_TYPE_STRING)? "string" : "number",
                            (i2.type == ARGVAL_TYPE_STRING)? "string" : "number");
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
                if (result.is_temporary) {
                    free(result.value.s);
                }
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
