#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;

typedef struct vector {
    size_t size;
    size_t capacity;
    size_t length;
    byte* data;
} vector;

#define VECTOR_BASE_CAPACITY 20

void* vector_get(vector*, int32_t);

int32_t vector_add(vector*, void*);

void vector_set(vector*, void*, int32_t);

size_t vector_length(vector*);

void vector_delete(vector*);

vector* vector_new(size_t);

static void vector_resize(vector*);
