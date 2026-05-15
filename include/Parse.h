#pragma once
#include "Table.h"

int parse_header(FILE *f, Table *t);
int parse_data_line(FILE *f, Table *t);
int parse_cell_value(const char *s, Cell *out);
int parse_formula(const char *s, Formula *out);
const char *parse_cell_ref(const char *s, CellRef *out);
int parse_csv(FILE *f, Table *t);