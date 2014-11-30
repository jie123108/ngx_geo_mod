#ifndef __ARRAY_H_INCLUDED__
#define __ARRAY_H_INCLUDED__
#include <stdint.h>
#include <stdlib.h>

#define OK 0
#define ERROR -1
#define memzero(p, size) memset(p, 0, size)

typedef struct {
    void        *elts;
    uint32_t   nelts;
    size_t       size;
    uint32_t   nalloc;
} array_t;


array_t *array_create(uint32_t n, size_t size);
void array_destroy(array_t *a);
void *array_push(array_t *a);
void *array_push_n(array_t *a, uint32_t n);

#endif
