// Copyright (c) 2015 MIT License by 6.172 Staff

#include "./move_gen.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "./eval.h"
#include "./fen.h"
#include "./search.h"
#include "./tbassert.h"
#include "./util.h"

#define MAX(x, y)  ((x) > (y) ? (x) : (y))
#define MIN(x, y)  ((x) < (y) ? (x) : (y))

int USE_KO;  // Respect the Ko rule

static char* color_strs[2] = {"White", "Black"};

char* color_to_str(color_t c) {
  return color_strs[c];
}

// -----------------------------------------------------------------------------
// Piece orientation strings
// -----------------------------------------------------------------------------

// King orientations
char* king_ori_to_rep[2][NUM_ORI] = { { "NN", "EE", "SS", "WW" },
  { "nn", "ee", "ss", "ww" }
};

// Pawn orientations
char* pawn_ori_to_rep[2][NUM_ORI] = { { "NW", "NE", "SE", "SW" },
  { "nw", "ne", "se", "sw" }
};

char* nesw_to_str[NUM_ORI] = {"north", "east", "south", "west"};

// -----------------------------------------------------------------------------
// Board hashing
// -----------------------------------------------------------------------------

// Zobrist hashing
//
// https://www.chessprogramming.org/Zobrist_Hashing 
//
// NOTE: Zobrist hashing uses piece_t as an integer index into to the zob table.
// So if you change your piece representation, you'll need to recompute what the
// old piece representation is when indexing into the zob table to get the same
// node counts.
static uint64_t   zob[ARR_SIZE][1 << PIECE_SIZE];
static uint64_t   zob_color;
uint64_t myrand();

uint64_t compute_zob_key(position_t* p) {
  uint64_t key = 0;
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t sq = square_of(f, r);
      key ^= zob[sq][p->board[sq]&REVERT_MASK];
    }
  }
  if (color_to_move_of(p) == BLACK) {
    key ^= zob_color;
  }

  return key;
}

void init_zob() {
  for (int i = 0; i < ARR_SIZE; i++) {
    for (int j = 0; j < (1 << PIECE_SIZE); j++) {
      zob[i][j] = myrand();
    }
  }
  zob_color = myrand();
}

// converts a square to string notation, returns number of characters printed
int square_to_str(square_t sq, char* buf, size_t bufsize) {
  fil_t f = fil_of(sq);
  rnk_t r = rnk_of(sq);
  if (f >= 0) {
    return snprintf(buf, bufsize, "%c%d", 'a' + f, r);
  } else  {
    return snprintf(buf, bufsize, "%c%d", 'z' + f + 1, r);
  }
}

// -----------------------------------------------------------------------------
// Board direction and laser direction
// -----------------------------------------------------------------------------

// direction map
static int dir[8] = { -ARR_WIDTH - 1, -ARR_WIDTH, -ARR_WIDTH + 1, -1, 1,
                      ARR_WIDTH - 1, ARR_WIDTH, ARR_WIDTH + 1
                    };
inline int dir_of(int i) {
  // tbassert(i >= 0 && i < 8, "i: %d\n", i);
  return dir[i];
}


// directions for laser: NN, EE, SS, WW
static int beam[NUM_ORI] = {1, ARR_WIDTH, -1, -ARR_WIDTH};

inline int beam_of(int direction) {
  // tbassert(direction >= 0 && direction < NUM_ORI, "dir: %d\n", direction);
  return beam[direction];
}

// reflect[beam_dir][pawn_orientation]
// -1 indicates back of Pawn
int reflect[NUM_ORI][NUM_ORI] = {
  //  NW  NE  SE  SW
  { -1, -1, EE, WW},   // NN
  { NN, -1, -1, SS},   // EE
  { WW, EE, -1, -1 },  // SS
  { -1, NN, SS, -1 }   // WW
};

inline int reflect_of(int beam_dir, int pawn_ori) {
  // tbassert(beam_dir >= 0 && beam_dir < NUM_ORI, "beam-dir: %d\n", beam_dir);
  // tbassert(pawn_ori >= 0 && pawn_ori < NUM_ORI, "pawn-ori: %d\n", pawn_ori);
  return reflect[beam_dir][pawn_ori];
}

