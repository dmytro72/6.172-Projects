/**
 * Copyright (c) 2015 MIT License by 6.172 Staff
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALIN  GS
 * IN THE SOFTWARE.  
 **/  
  
#include <stdio.h>  
#include <stdint.h>  
#include <stdlib.h>  
#include <string.h>  
#include "./allocator_interface.h"  
#include "./memlib.h"  
#include <stdbool.h>  
  
// Don't call libc malloc!  
#define malloc(...) (USE_MY_MALLOC)  
#define free(...) (USE_MY_FREE)  
#define realloc(...) (USE_MY_REALLOC)  
#define HEADER_TYPE uint32_t
#define BLOCK_SIZE_THRESH (1ULL << 12)
//#define PRINT_REALLOC
//#define PRINT
//#define DEBUG
void print_heap(bool show_address);

// All blocks must have a specified minimum alignment.  
// The alignment requirement (from config.h) is >= 8 bytes.  
#ifndef ALIGNMENT  
  #define ALIGNMENT 8  
#endif  
  
// Rounds up to the nearest multiple of ALIGNMENT.  
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~(ALIGNMENT-1))

typedef struct free_node_t {
  struct free_node_t* next;
  struct free_node_t* prev;
} free_node_t;

// #define MIN_SIZE (sizeof(free_node_t) + HEADER_T_SIZE + FOOTER_T_SIZE)
#define MIN_SIZE (32)
  
#define CLEAN_BIT (1ULL)

#define NODE_BIT (1ULL << 1)
  
// The smallest aligned size that will hold a size_t value.  
#define HEADER_T_SIZE (sizeof(HEADER_TYPE))
#define FOOTER_T_SIZE (sizeof(HEADER_TYPE))
  
#define TO_VALUE(power) (1ULL << power)

#define MAX(x,y) (x ^ ((x ^ y) & -(x < y)))

HEADER_TYPE get_block_size_head(void* hp) {
  assert((*((HEADER_TYPE*)hp) & ~(CLEAN_BIT | NODE_BIT) ) >= MIN_SIZE);
  return *((HEADER_TYPE*)hp) & ~(CLEAN_BIT | NODE_BIT );
}

// expects a head pointer
#define get_footer_from_head(hp) ((char*)hp + (get_block_size_head(hp) - FOOTER_T_SIZE))
#define set_footer_size(p, n) (*((HEADER_TYPE*)get_footer_from_head(p)) = n)

void set_block_size_head(void* hp, uint64_t n) {
  *((HEADER_TYPE*)hp) = n | ( ( (*((HEADER_TYPE*)hp) & CLEAN_BIT) ) & NODE_BIT );
  set_footer_size(hp, n);
  assert(get_block_size_head(hp) == *((HEADER_TYPE*)get_footer_from_head(hp)));
}

// expects a data pointer
#define get_block_size_data(hp) (get_block_size_head((char*)hp - HEADER_T_SIZE))
#define set_block_size_data(hp, n) (set_block_size_head((char*)hp - HEADER_T_SIZE, n))

// expects a head pointer
#define get_prev_block_size(p)  (*((HEADER_TYPE*)((char*)p - FOOTER_T_SIZE)))
#define set_prev_block_size(p, n) (set_block_size_head((char*)p - get_prev_block_size(p), n))

//assumes p is at very start of block (before size)
void* next_block(void * p){
    assert(get_block_size_head(p) != 0);
    return ((char*) p) + get_block_size_head(p);
}

//assumes p is at very start of block (before size)
void* prev_block(void * p){
    assert(get_prev_block_size(p) != 0);
    return ((char*) p) - get_prev_block_size(p);
}

void remove_block(void* p);

// expects a data pointer
#define is_dirty(p)  (*((HEADER_TYPE*)(((char*) p) - HEADER_T_SIZE)) & CLEAN_BIT)
#define set_clean(p) (*((HEADER_TYPE*)(((char*) p) - HEADER_T_SIZE)) &= ~CLEAN_BIT)
#define set_dirty(p) (*((HEADER_TYPE*)(((char*) p) - HEADER_T_SIZE)) |= CLEAN_BIT)

//assumes p is at very start of block (before size)
#define join(p1, p2) set_block_size_head(p1, get_block_size_head(p1) + get_block_size_head(p2))

uint64_t start;
uint64_t FREES;

#ifdef DEBUG
  ACTIONS = 0;
#endif

int JOINS = 0;

void* coalesce_neighbors(void* p);
void* allocate_from_top(size_t size);

