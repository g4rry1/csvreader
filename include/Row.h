#pragma once
#include "Cell.h"

typedef struct {
  int row_num;
  Cell *cells;
} Row;

Row *row_create(int row_num, int n_columns);
void row_destroy(Row *row, int n_columns);