// converts a move to string notation for FEN
void move_to_str(move_t mv, char* buf, size_t bufsize) {
  square_t f = from_square(mv);  // from-square
  square_t i = intermediate_square(mv); //int-square
  square_t t = to_square(mv);    // to-square
  rot_t r = rot_of(mv);          // rotation
  const char* orig_buf = buf;

  buf += square_to_str(f, buf, bufsize);
  if (f != i) {
    buf += square_to_str(i, buf, bufsize - (buf - orig_buf));
  }
  if (i != t) {
    buf += square_to_str(t, buf, bufsize - (buf - orig_buf));
  }

  switch (r) {
    case NONE:
      break;
    case RIGHT:
      buf += snprintf(buf, bufsize - (buf - orig_buf), "R");
      break;
    case UTURN:
      buf += snprintf(buf, bufsize - (buf - orig_buf), "U");
      break;
    case LEFT:
      buf += snprintf(buf, bufsize - (buf - orig_buf), "L");
      break;
    default:
      tbassert(false, "Whoa, now.  Whoa, I say.\n");  // Bad, bad, bad
      break;
  }
}

// -----------------------------------------------------------------------------
// Move generation
// -----------------------------------------------------------------------------

// Original
int original_generate_all(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict) {
  color_t color_to_move = color_to_move_of(p);

  int move_count = 0;

  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t  sq = square_of(f, r);
      piece_t x = p->board[sq];

      ptype_t typ = ptype_of(x);
      color_t color = color_of(x);

      switch (typ) {
      case EMPTY:
        break;
      case PAWN:
      case KING:
        if (color != color_to_move) {  // Wrong color.
          break;
        }
        // directions
        for (int d = 0; d < 8; d++) {
          bool is_swap = false;
          int dest = sq + dir_of(d);
          switch(ptype_of(p->board[dest])) {
            case INVALID:
              // Skip moves into invalid squares
              continue;
              break;
            case EMPTY:
              break;
            case PAWN:
            case KING:
              if(color_of(p->board[dest]) == color){
                // Check that we are not swapping with a friendly piece
                continue;
              } else {
                is_swap = true;
                break;
              }
            default:
              tbassert(false, "Very unfortunate");
              break; // Bad!
          }

          if(!is_swap){
            WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
            WHEN_DEBUG_VERBOSE({
              move_to_str(move_of(typ, (rot_t) 0, sq, (square_t) sq, dest), buf, MAX_CHARS_IN_MOVE);
              DEBUG_LOG(1, "Before: %s ", buf);
            });
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq, dest);

            WHEN_DEBUG_VERBOSE({
              move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
              DEBUG_LOG(1, "After: %s\n", buf);
            });
          }

          // Consider double moves
          if(is_swap) {
            for (int d = 0; d < 8; d++) {
              int final_dest = dest + dir_of(d);
              ptype_t final_type = ptype_of(p->board[final_dest]);
              if(final_dest == sq) {
                continue; // Do not generate a null move
              }
              if (final_type != EMPTY) {
                continue; // Cannot do a double swap
              }

              WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
              WHEN_DEBUG_VERBOSE({
                move_to_str(move_of(typ, (rot_t) 0, sq, dest, final_dest), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "Before: %s ", buf);
              });
              tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
              sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest, final_dest);

              WHEN_DEBUG_VERBOSE({
                move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "After: %s\n", buf);
              });
            }

            // swap-rotates
            for (int rot = 1; rot < 4; ++rot) {
              tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
              sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, dest, dest);
            }
          }
        }

        // rotations - three directions possible
        for (int rot = 1; rot < 4; ++rot) {
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
          sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq, sq);
        }
        break;
      case INVALID:
      default:
        tbassert(false, "Bogus, man.\n");  // Couldn't BE more bogus!
      }
    }
  }

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "\nGenerated moves: ");
    for (int i = 0; i < move_count; ++i) {
      char buf[MAX_CHARS_IN_MOVE];
      move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "%s ", buf);
    }
    DEBUG_LOG(1, "\n");
  });

  return move_count;
}

