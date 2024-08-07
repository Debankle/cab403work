#include <stdio.h>
#include <stdlib.h>
#include "dbl_vector.h"

void dv_init(dbl_vector_t* vec) {
    vec->capacity = DV_INITIAL_CAPACITY;
    vec->size = 0;
    vec->data = malloc(DV_INITIAL_CAPACITY * sizeof(double));
}

void dv_ensure_capacity(dbl_vector_t* vec, size_t new_size) {
    if (new_size > vec->capacity) {
        size_t new_capacity = vec->capacity * DV_GROWTH_FACTOR;
        if (new_capacity < new_size) {
            new_capacity = new_size;
        }

        size_t new_mem_size = new_capacity * sizeof(double);

        double *new_data = (double *) realloc(vec->data, new_mem_size);
        if (new_data == NULL) {
            exit(-1);
        }

        vec->data = new_data;
        vec->capacity = new_capacity;
    }
}

void dv_destroy(dbl_vector_t* vec) {
    vec->capacity = 0,
    vec->size = 0;
    free(vec->data);
    vec->data = NULL;
}

void dv_copy(dbl_vector_t* vec, dbl_vector_t* dest) {
    dest->size = vec->size;
    dv_ensure_capacity(dest, vec->size);
    for (size_t i = 0; i < vec->size; i++) {
        dest->data[i] = vec->data[i];
    }
}

void dv_clear(dbl_vector_t* vec) {
    vec->size = 0;
}

void dv_push(dbl_vector_t* vec, double new_item) {
    size_t old_size = vec->size;
    vec->size = old_size + 1;
    dv_ensure_capacity(vec,old_size+1);
    vec->data[old_size] = new_item;
}

void dv_pop(dbl_vector_t* vec) {
    size_t old_size = vec->size;
    if (old_size > 0) {
        vec->size = old_size - 1;
    }
}

double dv_last(dbl_vector_t* vec) {
    double result = NAN;
    
    if (vec->size > 0) {
        return vec->data[vec->size - 1];
    }

    return result;
}

void dv_insert_at(dbl_vector_t* vec, size_t pos, double new_item) {
    size_t old_size = vec->size;
    vec->size = old_size + 1;
    size_t loc = pos;
    if (loc > old_size) loc = old_size;
    dv_ensure_capacity(vec,old_size+1);
    for (size_t i = loc; i < old_size + 1; i++) {
        vec->data[loc+1] = vec->data[loc];
    }
    vec->data[loc] = new_item;
}

void dv_remove_at(dbl_vector_t* vec, size_t pos) {
    if (pos >= vec->size) {
        return;
    }
    for (size_t i = pos; i < vec->size - 1; i++) {
        vec->data[i] = vec->data[i+1];
    }
    vec->size -= 1;
}

void dv_foreach(dbl_vector_t* vec, void (*callback)(double, void*), void* info) {
    for (size_t i = 0; i < vec->size; i++) {
        callback(vec->data[i], info);
    }
}