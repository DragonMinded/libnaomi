#ifndef __VECTOR_H
#define __VECTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "naomi/matrix.h"

// Type definition for a 3D vector.
typedef struct
{
    float x;
    float y;
    float z;
} vector_t;

// given two vertex points, return the vector that could be
// added to the first vertex point to get the second.
void vector_from_vertex(vector_t *vec, vertex_t *first, vertex_t *second);
void vector_from_textured_vertex(vector_t *vec, textured_vertex_t *first, textured_vertex_t *second);
        
// Given a vector, return its length.
float vector_length(vector_t *vec);

// Goven two vectors, return its dot product. For this to have
// any meaning, both vectors should be normalized first.
float vector_dot(vector_t *a, vector_t *b);

// Given two vectors, return its cross product.
void vector_cross(vector_t *cross, vector_t *a, vector_t *b);

// Takes a vector and normalizes it (ensures that it has
// unit length).
void vector_normalize(vector_t *vec);

#ifdef __cplusplus
}
#endif

#endif
