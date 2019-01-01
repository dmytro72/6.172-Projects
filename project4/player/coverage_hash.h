
#ifndef COVERAGE_HASH_H
#define COVERAGE_HASH_H

#include <float.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include "./move_gen.h"
#include "./search.h"

#define N_SETS (1 << 23)
#define HASH_MASK (N_SETS-1)

typedef struct RecordLaser {
  uint64_t  key;
  score_t laser_value;
} RecordLaser;

void laser_hashtable_init();
void laser_hashtable_put(uint64_t key, score_t laser_value);
bool laser_hashtable_get(uint64_t key, score_t* laser_value);


#endif



