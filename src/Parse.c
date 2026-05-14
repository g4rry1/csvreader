#include "Parse.h"
#include "HashStrInt.h"
#include "Row.h"
#include "Table.h"
#include "errors.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int build_column_index(Table *t) {
  int err = col_index_reserve(&t->col_index, t->n_columns);
  if (err != ERR_OK)
    return err;

  for (int i = 0; i < t->n_columns; i++) {
    err = col_index_put(&t->col_index, t->column_names[i], i);
    if (err != ERR_OK) {
      col_index_destroy(&t->col_index);
      return err;
    }
  }
  return ERR_OK;
}

static int parse_header_fail(char **column_names, int n_columns,
                             char *name_buf, int err) {
  for (int i = 0; i < n_columns; i++) {
    free(column_names[i]);
  }
  free(column_names);
  free(name_buf);
  return err;
}

int parse_header(FILE *f, Table *t) {
  int first = fgetc(f);
  if (first != ',') {
    fprintf(stderr, "Parse error: header must start with ','\n");
    return ERR_PARSE;
  }

  int cap_columns = 16;
  int n_columns = 0;
  char **column_names = malloc(cap_columns * sizeof(char *));
  if (column_names == NULL) {
    return ERR_MEMORY;
  }

  int name_buf_cap = 256;
  int name_buf_len = 0;
  char *name_buf = malloc(name_buf_cap);
  if (name_buf == NULL) {
    free(column_names);
    return ERR_MEMORY;
  }

  int c;
  while ((c = fgetc(f)) != '\n') {
    if (c == EOF)
      break;

    if (c == ',') {
      if (name_buf_len == 0) {
        fprintf(stderr, "Parse error: empty column name in header\n");
        return parse_header_fail(column_names, n_columns, name_buf, ERR_PARSE);
      }

      if (n_columns >= cap_columns) {
        cap_columns *= 2;
        char **tmp = realloc(column_names, cap_columns * sizeof(char *));
        if (tmp == NULL) {
          return parse_header_fail(column_names, n_columns, name_buf, ERR_MEMORY);
        }
        column_names = tmp;
      }

      name_buf[name_buf_len] = '\0';
      column_names[n_columns++] = name_buf;

      name_buf_cap = 256;
      name_buf_len = 0;
      name_buf = malloc(name_buf_cap);
      if (name_buf == NULL) {
        return parse_header_fail(column_names, n_columns, NULL, ERR_MEMORY);
      }
    } else {
      if (name_buf_len + 1 >= name_buf_cap) {
        name_buf_cap *= 2;
        char *tmp = realloc(name_buf, name_buf_cap);
        if (tmp == NULL) {
          return parse_header_fail(column_names, n_columns, name_buf, ERR_MEMORY);
        }
        name_buf = tmp;
      }
      name_buf[name_buf_len++] = (char)c;
    }
  }

  if (n_columns >= cap_columns) {
    int new_cap = cap_columns * 2;
    char **tmp = realloc(column_names, new_cap * sizeof(char *));
    if (tmp == NULL) {
      return parse_header_fail(column_names, n_columns, name_buf, ERR_MEMORY);
    }
    column_names = tmp;
    cap_columns = new_cap;
  }

  if (name_buf_len == 0) {
    fprintf(stderr, "Parse error: empty column name in header\n");
    return parse_header_fail(column_names, n_columns, name_buf, ERR_PARSE);
  }

  name_buf[name_buf_len] = '\0';
  column_names[n_columns++] = name_buf;

  t->column_names = column_names;
  t->n_columns = n_columns;
  return ERR_OK;
}

