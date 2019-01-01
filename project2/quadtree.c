#include <stdlib.h>

#include "./quadtree.h"
#include "./vec.h"
#include "./line.h"

extern IntersectionEventListReducer X;

#define MAX_LINES 75
#define MAX_DEPTH 8

QuadTree* QuadTree_make() {
  QuadTree* qt = malloc(sizeof(QuadTree));
  Bounds bounds;

  bounds.sw = Vec_make(BOX_XMIN, BOX_YMIN);
  bounds.ne = Vec_make(BOX_XMAX, BOX_YMAX);
  qt->bounds = bounds;
  qt->isLeaf = true;
  qt->current_depth = 0;
  qt->nodes = Vector_make();
  qt->parent = NULL;

  return qt;
}

// assumes l1 within bounds of the tree
void QuadTree_insert(QuadTree* qt, Line* line) {
  assert(line != NULL);

  if (qt->isLeaf) {
    if (qt->current_depth >= MAX_DEPTH || qt->nodes->count < MAX_LINES) {
      Vector_append(qt->nodes, line);
    } else {
      QuadTree_split(qt);
      QuadTree_insert(qt, line);
    }

    return;
  }

  if (QuadTree_contains(qt->children[0], line)) {
    QuadTree_insert(qt->children[0], line);
  } else if (QuadTree_contains(qt->children[1], line)) {
    QuadTree_insert(qt->children[1], line);
  } else if (QuadTree_contains(qt->children[2], line)) {
    QuadTree_insert(qt->children[2], line);
  } else if (QuadTree_contains(qt->children[3], line)) {
    QuadTree_insert(qt->children[3], line);
  } else {
    Vector_append(qt->nodes, line);
  }
}

bool rect_contains(Vec* sw1, Vec* ne1, Vec* sw2, Vec* ne2) {
    return sw1->x < sw2->x && sw1->y < sw2->y &&
           ne1->x > ne2->x && ne1->y > ne2->y;
}

bool rect_intersect(Vec* sw1, Vec* ne1, Vec* sw2, Vec* ne2) {
    return !(sw1->x > ne2->x || sw2->x > ne1->x ||
             sw1->y > ne2->y || sw2->y > ne1->y);
}


inline bool QuadTree_contains(QuadTree* qt, Line* l) {
  return rect_contains(&qt->bounds.sw, &qt->bounds.ne, &l->sw, &l->ne);
}

void Line_update_box(Line* line) {
  line->sw.x = MIN(line->p1.x, line->p2.x) + MIN(line->velocity.x, 0);
  line->sw.y = MIN(line->p1.y, line->p2.y) + MIN(line->velocity.y, 0);
  line->ne.x = MAX(line->p1.x, line->p2.x) + MAX(line->velocity.x, 0);
  line->ne.y = MAX(line->p1.y, line->p2.y) + MAX(line->velocity.y, 0);
}

void QuadTree_split(QuadTree* qt) {
  assert(qt->nodes->count >= MAX_LINES);

  qt->isLeaf = false;

  for (int i = 0; i < 4; i++) {
    qt->children[i] = malloc(sizeof(QuadTree));
    qt->children[i]->isLeaf = true;
    qt->children[i]->current_depth = qt->current_depth + 1;
    qt->children[i]->nodes = Vector_make();
    qt->children[i]->parent = qt;
  }

  double mid_x = (qt->bounds.sw.x + qt->bounds.ne.x)/2.0;
  double mid_y = (qt->bounds.sw.y + qt->bounds.ne.y)/2.0;

  Bounds nw_bounds;
  nw_bounds.sw = Vec_make(qt->bounds.sw.x, mid_y);
  nw_bounds.ne = Vec_make(mid_x, qt->bounds.ne.y);
  qt->children[0]->bounds = nw_bounds;

  Bounds ne_bounds;
  ne_bounds.sw = Vec_make(mid_x, mid_y);
  ne_bounds.ne = Vec_make(qt->bounds.ne.x, qt->bounds.ne.y);
  qt->children[1]->bounds = ne_bounds;

  Bounds se_bounds;
  se_bounds.sw = Vec_make(mid_x, qt->bounds.sw.y);
  se_bounds.ne = Vec_make(qt->bounds.ne.x, mid_y);
  qt->children[2]->bounds = se_bounds;

  Bounds sw_bounds;
  sw_bounds.sw = qt->bounds.sw;
  sw_bounds.ne = Vec_make(mid_x, mid_y);
  qt->children[3]->bounds = sw_bounds;

  // vector* old_nodes = qt->nodes;
  // vector* new_nodes = Vector_make();

  // int count = old_nodes->count;
  // qt->nodes = new_nodes;
  // vector *old_nodes = qt->nodes;
  // qt->nodes = Vector_make();
  int count = qt->nodes->count;
  Vector_switch(qt->nodes);
  for (int i = 0; i < count; i++) {
    QuadTree_insert(qt, qt->nodes->empty[i]);
  }
  // Vector_free(old_nodes);
}

