#pragma once
#include "Cell.h"
#include "HashStrInt.h"
#include "Row.h"
#include <HashIntRow.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
  char **column_names;
  int n_columns;
  HashStrInt col_index;

  long *rows;
  int n_rows;
  int rows_cap;

  HashIntRow row_index;
} Table;

void table_destroy(Table *t);
Row *table_add_row(Table *t, Row *row);

Cell *get_cell(Table *t, CellRef ref);

int evaluate_all(Table *t);
int evaluate_cell(Table *t, Cell *cell);

void table_print(const Table *t, FILE *f);