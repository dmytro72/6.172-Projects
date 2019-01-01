#include <assert.h>

#include "./intersection_detection.h"
#include "./intersection_event_list.h"
#include "./line.h"
#include "./vec.h"
#include "./collision_world.h"
#include "./vector.h"
#include "./types.h"

void QuadTree_insert(QuadTree* qt, Line* qn);

bool QuadTree_contains(QuadTree* qt, Line* line);

void QuadTree_collisions(QuadTree* qt,
                         IntersectionEventList* intersectionEventList,
                         CollisionWorld* collisionWorld);

QuadTree* QuadTree_make();

int QuadTree_count_lines(QuadTree* qt);

void QuadTree_delete(QuadTree* qt);

void QuadTree_split(QuadTree* qt);

void QuadTree_update(QuadTree* qt);

void Vector_intersect(vector* v1, vector* v2, 
                            IntersectionEventList* intersectionEventList,
                            CollisionWorld* collisionWorld);

void Vector_intersect_node(Line* line, vector* v,
                            IntersectionEventList* intersectionEventList,
                            CollisionWorld* collisionWorld);

void Line_update_box(Line* line);
