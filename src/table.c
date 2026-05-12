#include "Table.h"
#include "HashStrInt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

void table_init(Table *t) {
  t->column_names = NULL;
  t->n_columns = 0;
  col_index_init(&t->column_index, 16);

  rows_init(&t->rows, 16);

  t->rows_in_order = NULL;
  t->n_rows = 0;
  t->cap_rows_in_order = 0;
}
