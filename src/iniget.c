#include "query.h"
#include "error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

int main(int argc, char **argv)
{
    Query **queries;
    int i;

    if (argc == 1) {
        info("at least 1 query required");
        return EXIT_SUCCESS;
    }

    /* Allocate space for queries */
    if (!(queries = malloc((argc - 1) * sizeof *queries))) {
        info("memory error");
        return EXIT_FAILURE;
    }

    /* Parse queries */
    for (i = 1; i < argc; i++) {
        Query *q;

        if(parseQueryString(&q, argv[i])) {
            free(queries);
            return EXIT_FAILURE;
        }

        queries[i-1] = q;
    }

    /* Run queries */
    runQueries(stdin, (const Query**)queries, argc - 1);

    /* Cleanup */
    for (i = 1; i < argc; i++) {
        queryFree(queries[i - 1]);
    }
    free(queries);

    return EXIT_SUCCESS;
}
