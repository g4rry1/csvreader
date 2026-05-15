#pragma once
#include <stddef.h>

#include "Cell.h"

typedef struct {
    long row_num;
    Cell *cells;
} Row;

Row *row_create(long row_num, size_t n_columns);
void row_destroy(Row *row, size_t n_columns);
