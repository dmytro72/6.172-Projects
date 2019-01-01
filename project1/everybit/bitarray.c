/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

// Implements the ADT specified in bitarray.h as a packed array of bits; a bit
// array containing bit_sz bits will consume roughly bit_sz/8 bytes of
// memory.


#include "./bitarray.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <sys/types.h>


// ********************************* Types **********************************

// Concrete data type representing an array of bits.
struct bitarray {
  // The number of bits represented by this bit array.
  // Need not be divisible by 8.
  size_t bit_sz;

  // The underlying memory buffer that stores the bits in
  // packed form (8 per byte).
  uint64_t* restrict buf;
};

// ******************** Prototypes for static functions *********************

// Portable modulo operation that supports negative dividends.
//
// Many programming languages define modulo in a manner incompatible with its
// widely-accepted mathematical definition.
// http://stackoverflow.com/questions/1907565/c-python-different-behaviour-of-the-modulo-operation
// provides details; in particular, C's modulo
// operator (which the standard calls a "remainder" operator) yields a result
// signed identically to the dividend e.g., -1 % 10 yields -1.
// This is obviously unacceptable for a function which returns size_t, so we
// define our own.
//
// n is the dividend and m is the divisor
//
// Returns a positive integer r = n (mod m), in the range
// 0 <= r < m.
static size_t modulo(const ssize_t n, const size_t m);

// Produces a mask which, when ANDed with a byte, retains only the
// bit_index th byte.
//
// Example: bitmask(5) produces the byte 0b00100000.
//
// (Note that here the index is counted from right
// to left, which is different from how we represent bitarrays in the
// tests.  This function is only used by bitarray_get and bitarray_set,
// however, so as long as you always use bitarray_get and bitarray_set
// to access bits in your bitarray, this reverse representation should
// not matter.
static uint64_t bitmask(const size_t bit_index);

// Rotate the bitarray from bit_offset to bit_offset+length exclusive by amount bit_right_amount
static void rotate_shift(bitarray_t* const bitarray,
                  const size_t bit_offset,
                  const size_t bit_length,
                  const size_t bit_right_amount);

// ******************************* Lookup Tables ********************************

static const uint64_t lookup_bits[64] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096,
                                         8192, 16384, 32768, 65536, 131072, 262144, 524288, 1048576,
                                         2097152, 4194304, 8388608, 16777216, 33554432, 67108864,
                                         134217728, 268435456, 536870912, 1073741824, 2147483648,
                                         4294967296, 8589934592, 17179869184, 34359738368, 68719476736,
                                         137438953472, 274877906944, 549755813888, 1099511627776,
                                         2199023255552, 4398046511104, 8796093022208, 17592186044416,
                                         35184372088832, 70368744177664, 140737488355328, 281474976710656,
                                         562949953421312, 1125899906842624, 2251799813685248, 4503599627370496,
                                         9007199254740992, 18014398509481984, 36028797018963968,
                                         72057594037927936, 144115188075855872, 288230376151711744,
                                         576460752303423488, 1152921504606846976, 2305843009213693952,
                                         4611686018427387904, 9223372036854775808ULL};

// Note: assuming little endian, the least sig. i bits of a 64bit unint64 n are equal to
// (2**i-1) & n
// while the most sig. i bits of n are equal to
// (2**64-1 - (2**i-1)) & n

// ******************************* Functions ********************************

bitarray_t* bitarray_new(const size_t bit_sz) {
  // Allocate an underlying buffer of ceil(bit_sz/8) bytes.
  uint64_t* const buf = calloc(1, (bit_sz+7) >> 3);
  if (buf == NULL) {
    return NULL;
  }

  // Allocate space for the struct.
  bitarray_t* const bitarray = malloc(sizeof(struct bitarray));
  if (bitarray == NULL) {
    free(buf);
    return NULL;
  }

  bitarray->buf = buf;
  bitarray->bit_sz = bit_sz;
  return bitarray;
}

static bitarray_t* my_bitarray_new(const size_t bit_sz) {
  // Allocate an underlying buffer of ceil(bit_sz/8) bytes.
  uint64_t* const buf = calloc(1, (bit_sz+7) >> 3);
  if (buf == NULL) {
    return NULL;
  }

  // Allocate space for the struct.
  bitarray_t* const bitarray = malloc(sizeof(struct bitarray));
  if (bitarray == NULL) {
    free(buf);
    return NULL;
  }

  bitarray->buf = buf;
  bitarray->bit_sz = bit_sz;
  return bitarray;
}

void bitarray_free(bitarray_t* const bitarray) {
  if (bitarray == NULL) {
    return;
  }
  free(bitarray->buf);
  bitarray->buf = NULL;
  free(bitarray);
}

