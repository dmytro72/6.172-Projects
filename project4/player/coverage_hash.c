

#include "./coverage_hash.h"


RecordLaser laser_table[N_SETS];

// Got this from Stack Overflow
inline uint64_t hash(uint64_t key) {
  key ^= key >> 33;
  key *= 0xff51afd7ed558ccd;
  key ^= key >> 33;
  key *= 0xc4ceb9fe1a85ec53;
  key ^= key >> 33;
  return key;
}

void laser_hashtable_init() {
  for (int i = 0; i < N_SETS; i++) {
    laser_table[i].key = 0;
  }
}

void laser_hashtable_put(uint64_t key, score_t laser_value) {
  uint64_t idx = hash(key) & HASH_MASK;
  
  laser_table[idx].key = key;
  laser_table[idx].laser_value = laser_value;
}


bool laser_hashtable_get(uint64_t key, score_t* laser_value) {
  uint64_t idx = hash(key) & HASH_MASK;

  if (laser_table[idx].key == key) {
    //printf("got it\n");
    *laser_value = laser_table[idx].laser_value;
    return true;
  }
  return false;
}







