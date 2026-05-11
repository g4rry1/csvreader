#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum { ADD, SUB, MUL, DIV } Operation;

typedef enum {
  CELL_EMPTY,
  CELL_INT,
  CELL_FORMULA,
  CELL_EVALUATING,
  CELL_ERROR
} CellKind;

typedef enum { NUMBER, REF } ArgKind;

typedef struct {
  int col_index;
  int row_num;
} CellRef;

typedef struct {
  ArgKind kind;
  union {
    long number;
    CellRef ref;
  } as;
} Arg;

typedef struct {
  Arg arg1;
  Arg arg2;
  Operation op;
} Formula;

typedef struct {
  CellKind kind;
  union {
    long value;
    Formula *formula;
  } as;
} Cell;

typedef struct {
  int row_num;
  Cell *cells;
} Row;

typedef struct {
  int occupied;
  char *key;
  int value;
} ColIdxBucket;

typedef struct {
  ColIdxBucket *buckets;
  int capacity;
  int size;
} HashStrInt;

typedef struct {
  int occupied;
  int key;
  Row *value;
} RowBucket;

typedef struct {
  RowBucket *buckets;
  int capacity;
  int size;
} HashIntRow;

typedef struct {
  char **column_names;
  int n_columns;
  HashStrInt column_index;

  HashIntRow rows;

  Row **rows_in_order;
  int n_rows;
  int cap_rows_in_order;
} Table;

void col_index_init(HashStrInt *h, int capacity);
void col_index_destroy(HashStrInt *h);
void col_index_put(HashStrInt *h, const char *key, int value);
int col_index_get(const HashStrInt *h, const char *key);

void rows_init(HashIntRow *h, int initial_capacity);
void rows_destroy(HashIntRow *h);
void rows_put(HashIntRow *h, int key, Row *value);
Row *rows_get(const HashIntRow *h, int key);

Row *row_create(int row_num, int n_columns);
void row_destroy(Row *row, int n_columns);

void cell_clear(Cell *cell);

void table_init(Table *t);
void table_destroy(Table *t);
int table_add_column(Table *t, const char *name);
Row *table_add_row(Table *t, int row_num);
Cell *table_get_cell(Table *t, int col_index, int row_num);
int table_find_column_index(const Table *t, const char *name);

int parse_csv(FILE *f, Table *t);
int parse_header(FILE *f, Table *t);
int parse_data_line(const char *line, Table *t);
int parse_cell_value(const char *s, Cell *out, Table *t);
int parse_formula(const char *s, Formula *out, Table *t);
int parse_cell_ref(const char *s, CellRef *out, Table *t);

int evaluate_all(Table *t);
long evaluate_cell(Table *t, Cell *cell, int *ok);
long evaluate_arg(Table *t, Arg *arg, int *ok);

void table_print(const Table *t, FILE *f);
void cell_print(const Cell *cell, FILE *f);