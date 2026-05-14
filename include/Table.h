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
int table_add_column(Table *t, const char *name);
Row *table_add_row(Table *t, Row *row);
Cell *table_get_cell(Table *t, int col_index, long row_num);
int table_find_column_index(const Table *t, const char *name);

Cell *get_cell(Table *t, CellRef ref);

int evaluate_all(Table *t);
int evaluate_cell(Table *t, Cell *cell);
long evaluate_arg(Table *t, Arg *arg, int *ok);

void table_print(const Table *t, FILE *f);
void cell_print(const Cell *cell, FILE *f);