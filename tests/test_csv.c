/* Tests for csvreader: parsing and evaluation */
#include <setjmp.h>  // IWYU pragma: keep
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include "Cell.h"
#include "Parse.h"
#include "Table.h"
#include "errors.h"

/* ------------------------------------------------------------------ helpers */

static FILE *make_stream(const char *s) { return fmemopen((void *)s, strlen(s), "r"); }

/* ================================================================ parse_header
 */

static void test_header_valid_multiple_columns(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B,Cell\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_OK);
    assert_int_equal(t.n_columns, 3);
    assert_string_equal(t.column_names[0], "A");
    assert_string_equal(t.column_names[1], "B");
    assert_string_equal(t.column_names[2], "Cell");
    table_destroy(&t);
    fclose(f);
}

static void test_header_single_column(void **state) {
    (void)state;
    FILE *f = make_stream(",X\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_OK);
    assert_int_equal(t.n_columns, 1);
    assert_string_equal(t.column_names[0], "X");
    table_destroy(&t);
    fclose(f);
}

static void test_header_no_newline_eof(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_OK);
    assert_int_equal(t.n_columns, 2);
    table_destroy(&t);
    fclose(f);
}

static void test_header_no_leading_comma(void **state) {
    (void)state;
    FILE *f = make_stream("A,B\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_PARSE);
    fclose(f);
}

static void test_header_empty_column_between(void **state) {
    (void)state;
    FILE *f = make_stream(",A,,B\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_PARSE);
    fclose(f);
}

static void test_header_trailing_comma_empty_last(void **state) {
    (void)state;
    /* ",A,B," → last column name is empty */
    FILE *f = make_stream(",A,B,\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_PARSE);
    fclose(f);
}

/* ================================================================
 * parse_cell_ref */

static void test_cellref_valid_single_char(void **state) {
    (void)state;
    CellRef ref;
    const char *end = parse_cell_ref("A1", &ref);
    assert_non_null(end);
    assert_string_equal(ref.col_name, "A");
    assert_int_equal(ref.row_num, 1);
    assert_string_equal(end, "");
    free(ref.col_name);
}

static void test_cellref_valid_multichar_col(void **state) {
    (void)state;
    CellRef ref;
    const char *end = parse_cell_ref("Cell30", &ref);
    assert_non_null(end);
    assert_string_equal(ref.col_name, "Cell");
    assert_int_equal(ref.row_num, 30);
    free(ref.col_name);
}

static void test_cellref_lowercase_letters(void **state) {
    (void)state;
    CellRef ref;
    const char *end = parse_cell_ref("abc5", &ref);
    assert_non_null(end);
    assert_string_equal(ref.col_name, "abc");
    assert_int_equal(ref.row_num, 5);
    free(ref.col_name);
}

static void test_cellref_no_letters(void **state) {
    (void)state;
    CellRef ref;
    assert_null(parse_cell_ref("123", &ref));
}

static void test_cellref_no_row_digits(void **state) {
    (void)state;
    CellRef ref;
    assert_null(parse_cell_ref("ABC", &ref));
}

static void test_cellref_row_zero(void **state) {
    (void)state;
    CellRef ref;
    assert_null(parse_cell_ref("A0", &ref));
}

static void test_cellref_row_negative(void **state) {
    (void)state;
    CellRef ref;
    /* "-" is not a letter, so column part is empty → NULL */
    assert_null(parse_cell_ref("-1", &ref));
}

static void test_cellref_large_row(void **state) {
    (void)state;
    CellRef ref;
    const char *end = parse_cell_ref("A999", &ref);
    assert_non_null(end);
    assert_int_equal(ref.row_num, 999);
    free(ref.col_name);
}

/* ================================================================
 * parse_formula */

static void test_formula_add_numbers(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("3+5", &f), ERR_OK);
    assert_int_equal(f.op, ADD);
    assert_int_equal(f.arg1.kind, NUMBER);
    assert_int_equal(f.arg1.as.number, 3);
    assert_int_equal(f.arg2.kind, NUMBER);
    assert_int_equal(f.arg2.as.number, 5);
}

static void test_formula_sub(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("10-3", &f), ERR_OK);
    assert_int_equal(f.op, SUB);
    assert_int_equal(f.arg1.as.number, 10);
    assert_int_equal(f.arg2.as.number, 3);
}

static void test_formula_mul(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("2*4", &f), ERR_OK);
    assert_int_equal(f.op, MUL);
}

