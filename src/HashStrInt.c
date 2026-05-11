#include "HashStrInt.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

uint32_t hash_str(const char *s) {
  uint32_t h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

uint32_t next_power_of_two(uint32_t n) {
  uint32_t p = 1;
  while (p < n)
    p *= 2;
  return p;
}

char *str_copy(const char *s) {
  if (s == NULL)
    return NULL;
  size_t len = strlen(s) + 1;
  char *copy = malloc(len);
  if (copy == NULL)
    return NULL;
  memcpy(copy, s, len);
  return copy;
}

int col_index_init(HashStrInt *h, int n_strs) {
  uint32_t capacity = next_power_of_two(n_strs * 2);
  h->buckets = calloc(capacity, sizeof(ColIdxBucket));
  if (h->buckets == NULL) {
    return -1;
  }
  h->capacity = capacity;
  h->size = 0;
  return 0;
}

void col_index_destroy(HashStrInt *h) {
  for (int i = 0; i < h->capacity; i++) {
    if (h->buckets[i].occupied) {
      free(h->buckets[i].key);
    }
  }
  free(h->buckets);
  h->buckets = NULL;
  h->capacity = 0;
  h->size = 0;
}

int col_index_put(HashStrInt *h, const char *key, int value) {
  uint32_t idx = hash_str(key) & (h->capacity - 1);
  int steps = 0;

  while (h->buckets[idx].occupied) {
    if (strcmp(h->buckets[idx].key, key) == 0) {
      return -1;
    }
    idx = (idx + 1) & (h->capacity - 1);
    if (++steps >= h->capacity)
      return -1;
  }

  char *key_copy = str_copy(key);
  if (!key_copy)
    return -1;

  h->buckets[idx].key = key_copy;
  h->buckets[idx].occupied = 1;
  h->buckets[idx].value = value;
  h->size++;
  return 0;
}

int col_index_get(const HashStrInt *h, const char *key) {
  if (h->buckets == NULL)
    return -1;
  uint32_t idx = hash_str(key) & (h->capacity - 1);
  int steps = 0;

  while (h->buckets[idx].occupied) {
    if (strcmp(h->buckets[idx].key, key) == 0) {
      return h->buckets[idx].value;
    }
    idx = (idx + 1) & (h->capacity - 1);
    if (++steps >= h->capacity)
      break;
  }

  return -1;
}
