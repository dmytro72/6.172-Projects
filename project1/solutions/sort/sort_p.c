#include "util.h"
/* Function prototypes */

static void merge_p(data_t* __restrict A, int p, int q, int r) ;
static inline void copy_p(data_t* __restrict source,
                          data_t* __restrict dest, int n) ;


/* Function definitions */

/* Basic merge sort */
inline void sort_p(data_t* __restrict A, int p, int r) {
  assert(A) ;
  if (p < r) {
    int q = (p + r) / 2 ;
    sort_p(A, p, q);
    sort_p(A, q + 1, r);
    merge_p(A, p, q, r) ;
  }
}

/* A merge routine. Merges the sub-arrays A [p..q] and A [q + 1..r].
 * Uses two arrays 'left' and 'right' in the merge operation.
 */
static void merge_p(data_t* __restrict A, int p, int q, int r) {
  assert(A) ;
  assert(p <= q) ;
  assert((q + 1) <= r) ;
  int n1 = q - p + 1;
  int n2 = r - q ;

  data_t* __restrict left = 0;
  data_t* __restrict right = 0 ;
  mem_alloc(&left, n1 + 1) ;
  mem_alloc(&right, n2 + 1) ;
  if (left == NULL || right == NULL) {
    mem_free(&left) ;
    mem_free(&right) ;
    return ;
  }

  data_t* __restrict end = A + r ;
  copy_p(A + p, left, n1) ;
  copy_p(A + q + 1, right, n2) ;
  A += p ;
  left [n1] = UINT_MAX ;
  right [n2] = UINT_MAX ;
  data_t le = *left, ri = *right ;
  for (; A <= end ; A++) {
    if (le <= ri) {
      *A = le ;
      le = *++left ;
    } else {
      *A = ri ;
      ri = *++right ;
    }
  }
  left -= n1 ;
  right -= n2 ;
  mem_free(&left) ;
  mem_free(&right) ;
}


static inline void copy_p(data_t* __restrict source,
                          data_t* __restrict dest, int n) {
  assert(source) ;
  assert(dest) ;
  data_t* end = source + n ;
  while (source < end) {
    *dest++ = *source++ ;
  }
}
