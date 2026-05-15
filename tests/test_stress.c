/* Stress tests for csvreader.
 * Valid:   randomly generated correct tables — verify parse+eval+values.
 * Invalid: randomly generated broken tables — verify the right error is
 * returned.
 */
#include <cmocka.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>

#include "Cell.h"
#include "Parse.h"
#include "Table.h"
#include "errors.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ================================================================ StrBuf */

typedef struct {
  char *buf;
  size_t len;
  size_t cap;
} StrBuf;

static void sb_init(StrBuf *sb) {
  sb->cap = 4096;
  sb->len = 0;
  sb->buf = malloc(sb->cap);
  assert_non_null(sb->buf);
  sb->buf[0] = '\0';
}

static void sb_free(StrBuf *sb) {
  free(sb->buf);
  sb->buf = NULL;
  sb->len = sb->cap = 0;
}

static void sb_appendf(StrBuf *sb, const char *fmt, ...) {
  char tmp[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  size_t slen = strlen(tmp);
  while (sb->len + slen + 1 > sb->cap) {
    sb->cap *= 2;
    sb->buf = realloc(sb->buf, sb->cap);
    assert_non_null(sb->buf);
  }
  memcpy(sb->buf + sb->len, tmp, slen + 1);
  sb->len += slen;
}

/* ================================================================ utilities */

static FILE *open_stream(const char *s) {
  return fmemopen((void *)s, strlen(s), "r");
}

/* Returns a random int in [lo, hi] (inclusive). */
static int rng(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

/* Fill out[0..n-1] with n unique positive integers drawn from 1..200.*/
static void pick_rows(int *out, int n) {
  int pool[200];
  for (int i = 0; i < 200; i++)
    pool[i] = i + 1;
  for (int i = 0; i < n; i++) {
    int j = i + rand() % (200 - i);
    int t = pool[i];
    pool[i] = pool[j];
    pool[j] = t;
    out[i] = pool[i];
  }
}

/* Write ",A,B,...\n" header for n_cols columns.
 * Column names are single ASCII letters 'A'..'A'+n_cols-1 (max 8). */
static void write_header(StrBuf *sb, int n_cols) {
  for (int c = 0; c < n_cols; c++)
    sb_appendf(sb, ",%c", 'A' + c);
  sb_appendf(sb, "\n");
}

/* ================================================================
 * VALID STRESS TESTS
 * ================================================================ */

/*
 * All-integer tables (no formulas), random dimensions.
 * After parse+eval, every cell must contain its original value.
 */
static void test_stress_valid_all_integers(void **state) {
  (void)state;
  srand(42);

  for (int iter = 0; iter < 300; iter++) {
    int n_cols = rng(1, 8);
    int n_rows = rng(1, 15);
    int row_nums[15];
    pick_rows(row_nums, n_rows);

    int vals[15][8];
    for (int r = 0; r < n_rows; r++)
      for (int c = 0; c < n_cols; c++)
        vals[r][c] = rng(-9999, 9999);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);
    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++)
        sb_appendf(&sb, ",%d", vals[r][c]);
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        char col[2] = {(char)('A' + c), '\0'};
        CellRef ref;
        ref.col_name = col;
        ref.row_num = row_nums[r];
        Cell *cell = table_get_cell(&t, ref);
        assert_non_null(cell);
        assert_int_equal(cell->kind, CELL_INT);
        assert_int_equal(cell->as.value, (long)vals[r][c]);
      }
    }

    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Mix of plain integers and "=N op M" formulas where op in {+,-,*}.
 * Division is excluded to prevent accidental div-by-zero.
 * Computed values must match the arithmetic result.
 */
