#include "util.h"

void mem_alloc(data_t* __restrict* space, int size) {
  *space = (data_t*) malloc(sizeof(data_t) * size) ;
  if (*space == NULL) {
    printf("out of memory...\n") ;
  }
}


void mem_free(data_t* __restrict* space) {
  free(*space) ;
  *space = 0 ;
}

