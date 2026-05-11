#pragma once
typedef struct {
  int occupied;
  char *key;
  int value;
} ColIdxBucket;

typedef struct {
  ColIdxBucket *buckets;
  int capacity;
  int size;
} HashStrInt;

int col_index_init(HashStrInt *h, int capacity);
void col_index_destroy(HashStrInt *h);
int col_index_put(HashStrInt *h, const char *key, int value);
int col_index_get(const HashStrInt *h, const char *key);