static void test_stress_valid_literal_formulas(void **state) {
  (void)state;
  srand(137);
  static const char OPS[] = "+-*";

  for (int iter = 0; iter < 200; iter++) {
    int n_cols = rng(1, 6);
    int n_rows = rng(1, 10);
    int row_nums[10];
    pick_rows(row_nums, n_rows);

    long expected[10][6];
    int is_formula[10][6];
    int fa[10][6], fb[10][6], fop[10][6];

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        is_formula[r][c] = (rand() % 3 == 0);
        if (is_formula[r][c]) {
          fa[r][c] = rng(-50, 50);
          fb[r][c] = rng(-50, 50);
          fop[r][c] = rand() % 3;
          switch (fop[r][c]) {
          case 0:
            expected[r][c] = fa[r][c] + fb[r][c];
            break;
          case 1:
            expected[r][c] = fa[r][c] - fb[r][c];
            break;
          default:
            expected[r][c] = (long)fa[r][c] * fb[r][c];
            break;
          }
        } else {
          expected[r][c] = rng(-9999, 9999);
        }
      }
    }

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);
    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (is_formula[r][c])
          sb_appendf(&sb, ",=%d%c%d", fa[r][c], OPS[fop[r][c]], fb[r][c]);
        else
          sb_appendf(&sb, ",%ld", expected[r][c]);
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        char col[2] = {(char)('A' + c), '\0'};
        CellRef ref;
        ref.col_name = col;
        ref.row_num = row_nums[r];
        Cell *cell = table_get_cell(&t, ref);
        assert_non_null(cell);
        assert_int_equal(cell->kind, CELL_INT);
        assert_int_equal(cell->as.value, expected[r][c]);
      }
    }

    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Formulas referencing existing cells without cycles.
 * Column 0 ('A') is always an integer.
 * Column c > 0 is either an integer or "=<prev_col><row>+<literal>".
 * References only go left (col c → col c-1), so no cycles are possible.
 */
static void test_stress_valid_cell_ref_formulas(void **state) {
  (void)state;
  srand(999);

  for (int iter = 0; iter < 150; iter++) {
    int n_cols = rng(2, 6);
    int n_rows = rng(1, 8);
    int row_nums[8];
    pick_rows(row_nums, n_rows);

    long expected[8][6];
    int use_ref[8][6];
    int lit[8][6];

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        if (c == 0) {
          use_ref[r][c] = 0;
          expected[r][c] = rng(-100, 100);
        } else {
          use_ref[r][c] = rand() % 2;
          if (use_ref[r][c]) {
            lit[r][c] = rng(-20, 20);
            expected[r][c] = expected[r][c - 1] + lit[r][c];
          } else {
            expected[r][c] = rng(-100, 100);
          }
        }
      }
    }

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);
    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (use_ref[r][c])
          /* "=<prev_col_letter><row_num>+<literal>" */
          sb_appendf(&sb, ",=%c%d+%d", 'A' + c - 1, row_nums[r], lit[r][c]);
        else
          sb_appendf(&sb, ",%ld", expected[r][c]);
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        char col[2] = {(char)('A' + c), '\0'};
        CellRef ref;
        ref.col_name = col;
        ref.row_num = row_nums[r];
        Cell *cell = table_get_cell(&t, ref);
        assert_non_null(cell);
        assert_int_equal(cell->kind, CELL_INT);
        assert_int_equal(cell->as.value, expected[r][c]);
      }
    }

    table_destroy(&t);
    sb_free(&sb);
  }
}

/* ================================================================
 * INVALID STRESS TESTS — PARSE ERRORS
 * ================================================================ */

/*
 * Bad row numbers: 0, negative integer, or alphabetic text.
 * All variants must produce ERR_PARSE.
 */
