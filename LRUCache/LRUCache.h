#include "../types.h"

// This is my LRU cache implementation with a strict total bytes capacity rule. 
// Each item is stored as a void* with its size added just to indicate if it fits.
// Its typing is <char*, void*>

typedef struct LRUCache LRUCache;

LRUCache* init_LRUCache(size_t total_capacity, size_t max_items); 

void destroy_LRUCache();

void put_LRUCache(char* name, void* item, size_t size);

void* get_LRUCache(char* name);
