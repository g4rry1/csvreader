#include "Parse.h"
#include "HashIntRow.h"
#include "HashStrInt.h"
#include "Row.h"
#include "Table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int build_column_index(Table *t) {
  if (col_index_init(&t->column_index, t->n_columns) != 0) {
    return -1;
  }

  for (int i = 0; i < t->n_columns; i++) {
    if (col_index_put(&t->column_index, t->column_names[i], i) != 0) {
      col_index_destroy(&t->column_index);
      return -1;
    }
  }
  return 0;
}

static int parse_header_fail(char **column_names, int n_columns,
                             char *name_buf) {
  for (int i = 0; i < n_columns; i++) {
    free(column_names[i]);
  }
  free(column_names);
  free(name_buf);
  return -1;
}

int parse_header(FILE *f, Table *t) {
  int first = fgetc(f);
  if (first != ',') {
    return -1;
  }

  int cap_columns = 16;
  int n_columns = 0;
  char **column_names = malloc(cap_columns * sizeof(char *));
  if (column_names == NULL) {
    return -1;
  }

  int name_buf_cap = 256;
  int name_buf_len = 0;
  char *name_buf = malloc(name_buf_cap);
  if (name_buf == NULL) {
    free(column_names);
    return -1;
  }

  int c;
  while ((c = fgetc(f)) != '\n') {
    if (c == EOF)
      break;

    if (c == ',') {
      if (name_buf_len == 0) {
        return parse_header_fail(column_names, n_columns, name_buf);
      }

      if (n_columns >= cap_columns) {
        cap_columns *= 2;
        char **tmp = realloc(column_names, cap_columns * sizeof(char *));
        if (tmp == NULL) {
          return parse_header_fail(column_names, n_columns, name_buf);
        }
        column_names = tmp;
      }

      name_buf[name_buf_len] = '\0';
      column_names[n_columns++] = name_buf;

      name_buf_cap = 256;
      name_buf_len = 0;
      name_buf = malloc(name_buf_cap);
      if (name_buf == NULL) {
        return parse_header_fail(column_names, n_columns, NULL);
      }
    } else {
      if (name_buf_len + 1 >= name_buf_cap) {
        name_buf_cap *= 2;
        char *tmp = realloc(name_buf, name_buf_cap);
        if (tmp == NULL) {
          return parse_header_fail(column_names, n_columns, name_buf);
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
      return parse_header_fail(column_names, n_columns, name_buf);
    }
    column_names = tmp;
    cap_columns = new_cap;
  }

  if (name_buf_len == 0) {
    return parse_header_fail(column_names, n_columns, name_buf);
  }

  name_buf[name_buf_len] = '\0';
  column_names[n_columns++] = name_buf;

  t->column_names = column_names;
  t->n_columns = n_columns;
  return 0;
}

char *parse_cell_ref(char *s, CellRef *out, Table *t) {
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

  long number_row = strtol(s, NULL, 10);
  if (number_row == 0) {
    free(column_name_buf);
    return NULL;
  }
  out->col_name = column_name_buf;
  out->row_num = number_row;
  return s;
}

int parse_formula(char *s, Formula *out, Table *t) {
  char *endptr;
  long number1 = strtol(s, &endptr, 10);
  if (s == endptr) {
    CellRef ref;
    s = parse_cell_ref(s, &ref, t);
    if (s == NULL) {
      return -1;
    }
    out->arg1.kind = REF;
    out->arg1.as.ref = ref;
  } else {
    out->arg1.kind = NUMBER;
    out->arg1.as.number = number1;
  }
  int op = *s++;
  if (op != '+' && op != '-' && op != '*' && op != '/') {
    return -1;
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
    s = parse_cell_ref(s, &ref, t);
    if (s == NULL) {
      return -1;
    }
    out->arg2.kind = REF;
    out->arg2.as.ref = ref;
  } else {
    out->arg2.kind = NUMBER;
    out->arg2.as.number = number2;
  }
  if (*s != '\0') {
    return -1;
  }
  return 0;
}

int parse_cell_value(const char *s, Cell *out, Table *t) {
  char *endptr;
  long value = strtol(s, &endptr, 10);
  if (*endptr == '\0') {
    out->kind = CELL_INT;
    out->as.value = value;
    return 0;
  } else if (s[0] == '=') {
    if (parse_formula(s + 1, out->as.formula, t) != 0) {
      return -1;
    }
    out->kind = CELL_FORMULA;
  } else {
    return -1;
  }
  return 0;
}

int parse_data_line(FILE *f, Table *t) {
  int c;
  int cap = 32;
  int size = 0;
  char *cell_buf = malloc(cap);
  if (cell_buf == NULL) {
    return -1;
  }
  while ((c = fgetc(f)) != ',' && c != EOF) {
    if (size + 1 >= cap) {
      cap *= 2;
      char *tmp = realloc(cell_buf, cap);
      if (tmp == NULL) {
        free(cell_buf);
        return -1;
      }
      cell_buf = tmp;
    }
    cell_buf[size++] = (char)c;
  }
  cell_buf[size] = '\0';

  char *endptr;
  long value = strtol(cell_buf, &endptr, 10);
  if (*endptr != '\0') {
    free(cell_buf);
    return -1;
  }

  Row *row = row_create(value, t->n_columns);
  if (row == NULL) {
    free(cell_buf);
    return -1;
  }

  size = 0;

  int number_cells = 0;

  while ((c = fgetc(f)) != EOF && c != '\n') {
    if (c == ',') {
      if (size > 0) {
        cell_buf[size] = '\0';
        if (parse_cell_value(cell_buf, &row->cells[number_cells], t) != 0) {
          row_destroy(row, t->n_columns);
          free(cell_buf);
          return -1;
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
          free(cell_buf);
          return -1;
        }
        cell_buf = tmp;
      }
      cell_buf[size++] = (char)c;
    }
  }

  if (parse_cell_value(cell_buf, &row->cells[number_cells], t) != 0) {
    row_destroy(row, t->n_columns);
    free(cell_buf);
    return -1;
  }
  number_cells++;

  if (number_cells != t->n_columns) {
    row_destroy(row, t->n_columns);
    free(cell_buf);
    return -1;
  }

  table_add_row(t, row);
  rows_put(&t->rows, row->row_num, row);

  free(cell_buf);
  return 0;
}

int parse_csv(FILE *f, Table *t) {
  int err = parse_header(f, t);
  if (err)
    return err;

  err = build_column_index(t);
  if (err)
    return err;

  int result = 0;
  while ((result = parse_data_line(f, t)) == 0) {
  }
  if (result != EOF) {
    return result;
  }

  return 0;
}