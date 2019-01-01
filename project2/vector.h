#ifndef VECTOR_H__
#define VECTOR_H__

#include <stdlib.h>
#include "line.h"
#include "types.h"

vector* Vector_make();

void Vector_append(vector*, Line*);

void* Vector_get(vector*, int);

void Vector_switch(vector *v);

void Vector_free(vector*);

#endif