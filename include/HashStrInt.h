#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct {
  int occupied;
  char *key;
  int value;
} ColIdxBucket;

typedef struct {
  ColIdxBucket *buckets;
  uint32_t capacity;
  uint32_t size;
} HashStrInt;

int col_index_reserve(HashStrInt *h, size_t n_strs);
void col_index_destroy(HashStrInt *h);
int col_index_put(HashStrInt *h, const char *key, int value);
int col_index_get(const HashStrInt *h, const char *key);