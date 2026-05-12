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
  HashStrInt column_index;

  HashIntRow rows;

  Row **rows_in_order;
  int n_rows;
  int cap_rows_in_order;
} Table;

void table_init(Table *t);
void table_destroy(Table *t);
int table_add_column(Table *t, const char *name);
Row *table_add_row(Table *t, Row *row);
Cell *table_get_cell(Table *t, int col_index, int row_num);
int table_find_column_index(const Table *t, const char *name);

int evaluate_all(Table *t);
long evaluate_cell(Table *t, Cell *cell, int *ok);
long evaluate_arg(Table *t, Arg *arg, int *ok);

void table_print(const Table *t, FILE *f);
void cell_print(const Cell *cell, FILE *f);