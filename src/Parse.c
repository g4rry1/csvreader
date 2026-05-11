#include "Parse.h"
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

int parse_csv(FILE *f, Table *t) {
  int err = parse_header(f, t);
  if (err)
    return err;

  err = build_column_index(t);
  if (err)
    return err;

  return 0;
}