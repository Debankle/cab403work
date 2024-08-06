#include "dbl_vector.h"

#include <stdio.h>
#include <stdlib.h>

void dv_init(dbl_vector_t* vec) {
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->size = 0;
    vec->data = malloc(DV_INITIAL_CAPACITY * sizeof(double));
}

void dv_ensure_capacity(dbl_vector_t* vec, size_t new_size) {
    if (new_size <= vec->capacity) {
    } else {
    }
}

void dv_destroy(dbl_vector_t* vec) {
    // INSERT SOLUTION HERE
}

void dv_copy(dbl_vector_t* vec, dbl_vector_t* dest) {
    // INSERT SOLUTION HERE
}

void dv_clear(dbl_vector_t* vec) {
    // INSERT SOLUTION HERE
}

void dv_push(dbl_vector_t* vec, double new_item) {
    // INSERT SOLUTION HERE
}

void dv_pop(dbl_vector_t* vec) {
    // INSERT SOLUTION HERE
}

double dv_last(dbl_vector_t* vec) {
    double result = NAN;
    // INSERT SOLUTION HERE
    return result;
}

void dv_insert_at(dbl_vector_t* vec, size_t pos, double new_item) {
    // INSERT SOLUTION HERE
}

void dv_remove_at(dbl_vector_t* vec, size_t pos) {
    // INSERT SOLUTION HERE
}

void dv_foreach(dbl_vector_t* vec, void (*callback)(double, void*), void* info) {
    // INSERT SOLUTION HERE
}