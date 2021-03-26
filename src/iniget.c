#include "query.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

void help(void);

int main(int argc, char **argv)
{
    Query **queries;
    int i, ret;
    FILE *input;

    /* Parse command-line options */
    if (argc < 2) {
        info("try 'iniget --help' for more information.");
        return 0;
    }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        help();
        return 0;
    }
    
    /* Determine input stream */
    if (strcmp(argv[1], "-") == 0) {
        input = stdin;
    } else {
        if (!(input = fopen(argv[1], "r"))) {
            info("failed to open file");
            return 1;
        }
    }
    if (argc < 3) {
        /* No queries to run */
        fclose(input);
        return 0;
    }

    /* Allocate space for queries */
    if (!(queries = malloc((argc - 2) * sizeof *queries))) {
        info("memory error");
        return EXIT_FAILURE;
    }

    /* Parse queries */
    for (i = 2; i < argc; i++) {
        Query *q;

        if (parseQueryString(&q, argv[i])) {
            while (i-- > 2) {
                queryFree(queries[i-2]);
            }
            free(queries);
            return EXIT_FAILURE;
        }

        queries[i-2] = q;
    }

    /* Run queries */
    ret = runQueries(input, (const Query**)queries, argc - 2);

    /* Cleanup */
    fclose(input);
    for (i = 2; i < argc; i++) {
        queryFree(queries[i - 2]);
    }
    free(queries);

    return ret;
}

void help(void)
{
    printf("%s%s%s%s", 
"NAME\n"
"       iniget - extract information from INI files\n"
"\n"
"SYNOPSIS\n"
"       iniget [OPTION] [FILE] [QUERY]...\n"
"\n"
"DESCRIPTION\n"
"       Intakes a path to a file (or - for stdin) and\n"
"       evaluates any number of queries on that file\n"
"       (query syntax is explained in detail below).\n"
"\n"
"OPTIONS\n"
"       -h, --help\n"
"           Prints this help message.\n"
"\n",
"QUERY SYNTAX\n"
"       Each query is a mathematical expression built\n"
"       from operands and operators. Operands are values\n"
"       taken directly from the INI file, passed in the\n"
"       following format:\n"
"           {section.key}   (section can be omitted)\n"
"\n"
"       Value type is assumed based on the value itself,\n"
"       for example \"abc\" would be treated as a string,\n"
"       while \"15\" or \".999\" as a number.\n"
"\n",
"       Operators can be one of the following:\n"
"           +   (addition)\n"
"           -   (subtraction)\n"
"           *   (multiplication, can be implicit)\n"
"           /   (division)\n"
"           %   (modulus)\n"
"           ^   (exponentiation)\n"
"\n",
"       Operator precedence and associativity should be\n"
"       intuitive. Order of operation can be forced\n"
"       by enclosing a subexpression in parentheses.\n"
"       All operators work as expected on numbers.\n"
"       In addition, string concatenation is available\n"
"       by the use of '+', and multiplying a string by\n"
"       a non-negative number is also supported (causes\n"
"       a string to be repeated N times).\n"
"\n"
            );
}