static void test_formula_div(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("10/2", &f), ERR_OK);
    assert_int_equal(f.op, DIV);
}

static void test_formula_ref_ref(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("A1+B2", &f), ERR_OK);
    assert_int_equal(f.arg1.kind, REF);
    assert_string_equal(f.arg1.as.ref.col_name, "A");
    assert_int_equal(f.arg1.as.ref.row_num, 1);
    assert_int_equal(f.arg2.kind, REF);
    assert_string_equal(f.arg2.as.ref.col_name, "B");
    assert_int_equal(f.arg2.as.ref.row_num, 2);
    arg_clear(&f.arg1);
    arg_clear(&f.arg2);
}

static void test_formula_num_ref(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("5+A1", &f), ERR_OK);
    assert_int_equal(f.arg1.kind, NUMBER);
    assert_int_equal(f.arg2.kind, REF);
    assert_string_equal(f.arg2.as.ref.col_name, "A");
    arg_clear(&f.arg2);
}

static void test_formula_negative_first_arg(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("-2+3", &f), ERR_OK);
    assert_int_equal(f.arg1.as.number, -2);
    assert_int_equal(f.arg2.as.number, 3);
}

static void test_formula_negative_second_arg(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("2+-3", &f), ERR_OK);
    assert_int_equal(f.op, ADD);
    assert_int_equal(f.arg1.as.number, 2);
    assert_int_equal(f.arg2.as.number, -3);
}

static void test_formula_missing_operator(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("42", &f), ERR_PARSE);
}

static void test_formula_invalid_operator(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("1%2", &f), ERR_PARSE);
}

static void test_formula_trailing_chars(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("1+2abc", &f), ERR_PARSE);
}

static void test_formula_bad_ref_zero_row(void **state) {
    (void)state;
    Formula f;
    /* A0 — row must be positive */
    assert_int_equal(parse_formula("A0+1", &f), ERR_PARSE);
}

static void test_formula_empty_string(void **state) {
    (void)state;
    Formula f;
    /* no arg1 at all */
    assert_int_equal(parse_formula("", &f), ERR_PARSE);
}

/* ================================================================
 * parse_cell_value */

static void test_cellval_positive_int(void **state) {
    (void)state;
    Cell c;
    assert_int_equal(parse_cell_value("42", &c), ERR_OK);
    assert_int_equal(c.kind, CELL_INT);
    assert_int_equal(c.as.value, 42);
}

static void test_cellval_negative_int(void **state) {
    (void)state;
    Cell c;
    assert_int_equal(parse_cell_value("-5", &c), ERR_OK);
    assert_int_equal(c.kind, CELL_INT);
    assert_int_equal(c.as.value, -5);
}

static void test_cellval_zero(void **state) {
    (void)state;
    Cell c;
    assert_int_equal(parse_cell_value("0", &c), ERR_OK);
    assert_int_equal(c.kind, CELL_INT);
    assert_int_equal(c.as.value, 0);
}

static void test_cellval_formula(void **state) {
    (void)state;
    Cell c = {0};
    assert_int_equal(parse_cell_value("=1+2", &c), ERR_OK);
    assert_int_equal(c.kind, CELL_FORMULA);
    cell_clear(&c);
}