void* get_top() {
  void* last_block_footer = ((char*)mem_heap_hi() - 3);
  uint32_t last_block_size = *(uint32_t*)last_block_footer;
  void* last_block_header = (char*)last_block_footer - last_block_size + HEADER_T_SIZE;

  return last_block_header;
}

uint64_t round_down(size_t v){
  uint64_t r = 0;
  uint64_t shift = 0;
  r =     (v > 0xFFFF) << 4; v >>= r;
  shift = (v > 0xFF  ) << 3; v >>= shift; r |= shift;
  shift = (v > 0xF   ) << 2; v >>= shift; r |= shift;
  shift = (v > 0x3   ) << 1; v >>= shift; r |= shift;
                                          r |= (v >> 1);
  return r;
}

// #define MIN_POWER (round_up(MIN_SIZE))
#define MIN_POWER (4)
#define MAX_POWER (31)

#define is_list(p) (p < FREE_LIST_END)

#define NUM_BINS (MAX_POWER - MIN_POWER + 1)

free_node_t FREE_LIST[NUM_BINS];

#define FREE_LIST_END ((free_node_t*)FREE_LIST + NUM_BINS)

#define POWER_TWO(value) ((value == 0)^(value == (value & -value)))
// #define POWER_TWO(v) (v && !(v & (v - 1)))

#define round_up(n) (round_down(n) + 1 - POWER_TWO(n))

uint64_t bin_index(free_node_t* bin) {
  assert(bin - (free_node_t*)FREE_LIST >= 0);
  return (bin - (free_node_t*)FREE_LIST);
}

// check - This checks our invariant that the size_t header before every
// block points to either the beginning of the next block, or the end of the
// heap.
int my_check() {
  return 0;
}

// init - Initialize the malloc package.  Called once before any other
// calls are made.  Since this is a very simple implementation, we just
// return success.
int my_init() {
  void* t = mem_heap_hi() + 1;
  uint64_t padding = ALIGN((uint64_t)t) - (uint64_t)t + 4;
  start = ((uint64_t)mem_sbrk(padding)) + 4;

  free_node_t* list = (free_node_t*) FREE_LIST;
  for (int i = 0; i < NUM_BINS; i++) {
    list->prev = NULL;
    list->next = NULL;
    list = (char*)list + sizeof(free_node_t);
  }

  return 0;
}

void print_freelist(){
    free_node_t* curr_list = FREE_LIST;
    free_node_t* prev_list = NULL;
    printf("HEAD -> ");
    uint16_t last_power = MIN_POWER;
    while(curr_list!= NULL && curr_list < FREE_LIST_END && last_power < MAX_POWER){ 
        int num_nodes = 0;
        free_node_t* curr_node = curr_list->next;
        free_node_t* prev_node = curr_list;
        while(curr_node != NULL){
            num_nodes++;
            assert(curr_node->prev == prev_node);
            prev_node = curr_node;
            curr_node = curr_node->next;
        }

        if (num_nodes) printf("(Power: %i  Nodes: %i) ->", last_power, num_nodes);
        prev_list = curr_list;
        curr_list++;
        last_power++;
    }
    printf(" END\n\n");
}

free_node_t* find_bin(uint64_t power) {
  assert(MIN_POWER <= power && power <= MAX_POWER);
  return &FREE_LIST[power - MIN_POWER];
}

void insert_to_bin(free_node_t* bin, void* p) {
  assert(bin->prev == NULL);
  assert(p != NULL);
  free_node_t* node = p;

  if (bin->next == NULL) {
    bin->next = node;
    node->next = NULL;
    node->prev = bin;
    return;
  }

  free_node_t* head = bin->next;
  assert(head != NULL);

  // TODO fix?
  while (head->next != NULL && get_block_size_data(head) < get_block_size_data(node)) {
    head = head->next;
  }

  assert(head != NULL);
  
  bin->next  = node;
  node->next = head;
  if (head->next) head->next->prev = head;
  head->prev = node;
  node->prev = bin;
}

void insert_free_node(uint64_t power, void * p){
    #ifdef PRINT
        printf("Inserting: power: %i    --- ", power);
        print_freelist();
    #endif

    free_node_t* bin = find_bin(power);
    insert_to_bin(bin, p);
}

