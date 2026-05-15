#include "Parse.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Cell.h"
#include "HashIntRow.h"
#include "HashStrInt.h"
#include "Row.h"
#include "Table.h"
#include "errors.h"

#define ERR_EOF 1                /* end of input — internal signal, never leaves parse_csv */
#define INITIAL_COLS_CAP 16      /* initial capacity of column_names array */
#define INITIAL_NAME_BUF_CAP 256 /* initial buffer for a single column name */
#define INITIAL_COL_REF_CAP 16   /* initial buffer for a column name in a cell ref */
#define ROW_NUM_BUF_CAP 32       /* fixed buffer for row number (long fits in 20 chars) */
#define INITIAL_CELL_BUF_CAP 32  /* initial buffer for a cell value string */

static int is_col_char(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }

static int build_column_index(Table *t) {
    int err = col_index_reserve(&t->col_index, t->n_columns);
    if (err != ERR_OK) {
        return err;
    }

    for (size_t i = 0; i < t->n_columns; i++) {
        err = col_index_put(&t->col_index, t->column_names[i], (int)i);
        if (err != ERR_OK) {
            col_index_destroy(&t->col_index);
            return err;
        }
    }
    return ERR_OK;
}

static int parse_header_fail(char **column_names, size_t n_columns, char *name_buf, int err) {
    for (size_t i = 0; i < n_columns; i++) {
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

    size_t cap_columns = INITIAL_COLS_CAP;
    size_t n_columns = 0;
    char **column_names = malloc(cap_columns * sizeof(char *));
    if (column_names == NULL) {
        return ERR_MEMORY;
    }

    size_t name_buf_cap = INITIAL_NAME_BUF_CAP;
    size_t name_buf_len = 0;
    char *name_buf = malloc(name_buf_cap);
    if (name_buf == NULL) {
        free(column_names);
        return ERR_MEMORY;
    }

    int c;
    while ((c = fgetc(f)) != '\n' && c != '\r' && c != EOF) {
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

            name_buf_cap = INITIAL_NAME_BUF_CAP;
            name_buf_len = 0;
            name_buf = malloc(name_buf_cap);
            if (name_buf == NULL) {
                return parse_header_fail(column_names, n_columns, NULL, ERR_MEMORY);
            }
        } else {
            if (!is_col_char(c)) {
                fprintf(stderr, "Parse error: invalid character '%c' in column name\n", c);
                return parse_header_fail(column_names, n_columns, name_buf, ERR_PARSE);
            }
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

    /* Consume '\n' that follows '\r' in CRLF files */
    if (c == '\r') {
        int next = fgetc(f);
        if (next != '\n') {
            ungetc(next, f);
        }
    }

    if (n_columns >= cap_columns) {
        size_t new_cap = cap_columns * 2;
        char **tmp = realloc(column_names, new_cap * sizeof(char *));
        if (tmp == NULL) {
            return parse_header_fail(column_names, n_columns, name_buf, ERR_MEMORY);
        }
        column_names = tmp;
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
    size_t cap = INITIAL_COL_REF_CAP;
    char *column_name_buf = malloc(cap);
    if (column_name_buf == NULL) {
        return NULL;
    }
    size_t column_name_len = 0;

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
    errno = 0;
    long number_row = strtol(s, &endptr, 10);
    if (endptr == s) {
        fprintf(stderr, "Parse error: missing row number in cell reference '%s'\n",
                column_name_buf);
        free(column_name_buf);
        return NULL;
    }
    if (errno == ERANGE) {
        fprintf(stderr, "Parse error: row number overflow in cell reference '%s'\n",
                column_name_buf);
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

static int is_number_start(char c) { return c == '-' || (c >= '0' && c <= '9'); }

int parse_formula(const char *s, Formula *out) {
    char *endptr = (char *)s;
    long number1 = 0;
    if (is_number_start(*s)) {
        errno = 0;
        number1 = strtol(s, &endptr, 10);
        if (errno == ERANGE) {
            fprintf(stderr, "Parse error: integer overflow in formula\n");
            return ERR_PARSE;
        }
    }
    if (s == endptr) {
        CellRef ref = {0};
        s = parse_cell_ref(s, &ref);
        if (s == NULL) {
            free(ref.col_name);
            return ERR_PARSE;
        }
        out->arg1.kind = REF;
        out->arg1.as.ref = ref;
    } else {
        out->arg1.kind = NUMBER;
        out->arg1.as.number = number1;
        s = endptr;
    }

    int op = (unsigned char)*s++;
    if (op == '\0') {
        fprintf(stderr, "Parse error: missing operator in formula (expected +, -, *, /)\n");
        arg_clear(&out->arg1);
        return ERR_PARSE;
    }
    if (op != '+' && op != '-' && op != '*' && op != '/') {
        fprintf(stderr, "Parse error: invalid operator '%c' in formula (expected +, -, *, /)\n",
                op);
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
        default:
            break;
    }

    endptr = (char *)s;
    long number2 = 0;
    if (is_number_start(*s)) {
        errno = 0;
        number2 = strtol(s, &endptr, 10);
        if (errno == ERANGE) {
            fprintf(stderr, "Parse error: integer overflow in formula\n");
            arg_clear(&out->arg1);
            return ERR_PARSE;
        }
    }
    if (s == endptr) {
        CellRef ref = {0};
        s = parse_cell_ref(s, &ref);
        if (s == NULL) {
            free(ref.col_name);
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
    if (s[0] == '=') {
        out->as.formula = malloc(sizeof(Formula));
        if (!out->as.formula) {
            return ERR_MEMORY;
        }
        if (parse_formula(s + 1, out->as.formula) != ERR_OK) {
            free(out->as.formula);
            return ERR_PARSE;
        }
        out->kind = CELL_FORMULA;
        return ERR_OK;
    }
    /* Reject whitespace, empty string, and anything that isn't a signed integer
     */
    if (s[0] != '-' && (s[0] < '0' || s[0] > '9')) {
        return ERR_PARSE;
    }
    errno = 0;
    char *endptr;
    long value = strtol(s, &endptr, 10);
    if (endptr == s || *endptr != '\0' || errno == ERANGE) {
        return ERR_PARSE;
    }
    out->kind = CELL_INT;
    out->as.value = value;
    return ERR_OK;
}

int parse_data_line(FILE *f, Table *t) {
    int peek = fgetc(f);
    if (peek == EOF) {
        return ERR_EOF;
    }
    ungetc(peek, f);

    int c;
    size_t cap = ROW_NUM_BUF_CAP;
    size_t size = 0;
    char *cell_buf = malloc(cap);
    if (cell_buf == NULL) {
        return ERR_MEMORY;
    }
    while ((c = fgetc(f)) != ',' && c != '\n' && c != '\r' && c != EOF) {
        if (size + 1 >= cap) {
            fprintf(stderr, "Parse error: row number too long\n");
            free(cell_buf);
            return ERR_PARSE;
        }
        cell_buf[size++] = (char)c;
    }
    if (c == '\r') {
        int next = fgetc(f);
        if (next != '\n') {
            ungetc(next, f);
        }
    }
    cell_buf[size] = '\0';

    char *endptr;
    errno = 0;
    long row_num = strtol(cell_buf, &endptr, 10);
    if (*endptr != '\0' || row_num <= 0 || errno == ERANGE) {
        fprintf(stderr, "Parse error: invalid row number '%s' (must be a positive integer)\n",
                cell_buf);
        free(cell_buf);
        return ERR_PARSE;
    }

    if (c != ',') {
        fprintf(stderr, "Parse error: row %ld has 0 cells, expected %zu\n", row_num, t->n_columns);
        free(cell_buf);
        return ERR_PARSE;
    }

    if (row_index_get(&t->row_index, row_num) != NULL) {
        fprintf(stderr, "Parse error: duplicate row number %ld\n", row_num);
        free(cell_buf);
        return ERR_PARSE;
    }

    Row *row = row_create(row_num, t->n_columns);
    if (row == NULL) {
        free(cell_buf);
        return ERR_MEMORY;
    }

    size = 0;
    size_t number_cells = 0;

    while ((c = fgetc(f)) != EOF && c != '\n' && c != '\r') {
        if (number_cells >= t->n_columns) {
            fprintf(stderr, "Parse error: row %ld has more than %zu cells\n", row_num,
                    t->n_columns);
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
    if (c == '\r') {
        int next = fgetc(f);
        if (next != '\n') {
            ungetc(next, f);
        }
    }

    if (number_cells >= t->n_columns) {
        fprintf(stderr, "Parse error: row %ld has more than %zu cells\n", row_num, t->n_columns);
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
            fprintf(stderr, "Parse error: invalid cell '%s' in row %ld, column '%s'\n", cell_buf,
                    row_num, t->column_names[number_cells]);
            row_destroy(row, t->n_columns);
            free(cell_buf);
            return ERR_PARSE;
        }
    }
    number_cells++;

    if (number_cells != t->n_columns) {
        fprintf(stderr, "Parse error: row %ld has %zu cells, expected %zu\n", row_num, number_cells,
                t->n_columns);
        row_destroy(row, t->n_columns);
        free(cell_buf);
        return ERR_PARSE;
    }

    int add_err = table_add_row(t, row);
    if (add_err != ERR_OK) {
        row_destroy(row, t->n_columns);
        free(cell_buf);
        return add_err;
    }

    free(cell_buf);
    return ERR_OK;
}

int parse_csv(FILE *f, Table *t) {
    int err = parse_header(f, t);
    if (err != ERR_OK) {
        return err;
    }
    if (ferror(f)) {
        fprintf(stderr, "Read error\n");
        return ERR_PARSE;
    }

    err = build_column_index(t);
    if (err != ERR_OK) {
        return err;
    }

    int result;
    while ((result = parse_data_line(f, t)) == ERR_OK) {
    }
    if (result != ERR_EOF) {
        return result;
    }
    if (ferror(f)) {
        fprintf(stderr, "Read error\n");
        return ERR_PARSE;
    }
    return ERR_OK;
}
