#include "dataset.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>

DataSet *datasetCreate()
{
    DataSet *new;

    if (!(new = malloc(sizeof *new))) {
        info("memory error");
        return NULL;
    }

    new->capacity = DATASET_INIT_CAPACITY;
    if (!(new->data = malloc(new->capacity * sizeof *new->data))) {
        info("memory error");
        free(new);
        return NULL;
    }

    new->size = 0;

    return new;
}

size_t datasetAdd(DataSet *set, const char *section, const char *key)
{
    size_t i;
    size_t size1, size2;

    if (!set) {
        STAMP();
        error("dataset is NULL");
        return DATASET_INTERNAL_ERROR;
    }

    /* Silently quit if the element already exists.
     * This is a very naive search implementation, it would
     * be much better to use a hashmap, but that's
     * a potential goal for the future. Right now I'm
     * keeping it simple.
     */
    for (i = 0; i < set->size; i++) {
        if (strcmp(set->data[i].section, section) == 0 && strcmp(set->data[i].key, key) == 0) {
            return i;
        }
    }

    /* Increase capacity, if needed */
    if (set->size == set->capacity) {
        set->capacity *= 2;
        if (!(set->data = realloc(set->data, set->capacity * sizeof *set->data))) {
            info("memory error");
            return -1;
        }
    }

    /* Allocate buffers for section/key strings */
    size1 = strlen(section) + 1;
    size2 = strlen(key) + 1;
    if (!(set->data[set->size].section = malloc(size1 * sizeof *set->data[set->size].section))) {
        info("memory error");
        return -1;
    }
    if (!(set->data[set->size].key = malloc(size2 * sizeof *set->data[set->size].key))) {
        info("memory error");
        return -1;
    }

    /* Copy section/key to their destination */
    strcpy(set->data[set->size].section, section);
    strcpy(set->data[set->size].key, key);

    return set->size++;
}

void datasetFree(DataSet *set)
{
    size_t i;

    if (!set) {
        STAMP();
        error("dataset is NULL");
        return;
    }

    for (i = 0; i < set->size; i++) {
        free(set->data[i].section);
        free(set->data[i].key);
    }
    free(set->data);
    free(set);
}
