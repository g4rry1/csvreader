#pragma once
#include <stdint.h>
#include "Row.h"

typedef struct {
  int occupied;
  long key;
  Row *value;
} RowBucket;

typedef struct {
  RowBucket *buckets;
  uint32_t capacity;
  uint32_t size;
} HashIntRow;

void row_index_destroy(HashIntRow *h);
int row_index_put(HashIntRow *h, long key, Row *value);
Row *row_index_get(const HashIntRow *h, long key);