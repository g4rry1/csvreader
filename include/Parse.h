#pragma once
#include "Cell.h"
#include "Table.h"

int parse_csv(FILE *f, Table *t);
int parse_header(FILE *f, Table *t);
int parse_data_line(FILE *f, Table *t);
int parse_cell_value(const char *s, Cell *out, Table *t);
int parse_formula(char *s, Formula *out, Table *t);
char *parse_cell_ref(char *s, CellRef *out, Table *t);