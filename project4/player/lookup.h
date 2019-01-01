// Copyright (c) 2018 MIT License by 6.172 Staff

#ifndef LOOKUP_H
#define LOOKUP_H

#define OPEN_BOOK_DEPTH 6
int lookup_sizes[OPEN_BOOK_DEPTH] = {
  2,
  6,
  6,
  18,
  18,
  54
};

char* lookup_table_depth_0[] = {"", "g4L"};
char* lookup_table_depth_1[] = {"g4L", "b3b2", "h0g1", "b3L", "h0g0", "b3L"};
char* lookup_table_depth_2[] = {"g4Lb3L", "g4h5", "g4La7b6", "g4h5", "g4Lb4R", "g4h5"};
char* lookup_table_depth_3[] = {"g4Lb3b2g4h5", "b2L", "g4Lb3b2h0g1", "b2L", "g4Lb3b2g4g5", "b2L", "h0g1b3Lg4L", "b4c3", "h0g1b3Ld1c2", "c4R", "h0g1b3Ld1d2", "b3a2", "h0g0b3Lg4L", "b3a2", "h0g0b3Lf3R", "b3a2", "h0g0b3Ld1c0", "b4c3"};
char* lookup_table_depth_4[] = {"g4Lb3Lg4h5c5R", "g3R", "g4Lb3Lg4h5b4c3", "d1c2", "g4Lb3Lg4h5d6e7", "g3f4", "g4La7b6g4h5b3L", "g3R", "g4La7b6g4h5e6L", "e1L", "g4La7b6g4h5c4R", "e2d3", "g4Lb4Rg4h5b3L", "h5g6", "g4Lb4Rg4h5d6e7", "e2d3", "g4Lb4Rg4h5c4R", "f3e4"};
char* lookup_table_depth_5[] = {"g4Lb3b2g4h5b2Ld1c2", "c5R", "g4Lb3b2g4h5b2Lh5g6", "d6e7", "g4Lb3b2g4h5b2Ld1c1", "b4R", "g4Lb3b2h0g1b2Ld1c2", "b4c3", "g4Lb3b2h0g1b2Ld1c1", "b4c3", "g4Lb3b2h0g1b2Lf3R", "b4R", "g4Lb3b2g4g5b2Le2d2", "e6f6", "g4Lb3b2g4g5b2Le1d0", "a7b6", "g4Lb3b2g4g5b2Lg5g6", "a7a6", "h0g1b3Lg4Lb4c3f3R", "c4R", "h0g1b3Lg4Lb4c3d1c2", "b3b2", "h0g1b3Lg4Lb4c3f2e3", "c4R", "h0g1b3Ld1c2c4Rg4L", "a7b6", "h0g1b3Ld1c2c4Rf3R", "b4R", "h0g1b3Ld1c2c4Rc2b3a2", "a7b6", "h0g1b3Ld1d2b3a2g4L", "b4R", "h0g1b3Ld1d2b3a2d2c2", "b4b3", "h0g1b3Ld1d2b3a2e1d1", "b4c3", "h0g0b3Lg4Lb3a2f2R", "a2b1", "h0g0b3Lg4Lb3a2f2e3", "a2a1", "h0g0b3Lg4Lb3a2f3R", "c4d3", "h0g0b3Lf3Rb3a2g4L", "c4d3", "h0g0b3Lf3Rb3a2f2R", "b4c3", "h0g0b3Lf3Rb3a2e1d0", "d5e4", "h0g0b3Ld1c0b4c3g4L", "b3b2", "h0g0b3Ld1c0b4c3g4f4", "b3a2", "h0g0b3Ld1c0b4c3f3R", "b3a2"};

char** lookup_tables[OPEN_BOOK_DEPTH] = {
  lookup_table_depth_0,
  lookup_table_depth_1,
  lookup_table_depth_2,
  lookup_table_depth_3,
  lookup_table_depth_4,
  lookup_table_depth_5
};

#endif // LOOKUP_H
