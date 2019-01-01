// Copyright (c) 2015 MIT License by 6.172 Staff

#ifndef MOVE_GEN_H
#define MOVE_GEN_H

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

// The MAX_NUM_MOVES is just an estimate
#define MAX_NUM_MOVES 1024      // real number = 8 * (3 + 8 + 8 * (7 + 3)) = 728
#define MAX_PLY_IN_SEARCH 100  // up to 100 ply
#define MAX_PLY_IN_GAME 4096   // long game!  ;^)

// Used for debugging and display
#define MAX_CHARS_IN_MOVE 16  // Could be less
#define MAX_CHARS_IN_TOKEN 64

// -----------------------------------------------------------------------------
// Board
// -----------------------------------------------------------------------------

// The board (which is 8x8 or 10x10) is centered in a 16x16 array, with the
// excess height and width being used for sentinels.
#define ARR_WIDTH 16
#define ARR_SIZE (ARR_WIDTH * ARR_WIDTH)

// Board is 8 x 8 or 10 x 10
#define BOARD_WIDTH 8

typedef int square_t;
typedef int rnk_t;
typedef int fil_t;

#define FIL_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)
#define RNK_ORIGIN ((ARR_WIDTH - BOARD_WIDTH) / 2)

#define FIL_SHIFT 4
#define FIL_MASK 15
#define RNK_SHIFT 0
#define RNK_MASK 15

// -----------------------------------------------------------------------------
// Pieces
// -----------------------------------------------------------------------------

#define PIECE_SIZE 5  // Number of bits in (ptype, color, orientation)

typedef uint8_t piece_t;

#define INDEX_SHIFT 5
#define INDEX_MASK 7
#define REVERT_MASK 31

// -----------------------------------------------------------------------------
// Piece types
// -----------------------------------------------------------------------------

#define PTYPE_SHIFT 2
#define PTYPE_MASK 3

typedef enum {
  EMPTY,
  PAWN,
  KING,
  INVALID
} ptype_t;

// -----------------------------------------------------------------------------
// Colors
// -----------------------------------------------------------------------------

#define COLOR_SHIFT 4
#define COLOR_MASK 1

typedef enum {
  WHITE = 0,
  BLACK
} color_t;

// -----------------------------------------------------------------------------
// Orientations
// -----------------------------------------------------------------------------

#define NUM_ORI 4
#define ORI_SHIFT 0
#define ORI_MASK (NUM_ORI - 1)

typedef enum {
  NN,
  EE,
  SS,
  WW
} king_ori_t;

typedef enum {
  NW,
  NE,
  SE,
  SW
} pawn_ori_t;

// -----------------------------------------------------------------------------
// Moves
// -----------------------------------------------------------------------------

// MOVE_MASK is 28 bits
// Representation is PTYPE_MV|ROT|FROM|INTERMEDIATE|TO
// INTERMEDIATE is equal to FROM iff the move is not a double move
#define MOVE_MASK 0xfffffff

#define PTYPE_MV_SHIFT 26
#define PTYPE_MV_MASK 3
#define FROM_SHIFT 16
#define FROM_MASK 0xFF
#define INTERMEDIATE_SHIFT 8
#define INTERMEDIATE_MASK 0xFF
#define TO_SHIFT 0
#define TO_MASK 0xFF
#define ROT_SHIFT 24
#define ROT_MASK 3

typedef uint32_t move_t;
typedef uint64_t sortable_move_t;  // extra bits used to store sort key

typedef uint64_t laser_path_t;

// Rotations
typedef enum {
  NONE,
  RIGHT,
  UTURN,
  LEFT
} rot_t;

// A single move should only be able to zap 1 piece now.
typedef struct victims_t {
  int zapped_count;
  piece_t zapped;
} victims_t;

// returned by make move in illegal situation
#define KO_ZAPPED -1
// returned by make move in ko situation
#define ILLEGAL_ZAPPED -1

// -----------------------------------------------------------------------------
// Position
// -----------------------------------------------------------------------------

// Board representation is square-centric with sentinels.
//
// https://www.chessprogramming.org/Board_Representation
// https://www.chessprogramming.org/Mailbox
// https://www.chessprogramming.org/10x12_Board

typedef struct position {
  piece_t      board[ARR_SIZE];
  struct position*  history;     // history of position
  uint64_t     key;              // hash key
  int          ply;              // Even ply are White, odd are Black
  move_t       last_move;        // move that led to this position
  victims_t    victims;          // pieces destroyed by shooter
  square_t     kloc[2];          // location of kings
  square_t     ploc[2][8];       // location of pawns
} position_t;

// -----------------------------------------------------------------------------
// Function prototypes
// -----------------------------------------------------------------------------

char* color_to_str(color_t c);

// -----------------------------------------------------------------------------
// Piece getters and setters (including color, ptype, orientation)
// -----------------------------------------------------------------------------

// which color is moving next
inline color_t color_to_move_of(position_t* p) {
  return (p->ply & 1) ? BLACK : WHITE;
}

inline color_t color_of(piece_t x) {
  return (color_t)((x >> COLOR_SHIFT) & COLOR_MASK);
}

