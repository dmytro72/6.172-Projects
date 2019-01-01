#include "vector.h"

#include <assert.h>

#define VECTOR_INITIAL_SIZE 120

inline vector* Vector_make() {
    vector *v = malloc(sizeof(vector));
    v->size = VECTOR_INITIAL_SIZE;
    v->data = malloc(sizeof(void*) * VECTOR_INITIAL_SIZE);
    v->empty = malloc(sizeof(void*) * VECTOR_INITIAL_SIZE);
    v->count = 0;
    return v;
}

inline void Vector_append(vector *v, Line *e) {
    // last slot exhausted
    if (v->size == v->count) {
        v->size *= 2;
        v->data = realloc(v->data, sizeof(void*) * v->size);
        v->empty = realloc(v->data, sizeof(void*) * v->size);
    }

    v->data[v->count] = e;
    v->count++;
}

inline void* Vector_get(vector *v, int index) {
    assert(index < v->count);

    return v->data[index];
}

inline void Vector_switch(vector *v) {
    void **t = v->data;
    v->data = v->empty;
    v->empty = t;
    v->count = 0;
}

inline void Vector_free(vector *v) {
    free(v->data);
    // free(v->empty);
    v->data = NULL;
    v->empty = NULL;
    free(v);
    v = NULL;
}