#include "array.h"

static inline int array_init(array_t *array, uint32_t n, size_t size)
{
    /*
     * set "array->nelts" before "array->elts", otherwise MSVC thinks
     * that "array->nelts" may be used without having been initialized
     */

    array->nelts = 0;
    array->size = size;
    array->nalloc = n;

    array->elts = calloc(n, size);
    if (array->elts == NULL) {
        return ERROR;
    }

    return OK;
}

array_t *
array_create(uint32_t n, size_t size)
{
    array_t *a;

    a = calloc(1, sizeof(array_t));
    if (a == NULL) {
        return NULL;
    }

    if (array_init(a, n, size) != OK) {
        return NULL;
    }

    return a;
}


void array_destroy(array_t *a)
{
    if(a != NULL){
        if(a->elts != NULL){
            free(a->elts);
            a->elts = NULL;
        }
        free(a);
    }
}


void* array_push(array_t *a)
{
    void        *elt;
    size_t       size;

    if (a->nelts == a->nalloc) {
        /* the array is full */
        size = a->size * a->nalloc;
        a->elts = realloc(a->elts, size * 2);
        a->nalloc *= 2;
    }

    elt = (char*) a->elts + a->size * a->nelts;
    a->nelts++;

    return elt;
}