// Generate all moves from position p.  Returns number of moves.
// strict currently ignored
//
// https://www.chessprogramming.org/Move_Generation
int generate_all(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict) {
  color_t color_to_move = color_to_move_of(p);

  int move_count = 0;

  square_t* pieces = p->ploc[color_to_move];

  for (int i = 0; i < 8; i++) {
    square_t sq = pieces[i];
    if (sq == -1) { continue; }
    piece_t x = p->board[sq];

    ptype_t typ = ptype_of(x);
    color_t color = color_of(x);

    // directions
    for (int d = 0; d < 8; d++) {
      bool is_swap = false;
      int dest = sq + dir_of(d);
      switch(ptype_of(p->board[dest])) {
        case INVALID:
          // Skip moves into invalid squares
          continue;
          break;
        case EMPTY:
          break;
        case PAWN:
        case KING:
          if(color_of(p->board[dest]) == color){
            // Check that we are not swapping with a friendly piece
            continue;
          } else {
            is_swap = true;
            break;
          }
        default:
          tbassert(false, "Very unfortunate");
          break; // Bad!
      }

      if(!is_swap){
        WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
        WHEN_DEBUG_VERBOSE({
          move_to_str(move_of(typ, (rot_t) 0, sq, (square_t) sq, dest), buf, MAX_CHARS_IN_MOVE);
          DEBUG_LOG(1, "Before: %s ", buf);
        });
        tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
        sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq, dest);

        WHEN_DEBUG_VERBOSE({
          move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
          DEBUG_LOG(1, "After: %s\n", buf);
        });
      }

      // Consider double moves
      if(is_swap) {
        for (int d = 0; d < 8; d++) {
          int final_dest = dest + dir_of(d);
          ptype_t final_type = ptype_of(p->board[final_dest]);
          if(final_dest == sq) {
            continue; // Do not generate a null move
          }
          if (final_type != EMPTY) {
            continue; // Cannot do a double swap
          }

          WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
          WHEN_DEBUG_VERBOSE({
            move_to_str(move_of(typ, (rot_t) 0, sq, dest, final_dest), buf, MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "Before: %s ", buf);
          });
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
          sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest, final_dest);

          WHEN_DEBUG_VERBOSE({
            move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "After: %s\n", buf);
          });
        }

        // swap-rotates
        for (int rot = 1; rot < 4; ++rot) {
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
          sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, dest, dest);
        }
      }
    }

    // rotations - three directions possible
    for (int rot = 1; rot < 4; ++rot) {
      tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
      sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq, sq);
    }
  }

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "\nGenerated moves: ");
    for (int i = 0; i < move_count; ++i) {
      char buf[MAX_CHARS_IN_MOVE];
      move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "%s ", buf);
    }
    DEBUG_LOG(1, "\n");
  });

  return move_count;
}

