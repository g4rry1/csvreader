#include "Cell.h"
#include <stdlib.h>

void arg_clear(Arg *arg) {
  if (arg->kind == REF)
    free(arg->as.ref.col_name);
}

void cell_clear(Cell *cell) {
  if (cell->kind == CELL_FORMULA) {
    arg_clear(&cell->as.formula->arg1);
    arg_clear(&cell->as.formula->arg2);
    free(cell->as.formula);
  }
  cell->kind = CELL_EMPTY;
}