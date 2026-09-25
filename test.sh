#!/bin/bash
# test.sh — HT-Speed test runner (project root)
#
# Usage:
#   ./test.sh           run all tests (summary)
#   ./test.sh 15        debug test 15 only (verbose, hexdumps)

set -u

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
TESTS_DIR="$PROJECT_ROOT/tests"
COMPILER="$PROJECT_ROOT/htspeed_cib"

if [ -t 1 ]; then
    G="\033[32m"; R="\033[31m"; Y="\033[33m"; B="\033[1m"; D="\033[2m"; N="\033[0m"
else
    G=""; R=""; Y=""; B=""; D=""; N=""
fi

if [ ! -x "$COMPILER" ]; then
    echo -e "${R}Error:${N} compiler not found at $COMPILER"
    echo "       build it first:  cib htspeed_cib.c -Z4"
    exit 1
fi

if [ ! -f "$TESTS_DIR/setup.sh" ]; then
    echo -e "${R}Error:${N} $TESTS_DIR/setup.sh not found"
    exit 1
fi

# Always regenerate test files
bash "$TESTS_DIR/setup.sh" > /dev/null

run_single() {
    local num="$1"
    local hts
    hts=$(ls "$TESTS_DIR"/"$num"_*.hts 2>/dev/null | head -1)
    if [ -z "$hts" ]; then
        echo -e "${R}No test matching number $num${N}"
        exit 1
    fi
    local name expected exitcode_file args_file binary actual
    name=$(basename "$hts" .hts)
    expected="$TESTS_DIR/$name.expected"
    exitcode_file="$TESTS_DIR/$name.exitcode"
    args_file="$TESTS_DIR/$name.args"
    binary="$TESTS_DIR/$name.bin"
    actual="$TESTS_DIR/$name.actual"

    echo -e "${B}═══════════════════════════════════════════════════════════${N}"
    echo -e "${B}  DEBUG MODE: $name${N}"
    echo -e "${B}═══════════════════════════════════════════════════════════${N}"
    echo ""

    echo -e "${B}── Source ──────────────────────────────────────────────────${N}"
    cat "$hts"
    echo ""

    echo -e "${B}── Compile ─────────────────────────────────────────────────${N}"
    local compile_log compile_status
    compile_log=$("$COMPILER" "$hts" "$binary" 2>&1)
    compile_status=$?
    echo "$compile_log"
    echo "  exit: $compile_status"
    if [ $compile_status -ne 0 ] || [ ! -f "$binary" ]; then
        echo -e "  ${R}COMPILATION FAILED${N}"
        rm -f "$binary" "$actual"
        exit 1
    fi
    echo ""

    echo -e "${B}── Run ─────────────────────────────────────────────────────${N}"
    local args=()
    if [ -f "$args_file" ]; then
        mapfile -t args < "$args_file"
    fi

    "$binary" "${args[@]}" > "$actual" 2>&1
    local run_status=$?

    local expected_exit=0
    if [ -f "$exitcode_file" ]; then
        expected_exit=$(tr -d '[:space:]' < "$exitcode_file")
    fi

    echo "  exit: got $run_status, expected $expected_exit"
    echo ""

    echo -e "${B}── Actual output ───────────────────────────────────────────${N}"
    cat "$actual"
    echo ""
    echo -e "${D}  xxd:${N}"
    xxd "$actual"
    echo ""

    echo -e "${B}── Expected output ─────────────────────────────────────────${N}"
    cat "$expected"
    echo ""
    echo -e "${D}  xxd:${N}"
    xxd "$expected"
    echo ""

    echo -e "${B}── Diff ────────────────────────────────────────────────────${N}"
    if diff -u "$expected" "$actual"; then
        echo "  (no diff)"
    fi
    echo ""

    echo -e "${B}── Verdict ─────────────────────────────────────────────────${N}"
    local ok=1
    if [ $run_status -ne "$expected_exit" ]; then
        echo -e "  ${R}FAIL${N}: exit code mismatch"
        ok=0
    fi
    if ! diff -q "$expected" "$actual" > /dev/null 2>&1; then
        echo -e "  ${R}FAIL${N}: output mismatch"
        ok=0
    fi
    if [ $ok -eq 1 ]; then
        echo -e "  ${G}PASS${N}"
    fi

    rm -f "$binary" "$actual"
    echo ""
    [ $ok -eq 1 ]
}