int original_generate_all_with_laser(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict, laser_path_t laser_path) {
  color_t color_to_move = color_to_move_of(p);

  int move_count = 0;
 
  for (fil_t f = 0; f < BOARD_WIDTH; f++) {
    for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
      square_t  sq = square_of(f, r);
      fil_t sq_fil = fil_of(sq);
      rnk_t sq_rnk = rnk_of(sq);

      
      piece_t x = p->board[sq];

      ptype_t typ = ptype_of(x);
      color_t color = color_of(x);

      switch (typ) {
      case EMPTY:
        break;
      case PAWN:
      case KING:
        if (color != color_to_move) {  // Wrong color.
          break;
        }
        // directions
        for (int d = 0; d < 8; d++) {
          bool is_swap = false;
          int dest = sq + dir_of(d);
          switch(ptype_of(p->board[dest])) {
            case INVALID:
              // Skip moves into invalid squares
              continue;
              break;
            case EMPTY:
              break;
            case PAWN:
            case KING:
              if(color_of(p->board[dest]) == color){
                // Check that we are not swapping with a friendly piece
                continue;
              } else {
                is_swap = true;
                break;
              }
            default:
              tbassert(false, "Very unfortunate");
              break; // Bad!
          }

          if(!is_swap){
            WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
            WHEN_DEBUG_VERBOSE({
              move_to_str(move_of(typ, (rot_t) 0, sq, (square_t) sq, dest), buf, MAX_CHARS_IN_MOVE);
              DEBUG_LOG(1, "Before: %s ", buf);
            });
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                              (mask_of(fil_of(dest), rnk_of(dest)) & laser_path);
            if (is_in_path) {
              sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq, dest);
            }

            WHEN_DEBUG_VERBOSE({
              move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
              DEBUG_LOG(1, "After: %s\n", buf);
            });
          }

          // Consider double moves
          if(is_swap) {
            for (int d = 0; d < 8; d++) {
              int final_dest = dest + dir_of(d);
              ptype_t final_type = ptype_of(p->board[final_dest]);
              if(final_dest == sq) {
                continue; // Do not generate a null move
              }
              if (final_type != EMPTY) {
                continue; // Cannot do a double swap
              }

              WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
              WHEN_DEBUG_VERBOSE({
                move_to_str(move_of(typ, (rot_t) 0, sq, dest, final_dest), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "Before: %s ", buf);
              });
              tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
              bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                                (mask_of(fil_of(dest), rnk_of(dest)) & laser_path) ||
                                (mask_of(fil_of(final_dest), rnk_of(final_dest)) & laser_path);
              if (is_in_path) {
                sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest, final_dest);
              }
              WHEN_DEBUG_VERBOSE({
                move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
                DEBUG_LOG(1, "After: %s\n", buf);
              });
            }

            bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                              (mask_of(fil_of(dest), rnk_of(dest)) & laser_path);
            // swap-rotates
            if(is_in_path) {
              for (int rot = 1; rot < 4; ++rot) {
                tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
                sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, dest, dest);
              }
            }
          }
        }
        
        bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path);
        
        // rotations - three directions possible
        if (is_in_path) {
          for (int rot = 1; rot < 4; ++rot) {
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq, sq);
          }
        }
        break;
      case INVALID:
      default:
        tbassert(false, "Bogus, man.\n");  // Couldn't BE more bogus!
      }
    }
  }

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "\nGenerated moves: ");
    for (int i = 0; i < move_count; ++i) {
      char buf[MAX_CHARS_IN_MOVE];
      move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "%s ", buf);
    }
    DEBUG_LOG(1, "\n");
  });

  return move_count;
}
int generate_all_with_laser(position_t* p, sortable_move_t* sortable_move_list,
                 bool strict, laser_path_t laser_path) {
  color_t color_to_move = color_to_move_of(p);

  int move_count = 0;

  square_t* pieces = p->ploc[color_to_move];

  for (int i = 0; i < 8; i++) {
    square_t sq = pieces[i];
    if (sq == -1) { continue; }

    fil_t sq_fil = fil_of(sq);
    rnk_t sq_rnk = rnk_of(sq);
    piece_t x = p->board[sq];

    ptype_t typ = ptype_of(x);
    color_t color = color_of(x);

    // directions
    for (int d = 0; d < 8; d++) {
      bool is_swap = false;
      int dest = sq + dir_of(d);
      switch(ptype_of(p->board[dest])) {
        case INVALID:
          // Skip moves into invalid squares
          continue;
          break;
        case EMPTY:
          break;
        case PAWN:
        case KING:
          if(color_of(p->board[dest]) == color){
            // Check that we are not swapping with a friendly piece
            continue;
          } else {
            is_swap = true;
            break;
          }
        default:
          tbassert(false, "Very unfortunate");
          break; // Bad!
      }

      if(!is_swap){
        WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
        WHEN_DEBUG_VERBOSE({
          move_to_str(move_of(typ, (rot_t) 0, sq, (square_t) sq, dest), buf, MAX_CHARS_IN_MOVE);
          DEBUG_LOG(1, "Before: %s ", buf);
        });
        tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
        bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                          (mask_of(fil_of(dest), rnk_of(dest)) & laser_path);
        if (is_in_path) {
          sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, sq, dest);
        }

        WHEN_DEBUG_VERBOSE({
          move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
          DEBUG_LOG(1, "After: %s\n", buf);
        });
      }

      // Consider double moves
      if(is_swap) {
        for (int d = 0; d < 8; d++) {
          int final_dest = dest + dir_of(d);
          ptype_t final_type = ptype_of(p->board[final_dest]);
          if(final_dest == sq) {
            continue; // Do not generate a null move
          }
          if (final_type != EMPTY) {
            continue; // Cannot do a double swap
          }

          WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
          WHEN_DEBUG_VERBOSE({
            move_to_str(move_of(typ, (rot_t) 0, sq, dest, final_dest), buf, MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "Before: %s ", buf);
          });
          tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
          bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                (mask_of(fil_of(dest), rnk_of(dest)) & laser_path) ||
                (mask_of(fil_of(final_dest), rnk_of(final_dest)) & laser_path);
          if (is_in_path) {
            sortable_move_list[move_count++] = move_of(typ, (rot_t) 0, sq, dest, final_dest);
          }

          WHEN_DEBUG_VERBOSE({
            move_to_str(get_move(sortable_move_list[move_count - 1]), buf, MAX_CHARS_IN_MOVE);
            DEBUG_LOG(1, "After: %s\n", buf);
          });
        }

        bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path) ||
                          (mask_of(fil_of(dest), rnk_of(dest)) & laser_path);

        // swap-rotates
        if(is_in_path) {
          for (int rot = 1; rot < 4; ++rot) {
            tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
            sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, dest, dest);
          }
        }
      }
    }

    bool is_in_path = (mask_of(sq_fil, sq_rnk) & laser_path);

    // rotations - three directions possible
    if (is_in_path) {
      for (int rot = 1; rot < 4; ++rot) {
        tbassert(move_count < MAX_NUM_MOVES, "move_count: %d\n", move_count);
        sortable_move_list[move_count++] = move_of(typ, (rot_t) rot, sq, sq, sq);
      }
    }
  }

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "\nGenerated moves: ");
    for (int i = 0; i < move_count; ++i) {
      char buf[MAX_CHARS_IN_MOVE];
      move_to_str(get_move(sortable_move_list[i]), buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "%s ", buf);
    }
    DEBUG_LOG(1, "\n");
  });

  return move_count;
}


