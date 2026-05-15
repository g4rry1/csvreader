#include "Row.h"
#include <stdlib.h>

Row *row_create(long row_num, size_t n_columns) {
  Row *row = malloc(sizeof(Row));
  if (row == NULL) {
    return NULL;
  }
  row->row_num = row_num;
  row->cells = calloc(n_columns, sizeof(Cell));
  if (row->cells == NULL) {
    free(row);
    return NULL;
  }
  return row;
}

void row_destroy(Row *row, size_t n_columns) {
  for (size_t i = 0; i < n_columns; i++) {
    cell_clear(&row->cells[i]);
  }
  free(row->cells);
  free(row);
}