run_all() {
    local PASS=0 FAIL=0 SKIP=0
    local FAILED=()

    echo -e "${B}HT-Speed test suite${N}"
    echo "  compiler: $COMPILER"
    echo "  tests:    $TESTS_DIR"
    echo ""

    shopt -s nullglob
    local hts
    # Numerical sort for clean output
    for hts in $(ls -v "$TESTS_DIR"/*.hts); do
        local name expected binary actual exitcode_file args_file
        name=$(basename "$hts" .hts)
        expected="$TESTS_DIR/$name.expected"
        exitcode_file="$TESTS_DIR/$name.exitcode"
        args_file="$TESTS_DIR/$name.args"
        binary="$TESTS_DIR/$name.bin"
        actual="$TESTS_DIR/$name.actual"

        if [ ! -f "$expected" ]; then
            echo -e "  ${Y}[SKIP]${N} $name  (no .expected)"
            SKIP=$((SKIP + 1))
            continue
        fi

        local compile_log
        compile_log=$("$COMPILER" "$hts" "$binary" 2>&1)
        if [ $? -ne 0 ] || [ ! -f "$binary" ]; then
            echo -e "  ${R}[FAIL]${N} $name  (compile)"
            FAIL=$((FAIL + 1))
            FAILED+=("$name (compile)")
            rm -f "$binary" "$actual"
            continue
        fi

        local args=()
        if [ -f "$args_file" ]; then
            mapfile -t args < "$args_file"
        fi

        "$binary" "${args[@]}" > "$actual" 2>&1
        local run_status=$?

        local expected_exit=0
        if [ -f "$exitcode_file" ]; then
            expected_exit=$(tr -d '[:space:]' < "$exitcode_file")
        fi

        if [ "$run_status" -ne "$expected_exit" ]; then
            echo -e "  ${R}[FAIL]${N} $name  (exit: got $run_status, want $expected_exit)"
            FAIL=$((FAIL + 1))
            FAILED+=("$name (exit)")
            rm -f "$binary" "$actual"
            continue
        fi

        if diff -q "$expected" "$actual" > /dev/null 2>&1; then
            local bytes
            bytes=$(wc -c < "$binary")
            echo -e "  ${G}[PASS]${N} $name  (${bytes}B)"
            PASS=$((PASS + 1))
        else
            echo -e "  ${R}[FAIL]${N} $name  (output mismatch)"
            FAIL=$((FAIL + 1))
            FAILED+=("$name (diff)")
        fi

        rm -f "$binary" "$actual"
    done

    echo ""
    echo -e "${B}────────────────────────────────────────────────────────────${N}"
    local total=$((PASS + FAIL + SKIP))
    if [ $FAIL -eq 0 ]; then
        echo -e "  ${G}${B}ALL PASSED${N}   $PASS/$total"
    else
        echo -e "  ${R}${B}FAILURES${N}   ${G}$PASS passed${N}, ${R}$FAIL failed${N}, $SKIP skipped"
        echo ""
        echo "  failed:"
        local t
        for t in "${FAILED[@]}"; do
            echo "    - $t"
        done
    fi
    echo -e "${B}────────────────────────────────────────────────────────────${N}"
    [ $FAIL -eq 0 ]
}

if [ $# -ge 1 ] && [[ "$1" =~ ^[0-9]+$ ]]; then
    run_single "$1"
else
    run_all
fi