int generate_all_with_color(position_t* p, sortable_move_t* sortable_move_list, 
                 color_t color) {
  color_t color_to_move = color_to_move_of(p);
  if(color_to_move != color){
    p->ply++;
  }

  int num_moves = generate_all(p, sortable_move_list, false);

  if(color_to_move != color){
    p->ply--;
  }

  return num_moves;
}

int generate_all_with_color_with_laser(position_t* p, sortable_move_t* sortable_move_list, 
                 color_t color, laser_path_t laser_path) {
  color_t color_to_move = color_to_move_of(p);
  if(color_to_move != color){
    p->ply++;
  }

  int num_moves = generate_all_with_laser(p, sortable_move_list, false, laser_path);

  if(color_to_move != color){
    p->ply--;
  }

  return num_moves;
}

// -----------------------------------------------------------------------------
// Move execution
// -----------------------------------------------------------------------------

// Returns the square of piece that would be zapped by the laser if fired once,
// or 0 if no such piece exists.
//
// p : Current board state.
// c : Color of king shooting laser.
square_t fire_laser(position_t* p, color_t c) {
  // color_t fake_color_to_move = (color_to_move_of(p) == WHITE) ? BLACK : WHITE;0
  square_t sq = p->kloc[c];
  int bdir = ori_of(p->board[sq]);

  tbassert(ptype_of(p->board[sq]) == KING,
           "ptype: %d\n", ptype_of(p->board[sq]));

  while (true) {
    sq += beam_of(bdir);
    tbassert(sq < ARR_SIZE && sq >= 0, "sq: %d\n", sq);

    switch (ptype_of(p->board[sq])) {
    case EMPTY:  // empty square
      break;
    case PAWN:  // Pawn
      bdir = reflect_of(bdir, ori_of(p->board[sq]));
      if (bdir < 0) {  // Hit back of Pawn
        return sq;
      }
      break;
    case KING:  // King
      return sq;  // sorry, game over my friend!
      break;
    case INVALID:  // Ran off edge of board
      return 0;
      break;
    default:  // Shouldna happen, man!
      tbassert(false, "Like porkchops and whipped cream.\n");
      break;
    }
  }
}