void* slice_free_node(free_node_t * node, uint64_t required_size){
    // invariant: p always points to beginning of block (not beginning of data)
    void* p = (char*)node - HEADER_T_SIZE; //shift over to start of block

    uint64_t contained_size = get_block_size_head(p);
    assert(contained_size >= required_size);

    uint64_t new_size =  contained_size - required_size;
    assert(new_size % ALIGNMENT == 0);
    assert(required_size % ALIGNMENT == 0);

    if (new_size <= MIN_SIZE) return node;

    //initialize header with smaller size
    set_block_size_head(p, new_size);

    void* next_ptr = next_block(p);
    set_block_size_head(next_ptr, required_size);
    set_clean((char*)p + HEADER_T_SIZE);
    //insert the node
    insert_free_node(round_down(new_size), (char *)p + HEADER_T_SIZE);

    assert(start <= p && p <= mem_heap_hi());

    //shift p back to begining of data
    next_ptr = (char*)next_ptr + HEADER_T_SIZE;

    assert(get_block_size_data(next_ptr) >= MIN_SIZE);
    assert(get_block_size_data(next_ptr) % ALIGNMENT == 0);
    assert(get_block_size_data(next_ptr) >= required_size);
    return next_ptr;

    // //initialize header with smaller size
    // set_block_size_head(p, required_size);

    // void* next_ptr = next_block(p);
    // set_block_size_head(next_ptr, new_size);
    // set_clean((char*)next_ptr + HEADER_T_SIZE);
    // //insert the node
    // insert_free_node(round_down(new_size), (char *)next_ptr + HEADER_T_SIZE);

    // assert(start <= p && p <= mem_heap_hi());

    // //shift p back to begining of data
    // p = (char*)p + HEADER_T_SIZE;

    // assert(get_block_size_data(p) >= MIN_SIZE);
    // assert(get_block_size_data(p) % ALIGNMENT == 0);
    // return p;
}

void * allocate_from_top(size_t size){
    void * p = NULL;
    assert(size % ALIGNMENT == 0);
    assert(size >= MIN_SIZE);

    void* last_block = get_top();
    if (is_dirty((char*)last_block + HEADER_T_SIZE) || (char*)last_block - (char*)mem_heap_lo() <= HEADER_T_SIZE) {
        p = mem_sbrk(size);
        set_block_size_head(p, size);
        return (char*)p + HEADER_T_SIZE;
    }
    remove_block(last_block);
    if (get_block_size_head(last_block) >= size) {
      return slice_free_node(last_block, size);
    }
    // extend the top
    mem_sbrk(size - get_block_size_head(last_block));
    set_block_size_head(last_block, size);
    return (char*)last_block + HEADER_T_SIZE;
}

free_node_t* pop_node_from_bin(free_node_t* bin) {
  assert(is_list(bin));
  assert(bin->next != NULL);

  free_node_t* to_return = bin->next;
  bin->next = bin->next->next;
  if (bin->next) bin->next->prev = bin;

  return to_return;
}

free_node_t* pop_node_from_bin_same_power(free_node_t* bin, size_t size) {
  assert(is_list(bin));
  assert(bin->next != NULL);

  free_node_t* head = bin->next;
  while (head != NULL) {
    if (get_block_size_data(head) >= size) break;
    head = head->next;
  }

  if (head == NULL) return NULL;
  head->prev->next = head->next;
  if (head->next) head->next->prev = head->prev;

  return head;
}

free_node_t* find_next_nonempty_bin(free_node_t* bin) {
  bin++;
  while (bin->next == NULL && bin <= FREE_LIST_END) {
      bin++;
  }

  return is_list(bin) ? bin : NULL;
}

void * pop_free_list(size_t size){
  uint64_t power = round_down(size);
  free_node_t* bin = find_bin(power);

  if (bin->next != NULL) {
    free_node_t* p = pop_node_from_bin_same_power(bin, size);
    if (p != NULL) {
      return p;
    }
  }

  bin = find_next_nonempty_bin(bin);
  assert(bin == NULL || bin->next != NULL);

  return (bin == NULL) ? allocate_from_top(size) : slice_free_node(pop_node_from_bin(bin), size);
}

//  malloc - Allocate a block by incrementing the brk pointer.
//  Always allocate a block whose size is a multiple of the alignment.
void* my_malloc(size_t size) {
  #ifdef DEBUG
    ACTIONS++;
  #endif

  size_t aligned_size = MAX(ALIGN(size + HEADER_T_SIZE + FOOTER_T_SIZE), MIN_SIZE);

  void* p = pop_free_list(aligned_size);

  set_dirty(p);
  assert(is_dirty(p));
  return p;
}

