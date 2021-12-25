// vim: set fileencoding=utf-8
#include <stdlib.h>
#include "naomi/vector.h"

void test_vector_length(test_context_t *context)
{
    vector_t vec;

    vec.x = 1.0;
    vec.y = 0.0;
    vec.z = 0.0;
    ASSERT_EQUAL(vector_length(&vec), 1.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 1.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 0.0, "Unexpected coordinate for vector!");

    vec.x = 0.0;
    vec.y = 1.0;
    vec.z = 0.0;
    ASSERT_EQUAL(vector_length(&vec), 1.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 1.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 0.0, "Unexpected coordinate for vector!");

    vec.x = 0.0;
    vec.y = 0.0;
    vec.z = 1.0;
    ASSERT_EQUAL(vector_length(&vec), 1.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 1.0, "Unexpected coordinate for vector!");

    vec.x = 3.0;
    vec.y = 4.0;
    vec.z = 0.0;
    ASSERT_EQUAL(vector_length(&vec), 5.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 0.6, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 0.8, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 0.0, "Unexpected coordinate for vector!");

    vec.x = 3.0;
    vec.y = 0.0;
    vec.z = 4.0;
    ASSERT_EQUAL(vector_length(&vec), 5.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 0.6, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 0.8, "Unexpected coordinate for vector!");

    vec.x = 0.0;
    vec.y = 3.0;
    vec.z = 4.0;
    ASSERT_EQUAL(vector_length(&vec), 5.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_EQUAL(vec.x, 0.0, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.y, 0.6, "Unexpected coordinate for vector!");
    ASSERT_EQUAL(vec.z, 0.8, "Unexpected coordinate for vector!");

    vec.x = 2.0;
    vec.y = 3.0;
    vec.z = 6.0;
    ASSERT_EQUAL(vector_length(&vec), 7.0, "Unexpected length for vector!");
    vector_normalize(&vec);
    ASSERT_APPROX(vec.x, 0.2857, "Unexpected coordinate for vector!");
    ASSERT_APPROX(vec.y, 0.4286, "Unexpected coordinate for vector!");
    ASSERT_APPROX(vec.z, 0.8571, "Unexpected coordinate for vector!");
}

void test_vector_cross(test_context_t *context)
{
    vector_t first;
    vector_t second;
    vector_t answer;

    first.x = 1.0;
    first.y = 0.0;
    first.z = 0.0;

    second.x = 0.0;
    second.y = 1.0;
    second.z = 0.0;

    vector_cross(&answer, &first, &second);
    ASSERT_EQUAL(answer.x, 0.0, "Unexpected X value for cross product!");
    ASSERT_EQUAL(answer.y, 0.0, "Unexpected X value for cross product!");
    ASSERT_EQUAL(answer.z, 1.0, "Unexpected X value for cross product!");
}

void test_vector_dot(test_context_t *context)
{
    vector_t first;
    vector_t second;

    first.x = 1.0;
    first.y = 0.0;
    first.z = 0.0;
    second.x = 0.0;
    second.y = 1.0;
    second.z = 0.0;
    ASSERT_EQUAL(vector_dot(&first, &second), 0.0, "Unexpected dot product value!");

    first.x = 1.0;
    first.y = 0.0;
    first.z = 0.0;
    second.x = 1.0;
    second.y = 0.0;
    second.z = 0.0;
    ASSERT_EQUAL(vector_dot(&first, &second), 1.0, "Unexpected dot product value!");

    first.x = 1.0;
    first.y = 1.0;
    first.z = 0.0;
    second.x = 1.0;
    second.y = 0.0;
    second.z = 0.0;
    vector_normalize(&first);
    vector_normalize(&second);
    ASSERT_APPROX(first.x, 0.7071, "Unexpected coordinate for vector!");
    ASSERT_APPROX(first.y, 0.7071, "Unexpected coordinate for vector!");
    ASSERT_APPROX(first.z, 0.0, "Unexpected coordinate for vector!");
    ASSERT_APPROX(second.x, 1.0, "Unexpected coordinate for vector!");
    ASSERT_APPROX(second.y, 0.0, "Unexpected coordinate for vector!");
    ASSERT_APPROX(second.z, 0.0, "Unexpected coordinate for vector!");
    ASSERT_APPROX(vector_dot(&first, &second), 0.7071, "Unexpected dot product value!");
}
