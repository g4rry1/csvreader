#pragma once
#include "Cell.h"
#include "Table.h"

int parse_csv(FILE *f, Table *t);
int parse_header(FILE *f, Table *t);
int parse_data_line(const char *line, Table *t);
int parse_cell_value(const char *s, Cell *out, Table *t);
int parse_formula(const char *s, Formula *out, Table *t);
int parse_cell_ref(const char *s, CellRef *out, Table *t);