size_t bitarray_get_bit_sz(const bitarray_t* const bitarray) {
  return bitarray->bit_sz;
}

inline bool bitarray_get(const bitarray_t* const bitarray, const size_t bit_index) {
  assert(bit_index < bitarray->bit_sz);
  assert(bit_index >= 0);

  // We're storing bits in packed form, 8 per byte.  So to get the nth
  // bit, we want to look at the (n mod 8)th bit of the (floor(n/8)th)
  // byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to produce either a zero byte (if the bit was 0) or a nonzero byte
  // (if it wasn't).  Finally, we convert that to a boolean.
  return (bitarray->buf[bit_index >> 6] & bitmask(bit_index));
}

inline void bitarray_set(bitarray_t* const bitarray,
                  const size_t bit_index,
                  const bool value) {
  assert(bit_index < bitarray->bit_sz);

  // We're storing bits in packed form, 8 per byte.  So to set the nth
  // bit, we want to set the (n mod 8)th bit of the (floor(n/8)th) byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to clear out the bit we're about to set.  We bitwise-or the result
  // with a byte that has either a 1 or a 0 in the correct place.
  bitarray->buf[bit_index >> 6] =
    (bitarray->buf[bit_index >> 6] & ~bitmask(bit_index)) |
    (value ? bitmask(bit_index) : 0);
}

void bitarray_randfill(bitarray_t* const bitarray) {
  // Do big fills
  int64_t *wptr = (int64_t *)bitarray->buf;
  for (int64_t i = 0; i < bitarray->bit_sz/64; i++) {
    wptr[i] = (int64_t)&wptr[i];
  }

  // Fill remaining bytes
  int8_t *bptr = (int8_t *)bitarray->buf;
  int64_t filled_bytes = bitarray->bit_sz/64*8;
  int64_t total_bytes = (bitarray->bit_sz+7)/8;
  for (int64_t j = filled_bytes; j < total_bytes; j++) {
    bptr[j] = (int8_t)&bptr[j];  // Truncate
  }
}

void bitarray_rotate(bitarray_t* const bitarray,
                     const size_t bit_offset,
                     const size_t bit_length,
                     const ssize_t bit_right_amount) {
  assert(bit_offset + bit_length <= bitarray->bit_sz);

  if (bit_length == 0) {
    return;
  }

  rotate_shift(bitarray,
               bit_offset, 
               bit_length,
               modulo(bit_right_amount, bit_length));
}

