#include "naomi/vector.h"
#include "naomi/matrix.h"
#include "naomi/interrupt.h"

float vector_length(vector_t *vec)
{
    uint32_t old_irq = irq_disable();

    register vector_t *vec_param asm("r4") = vec;
    register float retval asm("fr0");
    asm volatile(" \
        fmov.s @r4+,fr0\n \
        fmov.s @r4+,fr1\n \
        fmov.s @r4+,fr2\n \
        fldi0 fr3\n \
        fipr fv0,fv0\n \
        fsqrt fr3\n \
        fmov fr3,fr0\n \
        " :
        "=r" (retval) :
        "r" (vec_param) :
        "fr1", "fr2", "fr3"
    );

    irq_restore(old_irq);
    return retval;
}

void vector_from_vertex(vector_t *vec, vertex_t *first, vertex_t *second)
{
    vec->x = second->x - first->x;
    vec->y = second->y - first->y;
    vec->z = second->z - first->z;
}

void vector_from_textured_vertex(vector_t *vec, textured_vertex_t *first, textured_vertex_t *second)
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
    uint32_t old_irq = irq_disable();

    register vector_t *a_param asm("r4") = a;
    register vector_t *b_param asm("r5") = b;
    register float retval asm("fr0");
    asm volatile(" \
        fmov.s @r4+,fr0\n \
        fmov.s @r4+,fr1\n \
        fmov.s @r4+,fr2\n \
        fldi0 fr3\n \
        fmov.s @r5+,fr4\n \
        fmov.s @r5+,fr5\n \
        fmov.s @r5+,fr6\n \
        fldi0 fr7\n \
        fipr fv4,fv0\n \
        fmov fr3,fr0\n \
        " :
        "=r" (retval) :
        "r" (a_param), "r" (b_param) :
        "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7"
    );

    irq_restore(old_irq);
    return retval;
}

void vector_normalize(vector_t *vec)
{
    float invlength = 1.0 / vector_length(vec);
    vec->x *= invlength;
    vec->y *= invlength;
    vec->z *= invlength;
}