void low_level_make_move(position_t* old, position_t* p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);
  WHEN_DEBUG_VERBOSE({
    move_to_str(mv, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "low_level_make_move: %s\n", buf);
  });

  tbassert(old->key == compute_zob_key(old),
           "old->key: %"PRIu64", zob-key: %"PRIu64"\n",
           old->key, compute_zob_key(old));

  WHEN_DEBUG_VERBOSE({
    fprintf(stderr, "Before:\n");
    display(old);
  });

  square_t from_sq = from_square(mv);
  square_t int_sq = intermediate_square(mv);
  square_t to_sq = to_square(mv);
  rot_t rot = rot_of(mv);

  WHEN_DEBUG_VERBOSE({
    DEBUG_LOG(1, "low_level_make_move 2:\n");
    square_to_str(from_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "from_sq: %s\n", buf);
    square_to_str(int_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "int_sq: %s\n", buf);
    square_to_str(to_sq, buf, MAX_CHARS_IN_MOVE);
    DEBUG_LOG(1, "to_sq: %s\n", buf);
    switch (rot) {
    case NONE:
      DEBUG_LOG(1, "rot: none\n");
      break;
    case RIGHT:
      DEBUG_LOG(1, "rot: R\n");
      break;
    case UTURN:
      DEBUG_LOG(1, "rot: U\n");
      break;
    case LEFT:
      DEBUG_LOG(1, "rot: L\n");
      break;
    default:
      tbassert(false, "Not like a boss at all.\n");  // Bad, bad, bad
      break;
    }
  });

  *p = *old;

  p->history = old;
  p->last_move = mv;

  tbassert(from_sq < ARR_SIZE && from_sq > 0, "from_sq: %d\n", from_sq);
  tbassert(p->board[from_sq] < (1 << PIECE_SIZE) && p->board[from_sq] >= 0,
           "p->board[from_sq]: %d\n", p->board[from_sq]);
  tbassert(to_sq < ARR_SIZE && to_sq > 0, "to_sq: %d\n", to_sq);
  tbassert(p->board[to_sq] < (1 << PIECE_SIZE) && p->board[to_sq] >= 0,
           "p->board[to_sq]: %d\n", p->board[to_sq]);

  p->key ^= zob_color;   // swap color to move

  piece_t from_piece = p->board[from_sq];
  piece_t int_piece = p->board[int_sq];
  piece_t to_piece = p->board[to_sq];
  bool is_double_move = false;

  if (to_sq != from_sq) {  // move, not rotation
    // Hash key updates
    p->key ^= zob[from_sq][from_piece&REVERT_MASK];  // remove from_piece from from_sq
    p->key ^= zob[to_sq][to_piece&REVERT_MASK];  // remove to_piece from to_sq

    // Get nevessary info to update ploc
    int from_index = index_of(from_piece);
    int int_index = index_of(int_piece);
    int to_index = index_of(to_piece);

    color_t from_color = color_of(from_piece);
    color_t int_color = color_of(int_piece);
    color_t to_color = color_of(to_piece);

    ptype_t from_type = ptype_of(from_piece);
    ptype_t int_type = ptype_of(int_piece);
    ptype_t to_type = ptype_of(to_piece);

    square_t* from_sqz = &p->ploc[from_color][from_index];
    square_t* int_sqz = &p->ploc[int_color][int_index];
    square_t* to_sqz = &p->ploc[to_color][to_index];

    square_t junk;

    if (from_type == EMPTY || from_type == INVALID) {
      from_sqz = &junk;
    }

    if (int_type == EMPTY || int_type == INVALID) {
      int_sqz = &junk;
    }

    if (to_type == EMPTY || to_type == INVALID) {
      to_sqz = &junk;
    }

    if(int_sq != from_sq){
      // This is a double move
      is_double_move = true;

      p->key ^= zob[int_sq][int_piece&REVERT_MASK];  // remove int_piece from int_sq

      p->board[from_sq] = int_piece;  // swap from_piece and int_piece on board
      p->board[int_sq] = from_piece;

      // Update ploc
      *int_sqz = from_sq;
      *from_sqz = int_sq;

      if(int_sq != to_sq) {
        // Either a swap-move or a swap-swap
        p->board[int_sq] = to_piece;  // Pieces should move the order from->to, to->int, int->from
        p->board[to_sq] = from_piece;

        // Update ploc
        *to_sqz = int_sq;
        *from_sqz = to_sq;

        p->key ^= zob[int_sq][to_piece&REVERT_MASK];   // place to_piece in int_sq
        p->key ^= zob[to_sq][from_piece&REVERT_MASK];  // place from_piece in to_sq
        p->key ^= zob[from_sq][int_piece&REVERT_MASK]; // place int_piece in from_sq
      } else {
        // A swap-rotate
        // Swap
        p->key ^= zob[from_sq][int_piece&REVERT_MASK]; // Place int_piece in from_sq
        p->key ^= zob[int_sq][from_piece&REVERT_MASK]; // Place from_piece in int_sq

        // And rotate
        p->key ^= zob[int_sq][from_piece&REVERT_MASK];
        set_ori(&from_piece, rot + ori_of(from_piece));  // rotate from_piece
        p->board[int_sq] = from_piece;  // place rotated piece on board
        p->key ^= zob[int_sq][from_piece&REVERT_MASK];              // ... and in hash
      }
    } else {
      p->board[to_sq] = from_piece;  // swap from_piece and to_piece on board
      p->board[from_sq] = to_piece;

      // Update ploc
      *from_sqz = to_sq;
      *to_sqz = from_sq;

      p->key ^= zob[to_sq][from_piece&REVERT_MASK];  // place from_piece in to_sq
      p->key ^= zob[from_sq][to_piece&REVERT_MASK];  // place to_piece in from_sq
    }

    // Update King locations if necessary
    if (ptype_of(from_piece) == KING) {
      p->kloc[color_of(from_piece)] = to_sq;
    }
    if (ptype_of(int_piece) == KING) {
      if (is_double_move) {p->kloc[color_of(int_piece)] = from_sq;}
    }
    if (ptype_of(to_piece) == KING) {
      if (is_double_move) {
        if (int_sq != to_sq) {
          p->kloc[color_of(to_piece)] = int_sq;
        } // The case of a swap-rotate is covered by the previous case.
      } else {
        p->kloc[color_of(to_piece)] = from_sq;
      }
    }

  } else {  // rotation
    // remove from_piece from from_sq in hash
    p->key ^= zob[from_sq][from_piece&REVERT_MASK];
    set_ori(&from_piece, rot + ori_of(from_piece));  // rotate from_piece
    p->board[from_sq] = from_piece;  // place rotated piece on board
    p->key ^= zob[from_sq][from_piece&REVERT_MASK];              // ... and in hash
  }

  // Increment ply
  p->ply++;

  tbassert(p->key == compute_zob_key(p),
           "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
           p->key, compute_zob_key(p));

  WHEN_DEBUG_VERBOSE({
    fprintf(stderr, "After:\n");
    display(p);
  });
}