// free - Freeing a block does nothing.
void my_free(void* p) {
  #ifdef DEBUG
    ACTIONS++;
    FREES++;
  #endif
  /* add to free list with different size now! */
  set_clean(p);
  assert(!is_dirty(p));
  assert(!is_list(p));

  p = coalesce_neighbors((char*)p - HEADER_T_SIZE);
  uint64_t power = MAX(round_down(get_block_size_data(p)), MIN_POWER); //lower bounds the power
  insert_free_node(power, p);
}

//#define PRINT_REALLOC
// realloc - Implemented simply in terms of malloc and free
void * my_realloc(void *ptr, size_t size) {
  #ifdef DEBUG
    ACTIONS++;
  #endif
  #ifdef PRINT_REALLOC
  printf("reallocating...%ld to size: %i\n", ptr, size);
  #endif

  size_t aligned_size = MAX(ALIGN(size + HEADER_T_SIZE + FOOTER_T_SIZE), MIN_SIZE);
  
  if(get_block_size_data(ptr) >=  aligned_size){
    // remove_block((char*)ptr - HEADER_T_SIZE);
    ptr = slice_free_node(ptr, aligned_size);
    return ptr;
  }

  ptr = (char*)ptr - HEADER_T_SIZE;
  void * next_block_data = (char*)next_block(ptr) + HEADER_T_SIZE;
  while (next_block(ptr) < mem_heap_hi() - HEADER_T_SIZE 
         && !is_dirty(next_block_data)
         &&  get_block_size_head(ptr) <  aligned_size) {
    void* t = next_block(ptr);
    join(ptr, t);
    remove_block(t);
    next_block_data = (char*)next_block(ptr) + HEADER_T_SIZE;
  }
  
  ptr = (char*)ptr + HEADER_T_SIZE;

  if(get_block_size_data(ptr) >= aligned_size){
      // remove_block((char*)ptr - HEADER_T_SIZE);
      ptr = slice_free_node(ptr, aligned_size);
      return ptr;
  }

  // if((char*)ptr + get_block_size_data(ptr) >= mem_heap_hi()-1){
  //     mem_sbrk(aligned_size - get_block_size_data(ptr));
  //     set_block_size_data(ptr, aligned_size);
  //     return ptr;
  // }
    
  void* new_ptr = my_malloc(size);
  memcpy(new_ptr, ptr, size);
  my_free(ptr);

  return new_ptr;
}

//assumes p is at very start of block (before size)
void* coalesce_neighbors(void* p) {
  #ifdef DEBUG
    bool coalesced = false;
  #endif
  void* next_block_data = (char*)next_block(p) + HEADER_T_SIZE;
  if (next_block(p) < mem_heap_hi() - HEADER_T_SIZE 
         && !is_dirty(next_block_data)) {
    #ifdef DEBUG
      // print_heap(0);
      coalesced = true;
    #endif
    void* t = next_block(p);
    join(p, t);
    remove_block(t);
    #ifdef DEBUG
      coalesced = true;
    #endif
  }

  if (p > start && !is_dirty((char*)prev_block(p) + HEADER_T_SIZE)){
    #ifdef DEBUG
      coalesced = true;
    #endif
    void* t = prev_block(p);
    remove_block(t);
    join(t, p);
    p = t;
    #ifdef DEBUG
      coalesced = true;
    #endif
  }

  return (char*)p + HEADER_T_SIZE;
}

//assumes p is at very start of block (before size)
void remove_block(void* p) {
  free_node_t* node = (char*)p + HEADER_T_SIZE;
  assert(node->prev != NULL);

  node->prev->next = node->next;
  if (node->next) node->next->prev = node->prev;
}

void print_heap(bool show_address) {
  printf("------ PRINTING HEAP ------ \n \n ");
  FREES=0;
  void* cur = start;
  void* next = next_block(cur);
  long blocks = 1;
  while (next < mem_heap_hi() - HEADER_T_SIZE){
    if(blocks%10 == 0) printf("\n");
    printf("|");
    ++ blocks;
    next = next_block(cur);

    if(is_dirty((char*)cur + HEADER_T_SIZE)){
        printf("#|");
    }
    else{
        printf("-|");
    }
    if (show_address) {
      printf("%ld|| %ld", get_block_size_head(cur), ((uint64_t)cur) % 10000000);
    } else {
      printf("%ld|| ", get_block_size_head(cur));
    }
    cur = next;

  }
  printf(" from last to mem_heap_hi %ld\n", (void*)mem_heap_hi() - next);
  printf(" mem_heap_hi %ld\n", (void*)mem_heap_hi());
  printf(" cur  %ld\n", cur);
  printf(" prev  %ld\n", prev_block(cur));
  printf(" TOTAL_BLOCKS: %ld\n\n", blocks);
}