static void test_cellval_formula_with_ref(void **state) {
    (void)state;
    Cell c = {0};
    assert_int_equal(parse_cell_value("=A1+B2", &c), ERR_OK);
    assert_int_equal(c.kind, CELL_FORMULA);
    cell_clear(&c);
}

static void test_cellval_invalid_text(void **state) {
    (void)state;
    Cell c;
    assert_int_equal(parse_cell_value("notanumber", &c), ERR_PARSE);
}

static void test_cellval_empty_formula(void **state) {
    (void)state;
    Cell c = {0};
    /* "=" with no operands — parse_formula("") returns ERR_PARSE */
    assert_int_equal(parse_cell_value("=", &c), ERR_PARSE);
}

static void test_cellval_formula_invalid_op(void **state) {
    (void)state;
    Cell c = {0};
    assert_int_equal(parse_cell_value("=1%2", &c), ERR_PARSE);
}

/* ================================================================ parse_csv
 * (integration) */

static void test_csv_basic_task_example(void **state) {
    (void)state;
    const char *csv =
        ",A,B,Cell\n"
        "1,1,0,1\n"
        "2,2,=A1+Cell30,0\n"
        "30,0,=B1+A1,5\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(t.n_columns, 3);
    assert_int_equal(t.n_rows, 3);
    table_destroy(&t);
}

static void test_csv_empty_cells(void **state) {
    (void)state;
    const char *csv = ",A,B\n1,,\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    CellRef ra = {"A", 1};
    CellRef rb = {"B", 1};
    assert_int_equal(table_get_cell(&t, ra)->kind, CELL_EMPTY);
    assert_int_equal(table_get_cell(&t, rb)->kind, CELL_EMPTY);
    table_destroy(&t);
}

static void test_csv_too_many_cells(void **state) {
    (void)state;
    const char *csv = ",A,B\n1,1,2,3\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_too_few_cells(void **state) {
    (void)state;
    const char *csv = ",A,B\n1,1\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_invalid_row_num_text(void **state) {
    (void)state;
    const char *csv = ",A,B\nabc,1,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_row_num_zero(void **state) {
    (void)state;
    const char *csv = ",A,B\n0,1,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_row_num_negative(void **state) {
    (void)state;
    const char *csv = ",A,B\n-1,1,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_bad_header_no_comma(void **state) {
    (void)state;
    const char *csv = "A,B\n1,1,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_duplicate_column_names(void **state) {
    (void)state;
    const char *csv = ",A,A\n1,1,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_invalid_cell_value(void **state) {
    (void)state;
    const char *csv = ",A,B\n1,hello,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_formula_ref_zero_row(void **state) {
    (void)state;
    const char *csv = ",A\n1,=A0+1\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_formula_invalid_operator(void **state) {
    (void)state;
    const char *csv = ",A\n1,=1%2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_duplicate_row_numbers(void **state) {
    (void)state;
    const char *csv = ",A\n1,5\n1,10\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
}

static void test_csv_crlf_line_endings(void **state) {
    (void)state;
    const char *csv = ",A,B\r\n1,3,7\r\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    CellRef ra = {"A", 1};
    CellRef rb = {"B", 1};
    assert_int_equal(table_get_cell(&t, ra)->as.value, 3);
    assert_int_equal(table_get_cell(&t, rb)->as.value, 7);
    table_destroy(&t);
}

static void test_csv_multiple_rows(void **state) {
    (void)state;
    const char *csv = ",A\n1,10\n2,20\n3,30\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(t.n_rows, 3);
    CellRef r3 = {"A", 3};
    assert_int_equal(table_get_cell(&t, r3)->as.value, 30);
    table_destroy(&t);
}

/* ================================================================ evaluate */

static void test_eval_add(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,3,=A1+A1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef r = {"B", 1};
    assert_int_equal(table_get_cell(&t, r)->as.value, 6);
    table_destroy(&t);
}

static void test_eval_sub(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,10,=A1-3\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef r = {"B", 1};
    assert_int_equal(table_get_cell(&t, r)->as.value, 7);
    table_destroy(&t);
}

