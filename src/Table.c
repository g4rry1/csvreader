#include "Table.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#include "Cell.h"
#include "HashIntRow.h"
#include "HashStrInt.h"
#include "Row.h"
#include "errors.h"

#define INITIAL_ROWS_CAP 16

#if defined(__GNUC__) || defined(__clang__)
#define ADD_OVERFLOW(a, b, r) __builtin_add_overflow((a), (b), (r))
#define SUB_OVERFLOW(a, b, r) __builtin_sub_overflow((a), (b), (r))
#define MUL_OVERFLOW(a, b, r) __builtin_mul_overflow((a), (b), (r))
#else
static int add_overflow(long a, long b, long *r) {
    if (b > 0 && a > LONG_MAX - b) return 1;
    if (b < 0 && a < LONG_MIN - b) return 1;
    *r = a + b;
    return 0;
}
static int sub_overflow(long a, long b, long *r) {
    if (b < 0 && a > LONG_MAX + b) return 1;
    if (b > 0 && a < LONG_MIN + b) return 1;
    *r = a - b;
    return 0;
}
static int mul_overflow(long a, long b, long *r) {
    if (a == 0 || b == 0) {
        *r = 0;
        return 0;
    }
    if (a > 0 && b > 0 && a > LONG_MAX / b) return 1;
    if (a < 0 && b < 0 && a < LONG_MAX / b) return 1;
    if (a > 0 && b < 0 && b < LONG_MIN / a) return 1;
    if (a < 0 && b > 0 && a < LONG_MIN / b) return 1;
    *r = a * b;
    return 0;
}
#define ADD_OVERFLOW(a, b, r) add_overflow((a), (b), (r))
#define SUB_OVERFLOW(a, b, r) sub_overflow((a), (b), (r))
#define MUL_OVERFLOW(a, b, r) mul_overflow((a), (b), (r))
#endif

int table_add_row(Table *t, Row *row) {
    if (t->n_rows >= t->rows_cap) {
        size_t new_cap = t->rows_cap == 0 ? INITIAL_ROWS_CAP : t->rows_cap * 2;
        long *tmp = realloc(t->rows, new_cap * sizeof(long));
        if (tmp == NULL) {
            return ERR_MEMORY;
        }
        t->rows = tmp;
        t->rows_cap = new_cap;
    }
    int err = row_index_put(&t->row_index, row->row_num, row);
    if (err != ERR_OK) {
        return err;
    }
    t->rows[t->n_rows++] = row->row_num;
    return ERR_OK;
}

void table_print(const Table *t, FILE *f) {
    if (!t || !f) {
        return;
    }

    for (size_t i = 0; i < t->n_columns; i++) {
        fprintf(f, ",");
        fprintf(f, "%s", t->column_names[i]);
    }
    fprintf(f, "\n");

    for (size_t i = 0; i < t->n_rows; i++) {
        fprintf(f, "%ld", t->rows[i]);
        fprintf(f, ",");
        Row *row = row_index_get(&t->row_index, t->rows[i]);
        for (size_t j = 0; j < t->n_columns; j++) {
            if (row->cells[j].kind == CELL_INT) {
                fprintf(f, "%ld", row->cells[j].as.value);
            } else if (row->cells[j].kind == CELL_EMPTY) {
                fprintf(f, "EMPTY");
            } else {
                fprintf(f, "FORMULA");
            }
            if (j < t->n_columns - 1) {
                fprintf(f, ",");
            }
        }
        fprintf(f, "\n");
    }
}

void table_destroy(Table *t) {
    if (!t) {
        return;
    }

    for (size_t i = 0; i < t->n_columns; i++) {
        free(t->column_names[i]);
    }
    free(t->column_names);
    col_index_destroy(&t->col_index);

    for (size_t i = 0; i < t->n_rows; i++) {
        row_destroy(row_index_get(&t->row_index, t->rows[i]), t->n_columns);
    }
    free(t->rows);
    row_index_destroy(&t->row_index);
}

Cell *table_get_cell(Table *t, CellRef ref) {
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

static int resolve_arg(Table *t, const Arg *arg, long *out) {
    if (arg->kind == NUMBER) {
        *out = arg->as.number;
        return ERR_OK;
    }
    Cell *ref_cell = table_get_cell(t, arg->as.ref);
    if (ref_cell == NULL) {
        fprintf(stderr, "Eval error: cell '%s%ld' does not exist\n", arg->as.ref.col_name,
                arg->as.ref.row_num);
        return ERR_EVAL;
    }
    if (ref_cell->kind == CELL_EVALUATING) {
        fprintf(stderr, "Eval error: circular reference detected at '%s%ld'\n",
                arg->as.ref.col_name, arg->as.ref.row_num);
        return ERR_EVAL;
    }
    if (ref_cell->kind == CELL_FORMULA) {
        if (evaluate_cell(t, ref_cell) != ERR_OK) {
            return ERR_EVAL;
        }
        *out = ref_cell->as.value;
        return ERR_OK;
    }
    if (ref_cell->kind == CELL_INT) {
        *out = ref_cell->as.value;
        return ERR_OK;
    }
    fprintf(stderr, "Eval error: cell '%s%ld' is empty\n", arg->as.ref.col_name,
            arg->as.ref.row_num);
    return ERR_EVAL;
}

int evaluate_cell(Table *t, Cell *cell) {
    if (cell->kind != CELL_FORMULA) {
        return ERR_EVAL;
    }

    cell->kind = CELL_EVALUATING;
    Formula *formula = cell->as.formula;
    long arg1;
    long arg2;

    if (resolve_arg(t, &formula->arg1, &arg1) != ERR_OK) {
        cell->kind = CELL_FORMULA;
        return ERR_EVAL;
    }
    if (resolve_arg(t, &formula->arg2, &arg2) != ERR_OK) {
        cell->kind = CELL_FORMULA;
        return ERR_EVAL;
    }

    long result = 0;
    switch (formula->op) {
        case ADD:
            if (ADD_OVERFLOW(arg1, arg2, &result)) {
                fprintf(stderr, "Eval error: integer overflow\n");
                cell->kind = CELL_FORMULA;
                return ERR_EVAL;
            }
            break;
        case SUB:
            if (SUB_OVERFLOW(arg1, arg2, &result)) {
                fprintf(stderr, "Eval error: integer overflow\n");
                cell->kind = CELL_FORMULA;
                return ERR_EVAL;
            }
            break;
        case MUL:
            if (MUL_OVERFLOW(arg1, arg2, &result)) {
                fprintf(stderr, "Eval error: integer overflow\n");
                cell->kind = CELL_FORMULA;
                return ERR_EVAL;
            }
            break;
        case DIV:
            if (arg2 == 0) {
                fprintf(stderr, "Eval error: division by zero\n");
                cell->kind = CELL_FORMULA;
                return ERR_EVAL;
            }
            result = arg1 / arg2;
            break;
        default:
            cell->kind = CELL_FORMULA;
            return ERR_EVAL;
    }

    cell_clear(cell);
    cell->kind = CELL_INT;
    cell->as.value = result;
    return ERR_OK;
}

int evaluate_all(Table *t) {
    for (size_t i = 0; i < t->n_rows; i++) {
        Row *row = row_index_get(&t->row_index, t->rows[i]);
        for (size_t j = 0; j < t->n_columns; j++) {
            Cell *cell = &row->cells[j];
            if (cell->kind == CELL_FORMULA) {
                int err = evaluate_cell(t, cell);
                if (err != ERR_OK) {
                    return err;
                }
            }
        }
    }
    return ERR_OK;
}
