#include "util.h"

/* Function prototypes */

void isort(data_t* left, data_t* right) ;
static void merge_m(data_t* __restrict A, int p, int q, int r);
static inline void copy_m(data_t* source, data_t* dest, int n) ;

/* Function definitions */

/* Basic merge sort */
inline void sort_m(data_t* __restrict A, int p, int r) {
  assert(A) ;
  if ((r - p) <= 70) {
    isort(A + p, A + r) ;
  } else {
    int q = (p + r) / 2 ;
    sort_m(A, p, q);
    sort_m(A, q + 1, r);
    merge_m(A, p, q, r) ;
  }
}

/* A merge routine. Merges the sub-arrays A [p..q] and A [q + 1..r].
 * Uses two arrays 'left' and 'right' in the merge operation.
 */
static void merge_m(data_t* __restrict A, int p, int q, int r) {
  assert(A) ;
  assert(p <= q) ;
  assert((q + 1) <= r) ;
  int n1 = q - p + 1;

  data_t* __restrict left = 0 ;
  data_t* __restrict right = A + q + 1 ;
  mem_alloc(&left, n1 + 1) ;
  if (left == NULL) {
    return ;
  }

  data_t* __restrict end = A + r ;
  copy_m(A + p, left, n1) ;
  left [n1] = UINT_MAX ;
  A += p ;

  while (right <= end) {
    int chk = (*left <= *right) ;
    *A++ = *right ^ ((*right ^ *left) & (-chk)) ;
    left += chk ;
    right += 1 - chk ;
  }
  while (A <= end) {
    *A++ = *left++ ;
  }
  left -= n1 ;
  mem_free(&left) ;
}

static inline void copy_m(data_t* source, data_t* dest, int n) {
  assert(source) ;
  assert(dest) ;
  data_t* end = source + n ;
  while (source < end) {
    *dest++ = *source++ ;
  }
}
