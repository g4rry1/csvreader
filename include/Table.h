#pragma once
#include "Cell.h"
#include "HashStrInt.h"
#include "Row.h"
#include "HashIntRow.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  char **column_names;
  size_t n_columns;
  HashStrInt col_index;

  long *rows;
  size_t n_rows;
  size_t rows_cap;

  HashIntRow row_index;
} Table;

void table_destroy(Table *t);
int  table_add_row(Table *t, Row *row);

Cell *table_get_cell(Table *t, CellRef ref);

int evaluate_all(Table *t);
int evaluate_cell(Table *t, Cell *cell);

void table_print(const Table *t, FILE *f);