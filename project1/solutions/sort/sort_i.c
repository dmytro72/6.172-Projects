#include "util.h"
/* Function prototypes */

static void merge_i(data_t* A, int p, int q, int r)  ;
static inline void copy_i(data_t* source, data_t* dest, int n)  ;

/* Function definitions */

/* Basic merge sort */
inline void sort_i(data_t* A, int p, int r) {
  assert(A) ;
  if (p < r) {
    int q = (p + r) / 2 ;
    sort_i(A, p, q);
    sort_i(A, q + 1, r);
    merge_i(A, p, q, r) ;
  }
}

/* A merge routine. Merges the sub-arrays A [p..q] and A [q + 1..r].
 * Uses two arrays 'left' and 'right' in the merge operation.
 */
static void merge_i(data_t* A, int p, int q, int r) {
  assert(A) ;
  assert(p <= q) ;
  assert((q + 1) <= r) ;
  int n1 = q - p + 1;
  int n2 = r - q ;

  data_t* left = 0, * right = 0 ;
  mem_alloc(&left, n1 + 1) ;
  mem_alloc(&right, n2 + 1) ;
  if (left == NULL || right == NULL) {
    mem_free(&left) ;
    mem_free(&right) ;
    return ;
  }

  copy_i(&(A [p]), left, n1) ;
  copy_i(&(A [q + 1]), right, n2) ;
  left [n1] = UINT_MAX ;
  right [n2] = UINT_MAX ;

  int i = 0 ;
  int j = 0 ;
  int k = p ;
  data_t le = left [i], ri = right [j] ;
  for (; k <= r ; k++) {
    if (le <= ri) {
      A [k] = le ;
      le = left [++i] ;
    } else {
      A [k] = ri ;
      ri = right [++j] ;
    }
  }

  mem_free(&left) ;
  mem_free(&right) ;
}

static inline void copy_i(data_t* source, data_t* dest, int n) {
  assert(dest) ;
  assert(source) ;
  int i ;
  for (i = 0 ; i < n ; i++) {
    dest [i] = source [i] ;
  }
}
