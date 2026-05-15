#!/usr/bin/env bash
# Black-box tests for the csvreader binary.
# Usage: test_binary.sh <path-to-csvreader>

set -euo pipefail

CSVREADER="${1:?Usage: test_binary.sh <path-to-csvreader>}"

PASS=0
FAIL=0

ERR_PARSE=3
ERR_EVAL=4

# check_csv DESC EXPECTED_EXIT EXPECTED_OUT CSV
# For EXPECTED_EXIT != 0: also verifies stderr is non-empty.
# For EXPECTED_EXIT == 0: also verifies stderr is empty.
check_csv() {
    local desc="$1" expected_exit="$2" expected_out="$3" csv="$4"
    local tmp_csv tmp_err actual_out actual_err actual_exit
    tmp_csv=$(mktemp)
    tmp_err=$(mktemp)
    printf '%s' "$csv" > "$tmp_csv"
    actual_out=$("$CSVREADER" "$tmp_csv" 2>"$tmp_err") && actual_exit=0 || actual_exit=$?
    actual_err=$(cat "$tmp_err")
    rm -f "$tmp_csv" "$tmp_err"

    local ok=1
    [ "$actual_exit" -ne "$expected_exit" ]                    && ok=0
    [ "$actual_out"  != "$expected_out"   ]                    && ok=0
    [ "$expected_exit" -ne 0 ] && [ -z "$actual_err" ]        && ok=0
    [ "$expected_exit" -eq 0 ] && [ -n "$actual_err" ]        && ok=0

    if [ "$ok" -eq 1 ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $desc"
        echo "  expected exit=$expected_exit, got=$actual_exit"
        if [ "$actual_out" != "$expected_out" ]; then
            printf '  expected output: %s\n' "$expected_out"
            printf '  actual   output: %s\n' "$actual_out"
        fi
        [ "$expected_exit" -ne 0 ] && [ -z "$actual_err" ] && \
            echo "  expected non-empty stderr, got empty"
        [ "$expected_exit" -eq 0 ] && [ -n "$actual_err" ] && \
            printf '  unexpected stderr: %s\n' "$actual_err"
    fi
}

# check_cmd DESC EXPECTED_EXIT CMD...
# Runs CMD, verifies exit code exactly and that stderr is non-empty.
check_cmd() {
    local desc="$1" expected_exit="$2"
    shift 2
    local tmp_err actual_err actual_exit
    tmp_err=$(mktemp)
    "$@" 2>"$tmp_err" && actual_exit=0 || actual_exit=$?
    actual_err=$(cat "$tmp_err")
    rm -f "$tmp_err"

    if [ "$actual_exit" -eq "$expected_exit" ] && [ -n "$actual_err" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "FAIL: $desc"
        echo "  expected exit=$expected_exit, got=$actual_exit"
        [ -z "$actual_err" ] && echo "  expected non-empty stderr, got empty"
    fi
}

# ---------------------------------------------------------------------------
# 1. No arguments → ERR_PARSE + usage message on stderr
# ---------------------------------------------------------------------------
check_cmd "no-args" $ERR_PARSE "$CSVREADER"

# ---------------------------------------------------------------------------
# 2. Non-existent file → ERR_PARSE + error message on stderr
# ---------------------------------------------------------------------------
check_cmd "missing file" $ERR_PARSE \
    "$CSVREADER" /tmp/this_file_does_not_exist_csvreader_test

# ---------------------------------------------------------------------------
# 3. Task example from the specification
# ---------------------------------------------------------------------------
check_csv "task example" 0 \
",A,B,C
1,1,0,1
2,2,1,2" \
",A,B,C
1,1,0,1
2,2,=A1+B1,=B2+C1
"

# ---------------------------------------------------------------------------
# 4. Header-only table (no data rows) → print header, exit 0
# ---------------------------------------------------------------------------
check_csv "header only" 0 \
",A,B" \
",A,B
"

# ---------------------------------------------------------------------------
# 5. Division by zero → ERR_EVAL + error on stderr
# ---------------------------------------------------------------------------
check_csv "division by zero" $ERR_EVAL "" \
",A
1,=1/0
"

# ---------------------------------------------------------------------------
# 6. Parse error (bad header) → ERR_PARSE + error on stderr
# ---------------------------------------------------------------------------
check_csv "bad header" $ERR_PARSE "" \
"A,B,C
1,2,3
"

# ---------------------------------------------------------------------------
# 7. CRLF line endings — same result as LF
# ---------------------------------------------------------------------------
check_csv "CRLF line endings" 0 \
",A,B
1,3,4" \
$',A,B\r\n1,3,4\r\n'

# ---------------------------------------------------------------------------
# 8. No trailing newline on last line — still parses correctly
# ---------------------------------------------------------------------------
check_csv "no trailing newline" 0 \
",A
1,42" \
",A
1,42"

# ---------------------------------------------------------------------------
# 9. Single cell, negative integer
# ---------------------------------------------------------------------------
check_csv "single negative cell" 0 \
",X
5,-999" \
",X
5,-999
"

# ---------------------------------------------------------------------------
# 10. Circular reference → ERR_EVAL + error on stderr
# ---------------------------------------------------------------------------
check_csv "circular reference" $ERR_EVAL "" \
",A,B
1,=B1+1,=A1+1
"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo "binary_tests: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