void rotate_shift(bitarray_t* const bitarray,
                  const size_t bit_offset,
                  const size_t bit_length,
                  const size_t bit_right_amount) {

  uint64_t start = bit_offset;

  uint64_t middle = bit_offset + (bit_length-bit_right_amount);

  uint64_t end = bit_offset + bit_length;

  uint64_t s64 = start/64;
  uint64_t m64 = middle/64;
  uint64_t e64 = end/64;

  bitarray_t* swap_bitarray = my_bitarray_new(bitarray->bit_sz);

  uint64_t i;

  // store the initial part of A [start, (s64+1)*64 ) onto swap
  for (i = start; i < (s64+1)*64 ; i++) {
    bitarray_set(swap_bitarray, i, bitarray_get(bitarray, i));
  } // we can improve here by not doing bit by bit

  // copy last part of A [m64*64, middle ) onto swap
  for (i = m64*64; i < middle; i++) {
    bitarray_set(swap_bitarray, i, bitarray_get(bitarray, i));
  } // we can improve here by not doing bit by bit

  // copy bulk of A onto swap buffer from [(s64+1)*64, m64*64) to [0, m64*64-(s64+1)*64 )
  for (i = s64+1; i < m64; i++) {
    swap_bitarray->buf[i] = bitarray->buf[i];
  }

  //////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////
  // Set all of B into where A used to be from [middle, end) to [start, start+end-middle)
  uint64_t x = (s64+1)*64 - start;
  uint64_t y = (m64+1)*64 - middle;

  if (end - middle <= 64) {
    // Just do bitwise
    for (i = 0; i < end-middle; i++) {
    bitarray_set(bitarray, i+start, bitarray_get(bitarray, i+middle));
    }
  } else {
  // copy the first part of B
  for (i = 0; i < y; i++) {
    bitarray_set(bitarray, i+start, bitarray_get(bitarray, i+middle));
  }

  // copy the bulk of B and remaining
  if (x == y) {
    for (i = m64+1; i < e64; i++) {
      bitarray->buf[s64+1+i - (m64+1)] = bitarray->buf[i];
    }
    // copy remaining B
    for (i = e64*64; i < end; i++) {
      bitarray_set(bitarray, i - middle + start , bitarray_get(bitarray, i));
    }
  }

  if (x > y) {
    bitarray->buf[s64] = ((bitarray->buf[s64] << (x-y)) >> (x-y)) | (bitarray->buf[m64+1] << (64-(x-y))) ;
    for (i = s64+1; i < s64+1 + (e64-m64-1) ; i++) {
      bitarray->buf[i] = (bitarray->buf[i-(s64+1)+m64+1] >> (x-y)) | (bitarray->buf[i-(s64+1)+m64+2] << (64-(x-y)));
    }
    // copy remaining B
    for (i = e64*64; i < end; i++) {
      bitarray_set(bitarray, i+start-middle, bitarray_get(bitarray, i));
    }

  }

  if (y > x) {
    bitarray->buf[s64+1] = ( (bitarray->buf[s64+1] << (64-(y-x))) >> (64-(y-x)) ) | bitarray->buf[m64+1] << (y-x);
    for (i = s64+2; i < s64+2 + (e64-m64-1) ; i++) {      
      bitarray->buf[i] = (bitarray->buf[i-(s64+2)+m64+1] >> (64-(y-x))) | (bitarray->buf[i-(s64+2)+m64+2] << (y-x));
    } 
    // copy remaining B
    for (i = e64*64; i < end; i++) {
      bitarray_set(bitarray, i - middle + start, bitarray_get(bitarray, i));
    }
  }
  } // end else clause dealing with transfer of B
  //////////////////////////////////////////////////////////////////////////////////////

  // copy all of A back from buffers [start,middle) to A's [start+end-middle,end)
  uint64_t newMid = start+end-middle;
  uint64_t new64 = newMid/64;
  uint64_t z = (new64+1)*64 - newMid;

  uint64_t w = (e64+1)*64 - end;

  if (middle-start <= 64 || end-newMid <= 64) {
    for (i = 0; i < middle-start; i++) {
    bitarray_set(bitarray, newMid+i, bitarray_get(swap_bitarray, i+start));
    }
    bitarray_free(swap_bitarray);
    return;
  }

  // copy first part of A
  for (i = 0; i < x; i++) {
    bitarray_set(bitarray, newMid+i, bitarray_get(swap_bitarray, i+start));
  }

  if (x == z) {
    // copy bulk of A
    for (i = s64+1; i < m64; i++) {
      bitarray->buf[new64+1+i - (s64+1)] = swap_bitarray->buf[i];
    }
    // copy end of A
    for (i = m64*64; i < middle; i++) { // the end of A should be of length 64-y
      bitarray_set(bitarray, newMid+(m64*64-start)+i-m64*64, bitarray_get(swap_bitarray, i));
    }
  }

  if (z > x) {
    // copy bulk of A
    bitarray->buf[new64] = ((bitarray->buf[new64] << (z-x)) >> (z-x)) | (swap_bitarray->buf[s64+1] << (64-(z-x)));

    for (i = new64+1; i < e64; i++) {
      bitarray->buf[i] = (swap_bitarray->buf[i-(new64+1)+s64+1] >> (z-x)) | (swap_bitarray->buf[i-(new64+1)+s64+2] << (64-(z-x)));
    }

    for (i = 0; i < (64-w); i++) {
      bitarray_set(bitarray, e64*64+i, bitarray_get(swap_bitarray, middle-(64-w)+i));
    }
  }

  if (z < x) {
    // copy bulk of A
    bitarray->buf[new64+1] = ((bitarray->buf[new64+1] << (64-(x-z))) >> (64-(x-z))) | (swap_bitarray->buf[s64+1] << (x-z));

    for (i = new64+2; i < e64; i++) {
       bitarray->buf[i] = (swap_bitarray->buf[i-(new64+2)+s64+1] >> (64-(x-z))) | (swap_bitarray->buf[i-(new64+2)+s64+2] << (x-z));
    }

    for (i = 0; i < (64-w); i++) {
      bitarray_set(bitarray, e64*64+i, bitarray_get(swap_bitarray, middle-(64-w)+i));
    }
  }
  bitarray_free(swap_bitarray);
}

static inline size_t modulo(const ssize_t n, const size_t m) {
  const ssize_t signed_m = (ssize_t)m;
  assert(signed_m > 0);
  const ssize_t result = ((n % signed_m) + signed_m) % signed_m;
  assert(result >= 0);
  return (size_t)result;
}

static inline uint64_t bitmask(const size_t bit_index) {
  return lookup_bits[bit_index & 0x3F];
}

