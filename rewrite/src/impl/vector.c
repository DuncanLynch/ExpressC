#include "vector.h"

vector* vector_new(size_t datasize) {
    vector* temp = (vector*)malloc(sizeof(vector));
    temp->size = datasize;
    temp->capacity = VECTOR_BASE_CAPACITY;
    temp->length = 0;
    temp->data = (byte*)malloc(datasize * temp->capacity);
    return temp;
}

void vector_delete(vector* vec) {
    free(vec->data);
    free(vec);
}

int32_t vector_add(vector* vec, void* data) {
    if (vec->length >= vec->capacity) {
        vector_resize(vec);
    }

    byte* dest = vec->data + vec->length * vec->size;
    byte* src = data;

    memcpy(dest, src, vec->size);

    vec->length++;
    return vec->length - 1;
}
