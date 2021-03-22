/** @file
 * Structure for storing and consolidating section/key pairs.
 */

#ifndef DATASET_H
#define DATASET_H

#include <stdlib.h>
#include <limits.h>


/********************************************************
 *                     CONSTANTS                        *
 ********************************************************/

/** The initial capacity of a dataset (will dynamically increase if needed). */
#define DATASET_INIT_CAPACITY 16

/** Returned by some functions in case of an internal error. */
#define DATASET_INTERNAL_ERROR (INT_MIN)



/********************************************************
 *                      TYPEDEFS                        *
 ********************************************************/

/** @cond */
typedef struct Data Data;
typedef struct DataSet DataSet;
/** @endcond */


/********************************************************
 *                     STRUCTURES                       *
 ********************************************************/

/** A single INI location of a piece of data.
 *
 * This struct holds an "address" in the INI format.
 * It is used to reference a single value within a file.
 * The type or existence of this value is not known until
 * access is attempted.
 */
struct Data
{
    /** The name of the section (excluding brackets). */
    char *section;

    /** The name of the key. */
    char *key;
};

/** An ordered set of @ref Data elements.
 *
 * A DataSet is nothing more than an array of @ref Data
 * elements. The goal is to store every section/key pair
 * at most once, so all functions manipulating DataSets
 * will carefully check if a pair already exists before
 * adding a new one, etc.
 */
struct DataSet
{
    /** The array of section/key pairs. */
    Data *data;
    
    /** The number of elements in @ref data. */
    size_t size;

    /** The current max number of elements @ref data
     * can hold (will dynamically increase if needed). */
    size_t capacity;
};


/********************************************************
 *                     FUNCTIONS                        *
 ********************************************************/

/** Allocates a new dataset and returns its address.
 *
 * @returns
 * - valid address - success
 * - @c NULL - failure (malloc)
 */
DataSet *datasetCreate();

/** Adds a new value to a dataset.
 *
 * @returns
 * - index of the element inside the dataset (>=0) - success
 * - -1 - failure (realloc)
 * - @ref DATASET_INTERNAL_ERROR - internal error
 */
int datasetAdd(DataSet *set, const char *section, const char *key);

/** Frees all memory owned by the dataset. */
void datasetFree(DataSet *set);

#endif /* DATASET_H */
