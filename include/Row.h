#pragma once
#include "Cell.h"

typedef struct {
  long row_num;
  Cell *cells;
} Row;

Row *row_create(long row_num, int n_columns);
void row_destroy(Row *row, int n_columns);
