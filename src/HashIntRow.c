#include "HashIntRow.h"
#include <stdint.h>
#include <stdlib.h>

uint32_t hash_int(long key) {
  return (uint32_t)((unsigned long)key * 2654435769u);
}

int rows_init(HashIntRow *h, int initial_capacity) {
  h->buckets = calloc(initial_capacity, sizeof(RowBucket));
  if (h->buckets == NULL) {
    return -1;
  }
  h->capacity = initial_capacity;
  h->size = 0;
  return 0;
}

void rows_destroy(HashIntRow *h) {
  for (int i = 0; i < h->capacity; i++) {
    if (h->buckets[i].occupied) {
      row_destroy(h->buckets[i].value, 0);
    }
  }
  free(h->buckets);
  h->buckets = NULL;
  h->capacity = 0;
  h->size = 0;
}

int rows_put(HashIntRow *h, int key, Row *value) {
  if (h->size * 2 >= h->capacity) {
    int new_capacity = h->capacity * 2;
    RowBucket *new_buckets = calloc(new_capacity, sizeof(RowBucket));
    if (new_buckets == NULL) {
      return -1;
    }
    for (int i = 0; i < h->capacity; i++) {
      if (h->buckets[i].occupied) {
        uint32_t idx = hash_int(h->buckets[i].key) & (new_capacity - 1);
        while (new_buckets[idx].occupied) {
          idx = (idx + 1) & (new_capacity - 1);
        }
        new_buckets[idx] = h->buckets[i];
      }
    }
    free(h->buckets);
    h->buckets = new_buckets;
    h->capacity = new_capacity;
  }
  uint32_t idx = hash_int(key) & (h->capacity - 1);
  int steps = 0;

  while (h->buckets[idx].occupied) {
    if (h->buckets[idx].key == key) {
      return -1;
    }
    idx = (idx + 1) & (h->capacity - 1);
    if (++steps >= h->capacity) {
      return -1;
    }
  }
  return 0;
}

Row *rows_get(const HashIntRow *h, int key) {
  if (h->buckets == NULL)
    return NULL;
  uint32_t idx = hash_int(key) & (h->capacity - 1);
  int steps = 0;

  while (h->buckets[idx].occupied) {
    if (h->buckets[idx].key == key) {
      return h->buckets[idx].value;
    }
    idx = (idx + 1) & (h->capacity - 1);
    if (++steps >= h->capacity)
      break;
  }
  return NULL;
}