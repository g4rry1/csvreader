#pragma once
#include "Row.h"

typedef struct {
  int occupied;
  int key;
  Row *value;
} RowBucket;

typedef struct {
  RowBucket *buckets;
  int capacity;
  int size;
} HashIntRow;

int rows_init(HashIntRow *h, int initial_capacity);
void rows_destroy(HashIntRow *h);
int rows_put(HashIntRow *h, int key, Row *value);
Row *rows_get(const HashIntRow *h, int key);