static void test_stress_invalid_row_number(void **state) {
  (void)state;
  srand(11);

  for (int iter = 0; iter < 150; iter++) {
    int n_cols = rng(1, 5);
    int n_valid = rng(0, 4);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_valid; r++) {
      sb_appendf(&sb, "%d", r + 1);
      for (int c = 0; c < n_cols; c++)
        sb_appendf(&sb, ",%d", rng(-100, 100));
      sb_appendf(&sb, "\n");
    }

    /* bad row number */
    switch (rand() % 3) {
    case 0:
      sb_appendf(&sb, "0");
      break; /* zero */
    case 1:
      sb_appendf(&sb, "-%d", rng(1, 99));
      break; /* negative */
    default:
      sb_appendf(&sb, "abc");
      break; /* text */
    }
    for (int c = 0; c < n_cols; c++)
      sb_appendf(&sb, ",%d", rng(-100, 100));
    sb_appendf(&sb, "\n");

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Rows with wrong number of cells (too few or too many).
 */
static void test_stress_invalid_cell_count(void **state) {
  (void)state;
  srand(22);

  for (int iter = 0; iter < 150; iter++) {
    int n_cols = rng(2, 6);
    int n_valid = rng(0, 3);
    int bad_n;
    do {
      bad_n = rng(0, n_cols + 2);
    } while (bad_n == n_cols);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_valid; r++) {
      sb_appendf(&sb, "%d", r + 1);
      for (int c = 0; c < n_cols; c++)
        sb_appendf(&sb, ",%d", rng(-100, 100));
      sb_appendf(&sb, "\n");
    }

    sb_appendf(&sb, "%d", n_valid + 1);
    for (int c = 0; c < bad_n; c++)
      sb_appendf(&sb, ",%d", rng(-100, 100));
    sb_appendf(&sb, "\n");

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Non-numeric, non-formula text in a data cell.
 */
static void test_stress_invalid_cell_value(void **state) {
  (void)state;
  srand(33);
  static const char *GARBAGE[] = {"hello", "abc", "foo",  "!!!", "1.5",
                                  "1e3",   "nan", "true", "#",   "??"};
  int n_garbage = (int)(sizeof(GARBAGE) / sizeof(GARBAGE[0]));

  for (int iter = 0; iter < 150; iter++) {
    int n_cols = rng(1, 5);
    int n_valid = rng(0, 3);
    int bad_col = rng(0, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_valid; r++) {
      sb_appendf(&sb, "%d", r + 1);
      for (int c = 0; c < n_cols; c++)
        sb_appendf(&sb, ",%d", rng(-100, 100));
      sb_appendf(&sb, "\n");
    }

    sb_appendf(&sb, "%d", n_valid + 1);
    for (int c = 0; c < n_cols; c++) {
      if (c == bad_col)
        sb_appendf(&sb, ",%s", GARBAGE[rand() % n_garbage]);
      else
        sb_appendf(&sb, ",%d", rng(-100, 100));
    }
    sb_appendf(&sb, "\n");

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Malformed headers, three variants:
 *   (0) no leading comma:  "A,B,...\n"
 *   (1) empty column name: ",A,,C,...\n"  (hole at a random position)
 *   (2) duplicate column:  ",A,B,...,A\n" (last col repeats an earlier one)
 */
static void test_stress_invalid_header(void **state) {
  (void)state;
  srand(44);

  for (int iter = 0; iter < 150; iter++) {
    int n_cols = rng(2, 6);
    StrBuf sb;
    sb_init(&sb);

    switch (rand() % 3) {
    case 0:
      /* no leading comma */
      for (int c = 0; c < n_cols; c++) {
        if (c > 0)
          sb_appendf(&sb, ",");
        sb_appendf(&sb, "%c", 'A' + c);
      }
      sb_appendf(&sb, "\n");
      break;

    case 1: {
      /* empty column name — punch a hole */
      int hole = rng(0, n_cols - 1);
      for (int c = 0; c < n_cols; c++) {
        sb_appendf(&sb, ",");
        if (c != hole)
          sb_appendf(&sb, "%c", 'A' + c);
      }
      sb_appendf(&sb, "\n");
      break;
    }

    default: {
      /* duplicate column name — last column repeats column at index dup */
      int dup = rng(0, n_cols - 2);
      for (int c = 0; c < n_cols - 1; c++)
        sb_appendf(&sb, ",%c", 'A' + c);
      sb_appendf(&sb, ",%c\n", 'A' + dup); /* duplicate */
      break;
    }
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Syntactically broken formula strings in a cell.
 * Variants: invalid operator, trailing junk, missing operand, zero row in ref,
 * empty.
 */
static void test_stress_invalid_formula_syntax(void **state) {
  (void)state;
  srand(55);
  static const char *BAD[] = {
      "=1%2",    /* unsupported operator */
      "=1^2",    /* unsupported operator */
      "=1&2",    /* unsupported operator */
      "=1+2abc", /* trailing garbage */
      "=1+",     /* missing second operand */
      "=A0+1",   /* row number 0 in cell ref */
      "=",       /* empty formula */
  };
  int n_bad = (int)(sizeof(BAD) / sizeof(BAD[0]));

  for (int iter = 0; iter < 100; iter++) {
    int n_cols = rng(1, 4);
    int n_valid = rng(0, 3);
    int bad_col = rng(0, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_valid; r++) {
      sb_appendf(&sb, "%d", r + 1);
      for (int c = 0; c < n_cols; c++)
        sb_appendf(&sb, ",%d", rng(-100, 100));
      sb_appendf(&sb, "\n");
    }

    sb_appendf(&sb, "%d", n_valid + 1);
    for (int c = 0; c < n_cols; c++) {
      if (c == bad_col)
        sb_appendf(&sb, ",%s", BAD[rand() % n_bad]);
      else
        sb_appendf(&sb, ",%d", rng(-100, 100));
    }
    sb_appendf(&sb, "\n");

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_PARSE);
    fclose(f);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/* ================================================================
 * INVALID STRESS TESTS — EVAL ERRORS
 * ================================================================ */

/*
 * Division by zero: formula divides by literal 0 or by a cell-ref/0.
 * Parse must succeed; evaluate_all must return ERR_EVAL.
 */
static void test_stress_invalid_div_zero(void **state) {
  (void)state;
  srand(66);

  for (int iter = 0; iter < 100; iter++) {
    int n_cols = rng(1, 5);
    int n_rows = rng(1, 6);
    int row_nums[6];
    pick_rows(row_nums, n_rows);

    int bad_r = rng(0, n_rows - 1);
    int bad_c = rng(0, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (r == bad_r && c == bad_c) {
          /* Variant A: literal "=5/0"
           * Variant B: ref   "=<col><row>/0"  (divisor is always literal 0) */
          if (rand() % 2 == 0)
            sb_appendf(&sb, ",=5/0");
          else
            sb_appendf(&sb, ",=%c%d/0", 'A' + bad_c, row_nums[r]);
        } else {
          sb_appendf(&sb, ",%d", rng(1, 100));
        }
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * References to cells that do not exist in the table.
 * Two sub-variants: nonexistent column 'Z', or existing column but row 999.
 * Parse must succeed; evaluate_all must return ERR_EVAL.
 */
static void test_stress_invalid_nonexistent_ref(void **state) {
  (void)state;
  srand(77);

  for (int iter = 0; iter < 100; iter++) {
    int n_cols = rng(1, 4);
    int n_rows = rng(1, 5);
    int row_nums[5];
    pick_rows(row_nums, n_rows);

    /* row_nums are drawn from 1..200; row 999 is guaranteed absent */
    int bad_r = rng(0, n_rows - 1);
    int bad_c = rng(0, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (r == bad_r && c == bad_c) {
          if (rand() % 2 == 0)
            sb_appendf(&sb, ",=Z%d+1",
                       row_nums[r]); /* column Z doesn't exist */
          else
            sb_appendf(&sb, ",=%c999+1",
                       'A' + bad_c); /* row 999 doesn't exist */
        } else {
          sb_appendf(&sb, ",%d", rng(1, 100));
        }
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Self-referential formulas: "=<this_col><this_row>+1".
 * The evaluator must detect the cycle and return ERR_EVAL.
 */
static void test_stress_invalid_self_ref(void **state) {
  (void)state;
  srand(88);

  for (int iter = 0; iter < 100; iter++) {
    int n_cols = rng(1, 5);
    int n_rows = rng(1, 5);
    int row_nums[5];
    pick_rows(row_nums, n_rows);

    int bad_r = rng(0, n_rows - 1);
    int bad_c = rng(0, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (r == bad_r && c == bad_c)
          sb_appendf(&sb, ",=%c%d+1", 'A' + bad_c,
                     row_nums[bad_r]); /* self-reference */
        else
          sb_appendf(&sb, ",%d", rng(1, 100));
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Mutual circular reference: cell X = "=Y+1", cell Y = "=X+1".
 * Both X and Y are in the same row; X is to the left of Y.
 * The evaluator must detect the cycle and return ERR_EVAL.
 */
static void test_stress_invalid_mutual_circular(void **state) {
  (void)state;
  srand(101);

  for (int iter = 0; iter < 100; iter++) {
    int n_cols = rng(2, 6);
    int n_rows = rng(1, 5);
    int row_nums[5];
    pick_rows(row_nums, n_rows);

    int cycle_r = rng(0, n_rows - 1);
    int cycle_c1 = rng(0, n_cols - 2);            /* left cell  */
    int cycle_c2 = rng(cycle_c1 + 1, n_cols - 1); /* right cell */

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (r == cycle_r && c == cycle_c1)
          /* left cell references right cell */
          sb_appendf(&sb, ",=%c%d+1", 'A' + cycle_c2, row_nums[cycle_r]);
        else if (r == cycle_r && c == cycle_c2)
          /* right cell references left cell */
          sb_appendf(&sb, ",=%c%d+1", 'A' + cycle_c1, row_nums[cycle_r]);
        else
          sb_appendf(&sb, ",%d", rng(1, 100));
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Formula references an empty cell (cell exists but was left blank in CSV).
 * An empty cell has no integer value, so the evaluator must return ERR_EVAL.
 */
static void test_stress_invalid_empty_cell_ref(void **state) {
  (void)state;
  srand(202);

  for (int iter = 0; iter < 100; iter++) {
    /* Need at least 2 columns so one can be empty, another can reference it */
    int n_cols = rng(2, 5);
    int n_rows = rng(1, 5);
    int row_nums[5];
    pick_rows(row_nums, n_rows);

    int ref_r = rng(0, n_rows - 1); /* row with the problematic pair */
    int empty_c = rng(0, n_cols - 2);
    int formula_c = rng(empty_c + 1, n_cols - 1);

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);

    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (r == ref_r && c == empty_c)
          sb_appendf(&sb, ","); /* empty cell */
        else if (r == ref_r && c == formula_c)
          sb_appendf(&sb, ",=%c%d+1", 'A' + empty_c,
                     row_nums[ref_r]); /* references empty cell */
        else
          sb_appendf(&sb, ",%d", rng(1, 100));
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_EVAL);
    table_destroy(&t);
    sb_free(&sb);
  }
}

/*
 * Non-deterministic variant: random seed from the current time.
 * Runs a mix of all-integer tables and literal-operand formulas.
 * Verifies that parse+eval succeeds and every cell holds the expected value.
 * Failures here implicate timing-dependent edge cases missed by fixed seeds.
 */
static void test_stress_random_seed(void **state) {
  (void)state;
  srand((unsigned)time(NULL));
  static const char OPS[] = "+-*";

  for (int iter = 0; iter < 200; iter++) {
    int n_cols = rng(1, 6);
    int n_rows = rng(1, 10);
    int row_nums[10];
    pick_rows(row_nums, n_rows);

    long expected[10][6];
    int is_formula[10][6];
    int fa[10][6], fb[10][6], fop[10][6];

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        is_formula[r][c] = (rand() % 3 == 0);
        if (is_formula[r][c]) {
          fa[r][c] = rng(-50, 50);
          fb[r][c] = rng(-50, 50);
          fop[r][c] = rand() % 3;
          switch (fop[r][c]) {
          case 0:
            expected[r][c] = fa[r][c] + fb[r][c];
            break;
          case 1:
            expected[r][c] = fa[r][c] - fb[r][c];
            break;
          default:
            expected[r][c] = (long)fa[r][c] * fb[r][c];
            break;
          }
        } else {
          expected[r][c] = rng(-9999, 9999);
        }
      }
    }

    StrBuf sb;
    sb_init(&sb);
    write_header(&sb, n_cols);
    for (int r = 0; r < n_rows; r++) {
      sb_appendf(&sb, "%d", row_nums[r]);
      for (int c = 0; c < n_cols; c++) {
        if (is_formula[r][c])
          sb_appendf(&sb, ",=%d%c%d", fa[r][c], OPS[fop[r][c]], fb[r][c]);
        else
          sb_appendf(&sb, ",%ld", expected[r][c]);
      }
      sb_appendf(&sb, "\n");
    }

    FILE *f = open_stream(sb.buf);
    Table t = {0};
    assert_int_equal(parse_csv(f, &t), ERR_OK);
    fclose(f);
    assert_int_equal(evaluate_all(&t), ERR_OK);

    for (int r = 0; r < n_rows; r++) {
      for (int c = 0; c < n_cols; c++) {
        char col[2] = {(char)('A' + c), '\0'};
        CellRef ref;
        ref.col_name = col;
        ref.row_num = row_nums[r];
        Cell *cell = table_get_cell(&t, ref);
        assert_non_null(cell);
        assert_int_equal(cell->kind, CELL_INT);
        assert_int_equal(cell->as.value, expected[r][c]);
      }
    }

    table_destroy(&t);
    sb_free(&sb);
  }
}

/* ================================================================ main */

int main(void) {
  const struct CMUnitTest tests[] = {
      /* valid */
      cmocka_unit_test(test_stress_valid_all_integers),
      cmocka_unit_test(test_stress_valid_literal_formulas),
      cmocka_unit_test(test_stress_valid_cell_ref_formulas),
      cmocka_unit_test(test_stress_random_seed),
      /* invalid: parse errors */
      cmocka_unit_test(test_stress_invalid_row_number),
      cmocka_unit_test(test_stress_invalid_cell_count),
      cmocka_unit_test(test_stress_invalid_cell_value),
      cmocka_unit_test(test_stress_invalid_header),
      cmocka_unit_test(test_stress_invalid_formula_syntax),
      /* invalid: eval errors */
      cmocka_unit_test(test_stress_invalid_div_zero),
      cmocka_unit_test(test_stress_invalid_nonexistent_ref),
      cmocka_unit_test(test_stress_invalid_self_ref),
      cmocka_unit_test(test_stress_invalid_mutual_circular),
      cmocka_unit_test(test_stress_invalid_empty_cell_ref),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