inline color_t opp_color(color_t c) {
  return (c == WHITE) ? BLACK : WHITE;
}


inline void set_color(piece_t* x, color_t c) {
  *x = ((c & COLOR_MASK) << COLOR_SHIFT) |
       (*x & ~(COLOR_MASK << COLOR_SHIFT));
}


inline ptype_t ptype_of(piece_t x) {
  return (ptype_t)((x >> PTYPE_SHIFT) & PTYPE_MASK);
}

inline void set_ptype(piece_t* x, ptype_t pt) {
  *x = ((pt & PTYPE_MASK) << PTYPE_SHIFT) |
       (*x & ~(PTYPE_MASK << PTYPE_SHIFT));
}

inline int ori_of(piece_t x) {
  return (x >> ORI_SHIFT) & ORI_MASK;
}

inline void set_ori(piece_t* x, int ori) {
  *x = ((ori & ORI_MASK) << ORI_SHIFT) |
       (*x & ~(ORI_MASK << ORI_SHIFT));
}

inline int index_of(piece_t x) {
  return (x >> INDEX_SHIFT) & INDEX_MASK;
}

inline void set_index(piece_t* x, int index) {
  *x = ((index & INDEX_MASK) << INDEX_SHIFT) |
       (*x & ~(INDEX_MASK << INDEX_SHIFT));
}

void init_zob();
uint64_t compute_zob_key(position_t* p);

// -----------------------------------------------------------------------------
// Squares
// -----------------------------------------------------------------------------

// For no square, use 0, which is guaranteed to be off board
inline square_t square_of(fil_t f, rnk_t r) {
  return ARR_WIDTH * (FIL_ORIGIN + f) + RNK_ORIGIN + r;
}

// Finds file of square
inline fil_t fil_of(square_t sq) {
  return ((sq >> FIL_SHIFT) & FIL_MASK) - FIL_ORIGIN;
}

// Finds rank of square
inline rnk_t rnk_of(square_t sq) {
  return ((sq >> RNK_SHIFT) & RNK_MASK) - RNK_ORIGIN;
}

int square_to_str(square_t sq, char* buf, size_t bufsize);

int dir_of(int i);
int beam_of(int direction);
int reflect_of(int beam_dir, int pawn_ori);

// -----------------------------------------------------------------------------
// Move getters and setters
// -----------------------------------------------------------------------------

inline ptype_t ptype_mv_of(move_t mv) {
  return (ptype_t)((mv >> PTYPE_MV_SHIFT) & PTYPE_MV_MASK);
}

inline square_t from_square(move_t mv) {
  return (mv >> FROM_SHIFT) & FROM_MASK;
}

inline square_t intermediate_square(move_t mv) {
  return (mv >> INTERMEDIATE_SHIFT) & INTERMEDIATE_MASK;
}

inline square_t to_square(move_t mv) {
  return (mv >> TO_SHIFT) & TO_MASK;
}

inline rot_t rot_of(move_t mv) {
  return (rot_t)((mv >> ROT_SHIFT) & ROT_MASK);
}

inline move_t move_of(ptype_t typ, rot_t rot, square_t from_sq, square_t int_sq, square_t to_sq) {
  return ((typ & PTYPE_MV_MASK) << PTYPE_MV_SHIFT) |
         ((rot & ROT_MASK) << ROT_SHIFT) |
         ((from_sq & FROM_MASK) << FROM_SHIFT) |
         ((int_sq & INTERMEDIATE_MASK) << INTERMEDIATE_SHIFT) |
         ((to_sq & TO_MASK) << TO_SHIFT);
}

inline uint64_t mask_of(fil_t file, rnk_t rank) {
  return (1ULL << ((file * BOARD_WIDTH) + rank));
}

void move_to_str(move_t mv, char* buf, size_t bufsize);

int generate_all(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict);
int generate_all_with_color(position_t* p, sortable_move_t* sortable_move_list, color_t color_to_move);

int generate_all_with_laser(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict, laser_path_t laser_path);
int generate_all_with_color_with_laser(position_t* p, sortable_move_t* sortable_move_list, color_t color_to_move, laser_path_t laser_path);


void do_perft(position_t* gme, int depth, int ply);
void low_level_make_move(position_t* old, position_t* p, move_t mv);
victims_t make_move(position_t* old, position_t* p, move_t mv);
void display(position_t* p);

// -----------------------------------------------------------------------------
// Ko and illegal move signalling
// -----------------------------------------------------------------------------

inline victims_t KO() {
  return ((victims_t) {
    KO_ZAPPED, 0
  });
}

inline victims_t ILLEGAL() {
  return ((victims_t) {
    ILLEGAL_ZAPPED, 0
  });
}

inline bool is_KO(victims_t victims) {
  return (victims.zapped_count == KO_ZAPPED);
}

inline bool is_ILLEGAL(victims_t victims) {
  return (victims.zapped_count == ILLEGAL_ZAPPED);
}

inline bool zero_victims(victims_t victims) {
  return (victims.zapped_count == 0);
}

inline bool victim_exists(victims_t victims) {
  return (victims.zapped_count > 0);
}

#endif  // MOVE_GEN_H