static void test_eval_mul(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,4,=A1*3\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef r = {"B", 1};
    assert_int_equal(table_get_cell(&t, r)->as.value, 12);
    table_destroy(&t);
}

static void test_eval_div(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,10,=A1/2\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef r = {"B", 1};
    assert_int_equal(table_get_cell(&t, r)->as.value, 5);
    table_destroy(&t);
}

static void test_eval_div_by_zero_via_ref(void **state) {
    (void)state;
    /* A1=0, B1=5/A1 → division by zero through a cell reference */
    FILE *f = make_stream(",A,B\n1,0,=5/A1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_div_by_zero_literal(void **state) {
    (void)state;
    FILE *f = make_stream(",A\n1,=5/0\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_circular_ref(void **state) {
    (void)state;
    /* A1 = =B1+1, B1 = =A1+1 → cycle */
    FILE *f = make_stream(",A,B\n1,=B1+1,=A1+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_self_ref(void **state) {
    (void)state;
    FILE *f = make_stream(",A\n1,=A1+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_nonexistent_column(void **state) {
    (void)state;
    /* Z column doesn't exist */
    FILE *f = make_stream(",A\n1,=Z99+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_nonexistent_row(void **state) {
    (void)state;
    /* Column A exists but row 99 does not */
    FILE *f = make_stream(",A\n1,=A99+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_empty_cell_ref(void **state) {
    (void)state;
    /* A1 is empty, B1 references it */
    FILE *f = make_stream(",A,B\n1,,=A1+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_chained_formulas(void **state) {
    (void)state;
    /* A1=1, B1=A1+1=2, C1=B1*3=6 */
    FILE *f = make_stream(",A,B,C\n1,1,=A1+1,=B1*3\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef rc = {"C", 1};
    assert_int_equal(table_get_cell(&t, rc)->as.value, 6);
    table_destroy(&t);
}

static void test_eval_task_example(void **state) {
    (void)state;
    /* From the task description:
       ,A,B,Cell / 1,1,0,1 / 2,2,=A1+Cell30,0 / 30,0,=B1+A1,5
       Expected after eval:
         B2  = A1+Cell30 = 1+5 = 6
         B30 = B1+A1     = 0+1 = 1  */
    const char *csv =
        ",A,B,Cell\n"
        "1,1,0,1\n"
        "2,2,=A1+Cell30,0\n"
        "30,0,=B1+A1,5\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    CellRef b2 = {"B", 2};
    CellRef b30 = {"B", 30};
    assert_int_equal(table_get_cell(&t, b2)->as.value, 6);
    assert_int_equal(table_get_cell(&t, b30)->as.value, 1);
    table_destroy(&t);
}

/* ================================================================ evaluate:
 * overflow */

static void test_eval_overflow_add(void **state) {
    (void)state;
    FILE *f = make_stream(",A\n1,=9223372036854775807+1\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_overflow_sub(void **state) {
    (void)state;
    FILE *f = make_stream(",A\n1,=-9223372036854775807-2\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

static void test_eval_overflow_mul(void **state) {
    (void)state;
    FILE *f = make_stream(",A\n1,=9223372036854775807*2\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
}

/* ================================================================
 * parse_header: column name chars */

static void test_header_column_with_space(void **state) {
    (void)state;
    FILE *f = make_stream(",A B,C\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_PARSE);
    fclose(f);
}

static void test_header_column_with_digit(void **state) {
    (void)state;
    FILE *f = make_stream(",A1,B\n");
    Table t = {0};
    assert_int_equal(parse_header(f, &t), ERR_PARSE);
    fclose(f);
}

static void test_table_print_empty_cells(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,,2\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);

    char buf[256] = {0};
    FILE *out = fmemopen(buf, sizeof(buf), "w");
    table_print(&t, out);
    fclose(out);

    assert_string_equal(buf, ",A,B\n1,EMPTY,2\n");
    table_destroy(&t);
}

/* ================================================================
 * parse_formula: leading '+' */

static void test_formula_positive_prefix_rejected(void **state) {
    (void)state;
    /* strtol accepts "+1", but the task format is [-]digits or ref — not +digits
     */
    Formula f;
    assert_int_equal(parse_formula("+1+2", &f), ERR_PARSE);
}

static void test_formula_positive_prefix_second_arg_rejected(void **state) {
    (void)state;
    Formula f;
    assert_int_equal(parse_formula("1++2", &f), ERR_PARSE);
}

/* ================================================================ parse_csv:
 * bare CR line endings */

static void test_csv_cr_only_line_endings(void **state) {
    (void)state;
    /* Old Mac-style \r-only line endings */
    const char *csv = ",A,B\r1,3,7\r2,4,8\r";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(t.n_rows, 2);
    CellRef ra = {"A", 1};
    CellRef rb = {"B", 2};
    assert_int_equal(table_get_cell(&t, ra)->as.value, 3);
    assert_int_equal(table_get_cell(&t, rb)->as.value, 8);
    table_destroy(&t);
}

/* ================================================================ table_print:
 * row output order */

static void test_table_print_row_order_matches_input(void **state) {
    (void)state;
    /* Rows in non-sequential order: 30, 1, 2 — output must preserve that order */
    const char *csv = ",A\n30,3\n1,1\n2,2\n";
    FILE *f = make_stream(csv);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);

    char buf[128] = {0};
    FILE *out = fmemopen(buf, sizeof(buf), "w");
    table_print(&t, out);
    fclose(out);

    assert_string_equal(buf, ",A\n30,3\n1,1\n2,2\n");
    table_destroy(&t);
}

/* ================================================================ evaluate:
 * forward reference */

static void test_eval_forward_ref(void **state) {
    (void)state;
    /* Row 1 formula references row 2, which appears later in the file */
    FILE *f = make_stream(",A,B\n1,=A2+1,0\n2,5,0\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);
    CellRef r = {"A", 1};
    assert_int_equal(table_get_cell(&t, r)->as.value, 6);
    table_destroy(&t);
}

/* ================================================================ table_print
 */

static void test_table_print_format(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,1,2\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    char buf[256] = {0};
    FILE *out = fmemopen(buf, sizeof(buf), "w");
    table_print(&t, out);
    fclose(out);

    assert_string_equal(buf, ",A,B\n1,1,2\n");
    table_destroy(&t);
}

static void test_table_print_after_eval(void **state) {
    (void)state;
    FILE *f = make_stream(",A,B\n1,3,=A1+7\n");
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    char buf[256] = {0};
    FILE *out = fmemopen(buf, sizeof(buf), "w");
    table_print(&t, out);
    fclose(out);

    assert_string_equal(buf, ",A,B\n1,3,10\n");
    table_destroy(&t);
}

/* ================================================================ main */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* parse_header */
        cmocka_unit_test(test_header_valid_multiple_columns),
        cmocka_unit_test(test_header_single_column),
        cmocka_unit_test(test_header_no_newline_eof),
        cmocka_unit_test(test_header_no_leading_comma),
        cmocka_unit_test(test_header_empty_column_between),
        cmocka_unit_test(test_header_trailing_comma_empty_last),
        cmocka_unit_test(test_header_column_with_space),
        cmocka_unit_test(test_header_column_with_digit),

        /* parse_cell_ref */
        cmocka_unit_test(test_cellref_valid_single_char),
        cmocka_unit_test(test_cellref_valid_multichar_col),
        cmocka_unit_test(test_cellref_lowercase_letters),
        cmocka_unit_test(test_cellref_no_letters),
        cmocka_unit_test(test_cellref_no_row_digits),
        cmocka_unit_test(test_cellref_row_zero),
        cmocka_unit_test(test_cellref_row_negative),
        cmocka_unit_test(test_cellref_large_row),

        /* parse_formula */
        cmocka_unit_test(test_formula_add_numbers),
        cmocka_unit_test(test_formula_sub),
        cmocka_unit_test(test_formula_mul),
        cmocka_unit_test(test_formula_div),
        cmocka_unit_test(test_formula_ref_ref),
        cmocka_unit_test(test_formula_num_ref),
        cmocka_unit_test(test_formula_negative_first_arg),
        cmocka_unit_test(test_formula_negative_second_arg),
        cmocka_unit_test(test_formula_missing_operator),
        cmocka_unit_test(test_formula_invalid_operator),
        cmocka_unit_test(test_formula_trailing_chars),
        cmocka_unit_test(test_formula_bad_ref_zero_row),
        cmocka_unit_test(test_formula_empty_string),
        cmocka_unit_test(test_formula_positive_prefix_rejected),
        cmocka_unit_test(test_formula_positive_prefix_second_arg_rejected),

        /* parse_cell_value */
        cmocka_unit_test(test_cellval_positive_int),
        cmocka_unit_test(test_cellval_negative_int),
        cmocka_unit_test(test_cellval_zero),
        cmocka_unit_test(test_cellval_formula),
        cmocka_unit_test(test_cellval_formula_with_ref),
        cmocka_unit_test(test_cellval_invalid_text),
        cmocka_unit_test(test_cellval_empty_formula),
        cmocka_unit_test(test_cellval_formula_invalid_op),

        /* parse_csv */
        cmocka_unit_test(test_csv_basic_task_example),
        cmocka_unit_test(test_csv_empty_cells),
        cmocka_unit_test(test_csv_too_many_cells),
        cmocka_unit_test(test_csv_too_few_cells),
        cmocka_unit_test(test_csv_invalid_row_num_text),
        cmocka_unit_test(test_csv_row_num_zero),
        cmocka_unit_test(test_csv_row_num_negative),
        cmocka_unit_test(test_csv_bad_header_no_comma),
        cmocka_unit_test(test_csv_duplicate_column_names),
        cmocka_unit_test(test_csv_invalid_cell_value),
        cmocka_unit_test(test_csv_formula_ref_zero_row),
        cmocka_unit_test(test_csv_formula_invalid_operator),
        cmocka_unit_test(test_csv_duplicate_row_numbers),
        cmocka_unit_test(test_csv_crlf_line_endings),
        cmocka_unit_test(test_csv_cr_only_line_endings),
        cmocka_unit_test(test_csv_multiple_rows),

        /* evaluate */
        cmocka_unit_test(test_eval_add),
        cmocka_unit_test(test_eval_sub),
        cmocka_unit_test(test_eval_mul),
        cmocka_unit_test(test_eval_div),
        cmocka_unit_test(test_eval_div_by_zero_via_ref),
        cmocka_unit_test(test_eval_div_by_zero_literal),
        cmocka_unit_test(test_eval_circular_ref),
        cmocka_unit_test(test_eval_self_ref),
        cmocka_unit_test(test_eval_nonexistent_column),
        cmocka_unit_test(test_eval_nonexistent_row),
        cmocka_unit_test(test_eval_empty_cell_ref),
        cmocka_unit_test(test_eval_chained_formulas),
        cmocka_unit_test(test_eval_forward_ref),
        cmocka_unit_test(test_eval_task_example),
        cmocka_unit_test(test_eval_overflow_add),
        cmocka_unit_test(test_eval_overflow_sub),
        cmocka_unit_test(test_eval_overflow_mul),

        /* table_print */
        cmocka_unit_test(test_table_print_format),
        cmocka_unit_test(test_table_print_after_eval),
        cmocka_unit_test(test_table_print_empty_cells),
        cmocka_unit_test(test_table_print_row_order_matches_input),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