// return victim pieces or KO
victims_t make_move(position_t* old, position_t* p, move_t mv) {
  tbassert(mv != 0, "mv was zero.\n");

  WHEN_DEBUG_VERBOSE(char buf[MAX_CHARS_IN_MOVE]);

  // move phase 1 - moving a piece
  low_level_make_move(old, p, mv);

  // move phase 2 - shooting the laser
  square_t victim_sq = 0;
  p->victims.zapped_count = 0;

  if ((victim_sq = fire_laser(p, color_to_move_of(old)))) {
    WHEN_DEBUG_VERBOSE({
      square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "Zapping piece on %s\n", buf);
    });

    // we definitely hit something with laser, remove it from board
    piece_t victim_piece = p->board[victim_sq];
    p->victims.zapped_count++;
    p->victims.zapped = victim_piece;
    p->key ^= zob[victim_sq][victim_piece&REVERT_MASK];
    p->board[victim_sq] = 0;
    p->key ^= zob[victim_sq][0];

    color_t victim_color = color_of(victim_piece);
    p->ploc[victim_color][index_of(victim_piece)] = -1;

    tbassert(p->key == compute_zob_key(p),
             "p->key: %"PRIu64", zob-key: %"PRIu64"\n",
             p->key, compute_zob_key(p));

    WHEN_DEBUG_VERBOSE({
      square_to_str(victim_sq, buf, MAX_CHARS_IN_MOVE);
      DEBUG_LOG(1, "Zapped piece on %s\n", buf);
    });
  }

  if (USE_KO) {  // Ko rule
    if (p->key == (old->key ^ zob_color)) {
      bool match = true;

      for (fil_t f = 0; f < BOARD_WIDTH; f++) {
        for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
          if (p->board[square_of(f, r)] !=
              old->board[square_of(f, r)]) {
            match = false;
          }
        }
      }

      if (match) {
        return KO();
      }
    }

    if (p->key == old->history->key) {
      bool match = true;

      for (fil_t f = 0; f < BOARD_WIDTH; f++) {
        for (rnk_t r = 0; r < BOARD_WIDTH; r++) {
          if (p->board[square_of(f, r)] !=
              old->history->board[square_of(f, r)]) {
            match = false;
          }
        }
      }

      if (match) {
        return KO();
      }
    }
  }

  return p->victims;
}

