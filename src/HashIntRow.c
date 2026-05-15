#include "HashIntRow.h"
#include "errors.h"
#include <stdint.h>
#include <stdlib.h>

#define INITIAL_HASH_CAP 16

static uint32_t hash_int(long key) {
  return (uint32_t)((unsigned long)key * 2654435769u);
}

void row_index_destroy(HashIntRow *h) {
  free(h->buckets);
  h->buckets = NULL;
  h->capacity = 0;
  h->size = 0;
}

int row_index_put(HashIntRow *h, long key, Row *value) {
  if (h->size * 2 >= h->capacity) {
    uint32_t new_capacity = h->capacity == 0 ? INITIAL_HASH_CAP : h->capacity * 2;
    RowBucket *new_buckets = calloc(new_capacity, sizeof(RowBucket));
    if (new_buckets == NULL) {
      return ERR_MEMORY;
    }
    for (uint32_t i = 0; i < h->capacity; i++) {
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
  uint32_t steps = 0;

  while (h->buckets[idx].occupied) {
    if (h->buckets[idx].key == key) {
      return ERR_PARSE;
    }
    idx = (idx + 1) & (h->capacity - 1);
    if (++steps >= h->capacity) {
      return ERR_PARSE;
    }
  }
  h->buckets[idx].key = key;
  h->buckets[idx].value = value;
  h->buckets[idx].occupied = 1;
  h->size++;
  return ERR_OK;
}

Row *row_index_get(const HashIntRow *h, long key) {
  if (h->buckets == NULL)
    return NULL;
  uint32_t idx = hash_int(key) & (h->capacity - 1);
  uint32_t steps = 0;

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