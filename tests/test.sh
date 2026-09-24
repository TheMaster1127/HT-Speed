#!/bin/bash
# test.sh — compile, run, and verify every .hts test.
# Usage: bash tests/test.sh
#
# For each tests/NAME.hts:
#   1. compile with htspeed_cib -> tests/NAME.bin
#   2. run -> tests/NAME.actual
#   3. diff against tests/NAME.expected
#   4. delete the binary and the .actual file

set -u

TESTS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$TESTS_DIR/.." && pwd)"
COMPILER="$PROJECT_ROOT/htspeed_cib"

# ─── Colors ───────────────────────────────────────────────
if [ -t 1 ]; then
    G="\033[32m"; R="\033[31m"; Y="\033[33m"; B="\033[1m"; N="\033[0m"
else
    G=""; R=""; Y=""; B=""; N=""
fi

# ─── Preconditions ────────────────────────────────────────
if [ ! -x "$COMPILER" ]; then
    echo -e "${R}Error:${N} compiler not found at $COMPILER"
    echo "       build it first:  cib htspeed_cib.c -Z4"
    exit 1
fi

# ─── Counters ─────────────────────────────────────────────
PASS=0
FAIL=0
SKIP=0
FAILED=()

echo -e "${B}HT-Speed test suite${N}"
echo "  compiler: $COMPILER"
echo "  tests:    $TESTS_DIR"
echo ""

# ─── Run each test ────────────────────────────────────────
shopt -s nullglob
for hts in "$TESTS_DIR"/*.hts; do
    name=$(basename "$hts" .hts)
    expected="$TESTS_DIR/$name.expected"
    binary="$TESTS_DIR/$name.bin"
    actual="$TESTS_DIR/$name.actual"

    if [ ! -f "$expected" ]; then
        echo -e "  ${Y}[SKIP]${N} $name  (no .expected file)"
        SKIP=$((SKIP + 1))
        continue
    fi

    # Compile
    compile_log=$("$COMPILER" "$hts" "$binary" 2>&1)
    compile_status=$?
    if [ $compile_status -ne 0 ] || [ ! -f "$binary" ]; then
        echo -e "  ${R}[FAIL]${N} $name  (compilation failed)"
        echo "         $compile_log" | head -5
        FAIL=$((FAIL + 1))
        FAILED+=("$name (compile)")
        rm -f "$binary" "$actual"
        continue
    fi

    # Run
    "$binary" > "$actual" 2>&1
    run_status=$?

    # Expected exit code (default 0)
    expected_exit_file="$TESTS_DIR/$name.exitcode"
    expected_exit=0
    if [ -f "$expected_exit_file" ]; then
        expected_exit=$(cat "$expected_exit_file" | tr -d '[:space:]')
    fi

    if [ "$run_status" -ne "$expected_exit" ]; then
        echo -e "  ${R}[FAIL]${N} $name  (exit code: expected $expected_exit, got $run_status)"
        FAIL=$((FAIL + 1))
        FAILED+=("$name (exit code)")
        rm -f "$binary" "$actual"
        continue
    fi

    # Compare
    if diff -q "$expected" "$actual" > /dev/null 2>&1; then
        bytes=$(wc -c < "$binary")
        echo -e "  ${G}[PASS]${N} $name  (${bytes}B)"
        PASS=$((PASS + 1))
    else
        echo -e "  ${R}[FAIL]${N} $name  (output mismatch)"
        echo "         ── expected ──"
        sed 's/^/         /' "$expected"
        echo "         ── actual ──"
        sed 's/^/         /' "$actual"
        echo "         ─────────────"
        FAIL=$((FAIL + 1))
        FAILED+=("$name (diff)")
    fi

    # Cleanup
    rm -f "$binary" "$actual"
done

# ─── Summary ──────────────────────────────────────────────
echo ""
echo -e "${B}────────────────────────────────────────${N}"
total=$((PASS + FAIL + SKIP))
if [ $FAIL -eq 0 ]; then
    echo -e "  ${G}${B}ALL PASSED${N}   $PASS/$total"
else
    echo -e "  ${R}${B}FAILURES${N}   ${PASS} passed, ${R}${FAIL} failed${N}, ${SKIP} skipped"
    echo ""
    echo "  failed:"
    for t in "${FAILED[@]}"; do
        echo "    - $t"
    done
fi
echo -e "${B}────────────────────────────────────────${N}"

[ $FAIL -eq 0 ]