// -----------------------------------------------------------------------------
// Move path enumeration (perft)
// -----------------------------------------------------------------------------

// Helper function for do_perft() (ply starting with 0).
//
// NOTE: This function reimplements some of the logic for make_move().
static uint64_t perft_search(position_t* p, int depth, int ply) {
  uint64_t node_count = 0;
  position_t np;
  sortable_move_t lst[MAX_NUM_MOVES];
  int num_moves;
  int i;

  if (depth == 0) {
    return 1;
  }

  num_moves = generate_all(p, lst, true);

  if (depth == 1) {
    return num_moves;
  }

  for (i = 0; i < num_moves; i++) {
    move_t mv = get_move(lst[i]);

    low_level_make_move(p, &np, mv);  // make the move baby!

    square_t victim_sq = 0;  // the guys to disappear
    np.victims.zapped_count = 0;

    if ((victim_sq = fire_laser(&np, color_to_move_of(p)))) {  // hit a piece
      piece_t victim_piece = np.board[victim_sq];
      tbassert((ptype_of(victim_piece) != EMPTY) &&
               (ptype_of(victim_piece) != INVALID),
               "type: %d\n", ptype_of(victim_piece));

      np.victims.zapped_count++;
      np.victims.zapped = victim_piece;
      np.key ^= zob[victim_sq][victim_piece&REVERT_MASK];   // remove from board
      np.board[victim_sq] = 0;
      np.key ^= zob[victim_sq][0];

      color_t victim_color = color_of(victim_piece);
      np.ploc[victim_color][index_of(victim_piece)] = -1;
    }

    if (np.victims.zapped_count > 0 &&
        ptype_of(np.victims.zapped) == KING) {
      // do not expand further: hit a King
      node_count++;
      continue;
    }

    uint64_t partialcount = perft_search(&np, depth - 1, ply + 1);
    node_count += partialcount;
  }

  return node_count;
}

// Debugging function to help verify that the move generator is working
// correctly.
//
// https://www.chessprogramming.org/Perft
void do_perft(position_t* gme, int depth, int ply) {
  fen_to_pos(gme, "");

  for (int d = 1; d <= depth; d++) {
    printf("perft %2d ", d);
    uint64_t j = perft_search(gme, d, 0);
    printf("%" PRIu64 "\n", j);
  }
}

// -----------------------------------------------------------------------------
// Position display
// -----------------------------------------------------------------------------

void display(position_t* p) {
  char buf[MAX_CHARS_IN_MOVE];

  printf("\ninfo Ply: %d\n", p->ply);
  printf("info Color to move: %s\n", color_to_str(color_to_move_of(p)));

  square_to_str(p->kloc[WHITE], buf, MAX_CHARS_IN_MOVE);
  printf("info White King: %s, ", buf);
  square_to_str(p->kloc[BLACK], buf, MAX_CHARS_IN_MOVE);
  printf("info Black King: %s\n", buf);

  if (p->last_move != 0) {
    move_to_str(p->last_move, buf, MAX_CHARS_IN_MOVE);
    printf("info Last move: %s\n", buf);
  } else {
    printf("info Last move: NULL\n");
  }

  for (rnk_t r = BOARD_WIDTH - 1; r >= 0 ; --r) {
    printf("\ninfo %1d  ", r);
    for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
      square_t sq = square_of(f, r);

      tbassert(ptype_of(p->board[sq]) != INVALID,
               "ptype_of(p->board[sq]): %d\n", ptype_of(p->board[sq]));
      /*if (p->blocked[sq]) {
        printf(" xx");
        continue;
      }*/
      if (ptype_of(p->board[sq]) == EMPTY) {       // empty square
        printf(" --");
        continue;
      }

      int ori = ori_of(p->board[sq]);  // orientation
      color_t c = color_of(p->board[sq]);

      if (ptype_of(p->board[sq]) == KING) {
        printf(" %2s", king_ori_to_rep[c][ori]);
        continue;
      }

      if (ptype_of(p->board[sq]) == PAWN) {
        printf(" %2s", pawn_ori_to_rep[c][ori]);
        continue;
      }
    }
  }

  printf("\n\ninfo    ");
  for (fil_t f = 0; f < BOARD_WIDTH; ++f) {
    printf(" %c ", 'a' + f);
  }
  printf("\n\n");
}
