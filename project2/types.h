#ifndef TYPES_H_
#define TYPES_H_

#include "./line.h"
#include "./vec.h"

struct Bounds {
    Vec sw;
    Vec ne;
};
typedef struct Bounds Bounds;

struct vector {
    void **data;
    void **empty;
    int size;
    int count;
};
typedef struct vector vector;

struct QuadTree {
    Bounds bounds;
    bool isLeaf;

    //children
    //nw, ne, se, sw
    struct QuadTree* children[4];

    // LinkedList of nodes in the head that:
    //     are not splitted if line_count <= 4
    //     or
    //     can't be splitted
    vector* nodes;

    struct QuadTree* parent;

    int current_depth;
};
typedef struct QuadTree QuadTree;

#endif