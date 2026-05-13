#pragma once
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
  char *col_name;
  long row_num;
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

void arg_clear(Arg *arg);
void cell_clear(Cell *cell);