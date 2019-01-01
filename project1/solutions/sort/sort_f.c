#include "util.h"

/* Function prototypes */

void isort(data_t* left, data_t* right) ;
static void merge_f(data_t* __restrict A, int p, int q, int r,
                    data_t* __restrict left);
static inline void copy_f(data_t* source, data_t* dest, int n) ;
static inline void sort_f_local(data_t* __restrict A, int p, int r,
                                data_t* __restrict left) ;

/* Function definitions */

/* Basic merge sort */
inline void sort_f(data_t* __restrict A, int p, int r) {
  assert(A) ;
  if (r > p) {
    int n1 = (r - p + 1) / 2  ;
    data_t* __restrict left = 0 ;
    mem_alloc(&left, n1 + 1) ;
    if (left == NULL) {
      return ;
    }
    sort_f_local(A, p, r, left) ;
    mem_free(&left) ;
  }
}

/* Internal sort function */
static inline void sort_f_local(data_t* __restrict A, int p, int r,
                                data_t* __restrict left) {
  assert(A) ;
  if ((r - p) <= 70) {
    isort(A + p, A + r) ;
  } else {
    int q = (p + r) / 2 ;
    sort_f_local(A, p, q, left);
    sort_f_local(A, q + 1, r, left);
    merge_f(A, p, q, r, left) ;
  }
}

/* A merge routine. Merges the sub-arrays A [p..q] and A [q + 1..r].
 * Uses two arrays 'left' and 'right' in the merge operation.
 */
static void merge_f(data_t* __restrict A, int p, int q, int r,
                    data_t* __restrict left) {
  assert(A) ;
  assert(p <= q) ;
  assert((q + 1) <= r) ;
  int n1 = q - p + 1;

  data_t* __restrict right = A + q + 1 ;

  copy_f(A + p, left, n1) ;
  left [n1] = UINT_MAX ;

  data_t* __restrict end = A + r ;
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
}

static inline void copy_f(data_t* source, data_t* dest, int n) {
  assert(source) ;
  assert(dest) ;
  data_t* end = source + n ;
  while (source < end) {
    *dest++ = *source++ ;
  }
}
