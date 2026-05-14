#include "Table.h"
#include "Cell.h"
#include "HashIntRow.h"
#include "HashStrInt.h"
#include "Row.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

Row *table_add_row(Table *t, Row *row) {
  if (t->n_rows >= t->rows_cap) {
    int new_cap = t->rows_cap == 0 ? 16 : t->rows_cap * 2;
    long *tmp = realloc(t->rows, new_cap * sizeof(long));
    if (tmp == NULL) {
      return NULL;
    }
    t->rows = tmp;
    t->rows_cap = new_cap;
  }
  row_index_put(&t->row_index, row->row_num, row);
  t->rows[t->n_rows++] = row->row_num;
  return row;
}

void table_print(const Table *t, FILE *f) {
  if (!t || !f)
    return;

  for (int i = 0; i < t->n_columns; i++) {
    fprintf(f, ",");
    fprintf(f, "%s", t->column_names[i]);
  }
  fprintf(f, "\n");

  for (int i = 0; i < t->n_rows; i++) {
    fprintf(f, "%ld", t->rows[i]);
    fprintf(f, ",");
    Row *row = row_index_get(&t->row_index, t->rows[i]);
    for (int j = 0; j < t->n_columns; j++) {
      if (row->cells[j].kind != CELL_INT) {
        fprintf(
            f, "%s",
            row->cells[j].kind == CELL_EMPTY
                ? "EMPTY"
                : (row->cells[j].kind == CELL_FORMULA ? "FORMULA" : "ERROR"));
      } else if (row->cells[j].kind == CELL_INT) {
        fprintf(f, "%ld", row->cells[j].as.value);
      }
      if (j < t->n_columns - 1) {
        fprintf(f, ",");
      }
    }
    fprintf(f, "\n");
  }
}

void table_destroy(Table *t) {
  if (!t)
    return;

  for (int i = 0; i < t->n_columns; i++) {
    free(t->column_names[i]);
  }
  free(t->column_names);
  col_index_destroy(&t->col_index);

  for (int i = 0; i < t->n_rows; i++) {
    row_destroy(row_index_get(&t->row_index, t->rows[i]), t->n_columns);
  }
  free(t->rows);
  row_index_destroy(&t->row_index);
}

Cell *get_cell(Table *t, CellRef ref) {
  int col_index = col_index_get(&t->col_index, ref.col_name);
  if (col_index < 0) {
    return NULL;
  }
  Row *row = row_index_get(&t->row_index, ref.row_num);
  if (row == NULL) {
    return NULL;
  }
  return &row->cells[col_index];
}

int evaluate_cell(Table *t, Cell *cell) {
  if (cell->kind == CELL_FORMULA) {
    cell->kind = CELL_EVALUATING;
    Formula *formula = cell->as.formula;
    long arg1 = 0, arg2 = 0;
    if (formula->arg1.kind == REF) {

      Cell *ref_cell = get_cell(t, formula->arg1.as.ref);
      if (ref_cell == NULL) {
        fprintf(stderr, "Eval error: cell '%s%ld' does not exist\n",
                formula->arg1.as.ref.col_name, formula->arg1.as.ref.row_num);
        return ERR_EVAL;
      }
      if (ref_cell->kind == CELL_EVALUATING) {
        fprintf(stderr, "Eval error: circular reference detected at '%s%ld'\n",
                formula->arg1.as.ref.col_name, formula->arg1.as.ref.row_num);
        return ERR_EVAL;
      }
      if (ref_cell->kind == CELL_FORMULA) {
        if (evaluate_cell(t, ref_cell)) {
          return ERR_EVAL;
        }
        arg1 = ref_cell->as.value;
      } else if (ref_cell->kind == CELL_INT) {
        arg1 = ref_cell->as.value;
      } else {
        fprintf(stderr, "Eval error: cell '%s%ld' is empty\n",
                formula->arg1.as.ref.col_name, formula->arg1.as.ref.row_num);
        return ERR_EVAL;
      }
    } else {
      arg1 = formula->arg1.as.number;
    }

    if (formula->arg2.kind == REF) {
      Cell *ref_cell = get_cell(t, formula->arg2.as.ref);
      if (ref_cell == NULL) {
        fprintf(stderr, "Eval error: cell '%s%ld' does not exist\n",
                formula->arg2.as.ref.col_name, formula->arg2.as.ref.row_num);
        return ERR_EVAL;
      }
      if (ref_cell->kind == CELL_EVALUATING) {
        fprintf(stderr, "Eval error: circular reference detected at '%s%ld'\n",
                formula->arg2.as.ref.col_name, formula->arg2.as.ref.row_num);
        return ERR_EVAL;
      }
      if (ref_cell->kind == CELL_FORMULA) {
        if (evaluate_cell(t, ref_cell)) {
          return ERR_EVAL;
        }
        arg2 = ref_cell->as.value;
      } else if (ref_cell->kind == CELL_INT) {
        arg2 = ref_cell->as.value;
      } else {
        fprintf(stderr, "Eval error: cell '%s%ld' is empty\n",
                formula->arg2.as.ref.col_name, formula->arg2.as.ref.row_num);
        return ERR_EVAL;
      }
    } else {
      arg2 = formula->arg2.as.number;
    }

    long result = 0;

    switch (formula->op) {
    case ADD:
      result = arg1 + arg2;
      break;
    case SUB:
      result = arg1 - arg2;
      break;
    case MUL:
      result = arg1 * arg2;
      break;
    case DIV:
      if (arg2 == 0) {
        fprintf(stderr, "Eval error: division by zero\n");
        return ERR_EVAL;
      }
      result = arg1 / arg2;
      break;
    default:
      return ERR_EVAL;
    }
    cell_clear(cell);
    cell->kind = CELL_INT;
    cell->as.value = result;
    return ERR_OK;
  }
  return ERR_EVAL;
}

int evaluate_all(Table *t) {
  for (int i = 0; i < t->n_rows; i++) {
    Row *row = row_index_get(&t->row_index, t->rows[i]);
    for (int j = 0; j < t->n_columns; j++) {
      Cell *cell = &row->cells[j];
      if (cell->kind == CELL_FORMULA) {
        int err = evaluate_cell(t, cell);
        if (err != ERR_OK)
          return err;
      }
    }
  }
  return ERR_OK;
}