const char *parse_cell_ref(const char *s, CellRef *out) {
  int cap = 16;
  char *column_name_buf = malloc(cap);
  if (column_name_buf == NULL) {
    return NULL;
  }
  int column_name_len = 0;

  while ((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z')) {
    if (column_name_len + 1 >= cap) {
      cap *= 2;
      char *tmp = realloc(column_name_buf, cap);
      if (tmp == NULL) {
        free(column_name_buf);
        return NULL;
      }
      column_name_buf = tmp;
    }
    column_name_buf[column_name_len++] = *s;
    s++;
  }

  if (column_name_len == 0) {
    free(column_name_buf);
    return NULL;
  }

  column_name_buf[column_name_len] = '\0';

  char *endptr;
  long number_row = strtol(s, &endptr, 10);
  if (endptr == s) {
    fprintf(stderr, "Parse error: missing row number in cell reference '%s'\n", column_name_buf);
    free(column_name_buf);
    return NULL;
  }
  if (number_row <= 0) {
    fprintf(stderr, "Parse error: row number in cell reference '%s%ld' must be positive\n",
            column_name_buf, number_row);
    free(column_name_buf);
    return NULL;
  }
  s = endptr;
  out->col_name = column_name_buf;
  out->row_num = number_row;
  return s;
}

int parse_formula(const char *s, Formula *out) {
  char *endptr;
  long number1 = strtol(s, &endptr, 10);
  if (s == endptr) {
    CellRef ref;
    s = parse_cell_ref(s, &ref);
    if (s == NULL)
      return ERR_PARSE;
    out->arg1.kind = REF;
    out->arg1.as.ref = ref;
  } else {
    out->arg1.kind = NUMBER;
    out->arg1.as.number = number1;
    s = endptr;
  }

  int op = *s++;
  if (op == '\0') {
    fprintf(stderr, "Parse error: missing operator in formula (expected +, -, *, /)\n");
    arg_clear(&out->arg1);
    return ERR_PARSE;
  }
  if (op != '+' && op != '-' && op != '*' && op != '/') {
    fprintf(stderr, "Parse error: invalid operator '%c' in formula (expected +, -, *, /)\n", op);
    arg_clear(&out->arg1);
    return ERR_PARSE;
  }
  switch (op) {
  case '+':
    out->op = ADD;
    break;
  case '-':
    out->op = SUB;
    break;
  case '*':
    out->op = MUL;
    break;
  case '/':
    out->op = DIV;
    break;
  }

  long number2 = strtol(s, &endptr, 10);
  if (s == endptr) {
    CellRef ref;
    s = parse_cell_ref(s, &ref);
    if (s == NULL) {
      arg_clear(&out->arg1);
      return ERR_PARSE;
    }
    out->arg2.kind = REF;
    out->arg2.as.ref = ref;
  } else {
    out->arg2.kind = NUMBER;
    out->arg2.as.number = number2;
    s = endptr;
  }

  if (*s != '\0') {
    fprintf(stderr, "Parse error: unexpected characters after formula: '%s'\n", s);
    arg_clear(&out->arg1);
    arg_clear(&out->arg2);
    return ERR_PARSE;
  }
  return ERR_OK;
}

int parse_cell_value(const char *s, Cell *out) {
  char *endptr;
  long value = strtol(s, &endptr, 10);
  if (*endptr == '\0') {
    out->kind = CELL_INT;
    out->as.value = value;
    return ERR_OK;
  } else if (s[0] == '=') {
    out->as.formula = malloc(sizeof(Formula));
    if (!out->as.formula)
      return ERR_MEMORY;
    if (parse_formula(s + 1, out->as.formula) != ERR_OK) {
      free(out->as.formula);
      return ERR_PARSE;
    }
    out->kind = CELL_FORMULA;
  } else {
    return ERR_PARSE;
  }
  return ERR_OK;
}

int parse_data_line(FILE *f, Table *t) {
  int peek = fgetc(f);
  if (peek == EOF)
    return ERR_EOF;
  ungetc(peek, f);

  int c;
  int cap = 32;
  int size = 0;
  char *cell_buf = malloc(cap);
  if (cell_buf == NULL) {
    return ERR_MEMORY;
  }
  while ((c = fgetc(f)) != ',' && c != '\n' && c != EOF) {
    if (size + 1 >= cap) {
      free(cell_buf);
      return ERR_MEMORY;
    }
    cell_buf[size++] = (char)c;
  }
  cell_buf[size] = '\0';

  char *endptr;
  long row_num = strtol(cell_buf, &endptr, 10);
  if (*endptr != '\0' || row_num <= 0) {
    fprintf(stderr, "Parse error: invalid row number '%s' (must be a positive integer)\n", cell_buf);
    free(cell_buf);
    return ERR_PARSE;
  }

  if (c != ',') {
    fprintf(stderr, "Parse error: row %ld has 0 cells, expected %d\n", row_num, t->n_columns);
    free(cell_buf);
    return ERR_PARSE;
  }

  Row *row = row_create(row_num, t->n_columns);
  if (row == NULL) {
    free(cell_buf);
    return ERR_MEMORY;
  }

  size = 0;
  int number_cells = 0;

  while ((c = fgetc(f)) != EOF && c != '\n') {
    if (number_cells >= t->n_columns) {
      fprintf(stderr, "Parse error: row %ld has more than %d cells\n", row_num, t->n_columns);
      row_destroy(row, t->n_columns);
      free(cell_buf);
      return ERR_PARSE;
    }
    if (c == ',') {
      if (size > 0) {
        cell_buf[size] = '\0';
        int err = parse_cell_value(cell_buf, &row->cells[number_cells]);
        if (err != ERR_OK) {
          fprintf(stderr, "Parse error: invalid cell '%s' in row %ld, column '%s'\n",
                  cell_buf, row_num, t->column_names[number_cells]);
          row_destroy(row, t->n_columns);
          free(cell_buf);
          return ERR_PARSE;
        }
      } else {
        row->cells[number_cells].kind = CELL_EMPTY;
      }
      number_cells++;
      size = 0;
    } else {
      if (size + 1 >= cap) {
        cap *= 2;
        char *tmp = realloc(cell_buf, cap);
        if (tmp == NULL) {
          row_destroy(row, t->n_columns);
          free(cell_buf);
          return ERR_MEMORY;
        }
        cell_buf = tmp;
      }
      cell_buf[size++] = (char)c;
    }
  }

  if (number_cells >= t->n_columns) {
    fprintf(stderr, "Parse error: row %ld has more than %d cells\n", row_num, t->n_columns);
    row_destroy(row, t->n_columns);
    free(cell_buf);
    return ERR_PARSE;
  }

  if (size == 0) {
    row->cells[number_cells].kind = CELL_EMPTY;
  } else {
    cell_buf[size] = '\0';
    int err = parse_cell_value(cell_buf, &row->cells[number_cells]);
    if (err != ERR_OK) {
      fprintf(stderr, "Parse error: invalid cell '%s' in row %ld, column '%s'\n",
              cell_buf, row_num, t->column_names[number_cells]);
      row_destroy(row, t->n_columns);
      free(cell_buf);
      return ERR_PARSE;
    }
  }
  number_cells++;

  if (number_cells != t->n_columns) {
    fprintf(stderr, "Parse error: row %ld has %d cells, expected %d\n",
            row_num, number_cells, t->n_columns);
    row_destroy(row, t->n_columns);
    free(cell_buf);
    return ERR_PARSE;
  }

  if (table_add_row(t, row) == NULL) {
    row_destroy(row, t->n_columns);
    free(cell_buf);
    return ERR_MEMORY;
  }

  free(cell_buf);
  return ERR_OK;
}

int parse_csv(FILE *f, Table *t) {
  int err = parse_header(f, t);
  if (err != ERR_OK)
    return err;

  err = build_column_index(t);
  if (err != ERR_OK)
    return err;

  int result;
  while ((result = parse_data_line(f, t)) == ERR_OK) {
  }
  return result == ERR_EOF ? ERR_OK : result;
}