void Vector_intersect_node(Line* l1, vector* v,
                                 IntersectionEventList* intersectionEventList,
                                 CollisionWorld* collisionWorld) {
  int cmp;
  for (int i = 0; i < v->count; i++) {
    Line* l2 = v->data[i];
    // intersect expects compareLines(l1, l2) < 0 to be true.
    // Swap l1 and l2, if necessary.
    // IntersectionType ml =
    //       intersect(l1, l2);
    if (!rect_intersect(&l1->sw, &l1->ne,
                           &l2->sw, &l2->ne)) {
        assert(ml == NO_INTERSECTION);
    }
    if (rect_intersect(&l1->sw, &l1->ne,
                       &l2->sw, &l2->ne)) {
        cmp = compareLines(l1, l2);
        if (cmp > 0) {
        IntersectionType intersectionType =
          intersect(l2, l1, collisionWorld->timeStep);
        if (intersectionType != NO_INTERSECTION) {
          IntersectionEventList_appendNode(&REDUCER_VIEW(X), l2, l1,
                                           intersectionType);
        }
      } else { 
        IntersectionType intersectionType =
          intersect(l1, l2, collisionWorld->timeStep);
        if (intersectionType != NO_INTERSECTION) {
          IntersectionEventList_appendNode(&REDUCER_VIEW(X), l1, l2,
                                           intersectionType);
        }
      }
    }
  }
}

void QuadTree_intersect_node(QuadTree* qt,
                         IntersectionEventList* intersectionEventList,
                         Line* line,
                         CollisionWorld* collisionWorld) {
  assert(line != NULL);

  Vector_intersect_node(line, qt->nodes,
                        &REDUCER_VIEW(X),
                        collisionWorld);

  if (!qt->isLeaf) {
    for (int i = 0; i < 4; i++) {
      if (rect_intersect(&qt->children[i]->bounds.sw, &qt->children[i]->bounds.ne,
                         &line->sw, &line->ne)) {
         QuadTree_intersect_node(qt->children[i], &REDUCER_VIEW(X), line, collisionWorld);
      }
    }
  }
}

void QuadTree_collisions(QuadTree* qt,
                         IntersectionEventList* intersectionEventList,
                         CollisionWorld* collisionWorld) {
  assert(qt != NULL);

  cilk_spawn Vector_intersect(qt->nodes, qt->nodes,
                   &REDUCER_VIEW(X),
                   collisionWorld);

  if (!qt->isLeaf) {
    Line *line;
    for (int i = 0; i < qt->nodes->count; i++) {
        line = qt->nodes->data[i];
        for (int i = 0; i < 4; i++) {
          if (rect_intersect(&qt->children[i]->bounds.sw, &qt->children[i]->bounds.ne,
                             &line->sw, &line->ne)) {
            QuadTree_intersect_node(qt->children[i], &REDUCER_VIEW(X), line, collisionWorld);
          }
        }
      }

    if (qt->current_depth < MAX_DEPTH - 2) {
      cilk_for (int i = 0; i < 4; i++) {
        QuadTree_collisions(qt->children[i], &REDUCER_VIEW(X), collisionWorld);
      }
    } else {
      for (int i = 0; i < 4; i++) {
        QuadTree_collisions(qt->children[i], &REDUCER_VIEW(X), collisionWorld);
      }
    }
  }
  cilk_sync;
}

void Vector_intersect(vector* v1, vector* v2,
                            IntersectionEventList* intersectionEventList,
                            CollisionWorld* collisionWorld) {
  int cmp;
  for (int i = 0; i < v1->count; i++) {
    Line* l1 = v1->data[i];
    for (int j = i+1; j < v2->count; j++) {
      Line* l2 = v2->data[j];
      // intersect expects compareLines(l1, l2) < 0 to be true.
      // Swap l1 and l2, if necessary.
    if (!rect_intersect(&l1->sw, &l1->ne,&l2->sw, &l2->ne)) {
        assert(ml == NO_INTERSECTION);
    }
      if (rect_intersect(&l1->sw, &l1->ne,
                         &l2->sw, &l2->ne)) {
        if (l1->id > l2->id) {
            IntersectionType intersectionType =
              intersect(l2, l1, collisionWorld->timeStep);
          if (intersectionType != NO_INTERSECTION) {
            IntersectionEventList_appendNode(&REDUCER_VIEW(X), l2, l1,
                                             intersectionType);
          }
        } else { 
          IntersectionType intersectionType =
            intersect(l1, l2, collisionWorld->timeStep);
          if (intersectionType != NO_INTERSECTION) {
            IntersectionEventList_appendNode(&REDUCER_VIEW(X), l1, l2,
                                             intersectionType);
          }
        }
      }
    }
  }
}

void QuadTree_delete(QuadTree* qt) {
  if (qt == NULL) {
    return;
  }
  if (!qt->isLeaf) {
    for (int i = 0; i < 4; i++) {
      QuadTree* cur_qt = qt->children[i];
      if (cur_qt != NULL) {
        QuadTree_delete(qt->children[i]);
        qt->children[i] = NULL;
      }
    }
  }
  Vector_free(qt->nodes);
  qt->nodes = NULL;
  free(qt);
  qt = NULL;
}

void QuadTree_increase_node(QuadTree* qt, Line* line) {
  while (qt->parent != NULL && !QuadTree_contains(qt, line)) {
    qt = qt->parent;
  }
  QuadTree_insert(qt, line);
}

void QuadTree_update(QuadTree* qt) {
  int count = qt->nodes->count;
  Vector_switch(qt->nodes);
  for (int i = 0; i < count; i++) {
    QuadTree_increase_node(qt, qt->nodes->empty[i]);
  }

  if (!qt->isLeaf) {
    for (int i = 0; i < 4; i++) {
      QuadTree_update(qt->children[i]);
    }
  }
}
