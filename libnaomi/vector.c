#include "naomi/vector.h"
#include "naomi/matrix.h"
#include <math.h>

float vector_length(vector_t *vec)
{
    return sqrt((vec->x * vec->x) + (vec->y * vec->y) + (vec->z * vec->z));
}

void vector_from_vertex(vector_t *vec, vertex_t *first, vertex_t *second)
{
    vec->x = second->x - first->x;
    vec->y = second->y - first->y;
    vec->z = second->z - first->z;
}

void vector_cross(vector_t *cross, vector_t *a, vector_t *b)
{
    cross->x = (a->y * b->z) - (a->z * b->y);
    cross->y = (a->z * b->x) - (a->x * b->z);
    cross->z = (a->x * b->y) - (a->y * b->x);
}

float vector_dot(vector_t *a, vector_t *b)
{
    return (a->x * b->x) + (a->y * b->y) + (a->z * b->z);
}

void vector_normalize(vector_t *vec)
{
    float invlength = 1.0 / vector_length(vec);
    vec->x *= invlength;
    vec->y *= invlength;
    vec->z *= invlength;
}
