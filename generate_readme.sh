#!/bin/bash
# generate_readme.sh — Automated README generator for HT-Speed
# Profiles physical silicon live across Micro and Mega benchmarks

set -e

PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_ROOT"

if [ ! -x "./htspeed_cib" ]; then
    echo "[-] Compiler not found. Building with cib..."
    cib htspeed_cib.c -Z4
fi

if ! command -v tcc >/dev/null 2>&1; then
    echo "[-] Error: tcc not installed."
    exit 1
fi

fmt_num() {
    printf "%'d" "$1" 2>/dev/null || echo "$1"
}

# ============================================================
# 1. MICRO BENCHMARK (test_speed/test.hts vs test_speed/test.c)
# ============================================================
echo "[*] Profiling Micro-Benchmark (test.hts, 100 runs)..."
perf stat -r 100 ./htspeed_cib test_speed/test.hts /tmp/ht_micro_bin >/dev/null 2> /tmp/perf_ht_micro.txt
perf stat -r 100 tcc test_speed/test.c -o /tmp/tcc_micro_bin >/dev/null 2> /tmp/perf_tcc_micro.txt

HT_U_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_micro.txt)
TCC_U_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_micro.txt)
HT_U_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_micro.txt)
TCC_U_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_micro.txt)
HT_U_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_micro.txt)
TCC_U_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_micro.txt)
HT_U_TASK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_ht_micro.txt)
TCC_U_TASK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_tcc_micro.txt)
HT_U_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_ht_micro.txt)
TCC_U_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_tcc_micro.txt)
HT_U_SIZE=$(wc -c < /tmp/ht_micro_bin | tr -d ' ')
TCC_U_SIZE=$(wc -c < /tmp/tcc_micro_bin | tr -d ' ')

CYCLES_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_CYCLES / $HT_U_CYCLES}")
FAULTS_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_FAULTS / $HT_U_FAULTS}")
TASK_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_TASK / $HT_U_TASK}")
TIME_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_TIME / $HT_U_TIME}")
BRANCH_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_BRANCHES / $HT_U_BRANCHES}")
SIZE_U_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_U_SIZE / $HT_U_SIZE}")

BENCHMARK_TABLE_MICRO=$(cat <<EOF
| Metric | TinyCC (\`tcc\`) | HT-Speed (\`htspeed_cib\`) | Hardware Advantage |
| :--- | :--- | :--- | :--- |
| **CPU Cycles Burned** | **$(fmt_num "$TCC_U_CYCLES")** | **$(fmt_num "$HT_U_CYCLES")** | **${CYCLES_U_RATIO}× FEWER CYCLES** |
| **Kernel Page Faults** | **${TCC_U_FAULTS}** | **${HT_U_FAULTS}** | **${FAULTS_U_RATIO}× FEWER PAGE FAULTS** |
| **Active Task-Clock (CPU time)** | **${TCC_U_TASK} ms** | **${HT_U_TASK} ms** | **${TASK_U_RATIO}× FASTER CPU TIME** |
| **Elapsed Wall-Clock Time** | **${TCC_U_TIME} s** | **${HT_U_TIME} s** | **${TIME_U_RATIO}× FASTER WALL CLOCK** |
| **Branches Evaluated** | **$(fmt_num "$TCC_U_BRANCHES")** | **$(fmt_num "$HT_U_BRANCHES")** | **${BRANCH_U_RATIO}× FEWER BRANCHES** |
| **Output Executable Size** | **${TCC_U_SIZE} bytes** | **${HT_U_SIZE} bytes** | **${SIZE_U_RATIO}× SMALLER (Pure Static)** |
EOF
)

# ============================================================
# 2. MEGA BENCHMARK (1.2 Million Lines / 100,000 Functions)
# ============================================================
echo "[*] Profiling Mega-Benchmark (crazy_test.hts, 50 runs)..."
perf stat -r 50 ./htspeed_cib test_speed/crazy_test.hts /tmp/ht_mega_bin >/dev/null 2> /tmp/perf_ht_mega.txt
perf stat -r 50 tcc test_speed/crazy_test.c -o /tmp/tcc_mega_bin >/dev/null 2> /tmp/perf_tcc_mega.txt

HT_M_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_mega.txt)
TCC_M_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_mega.txt)
HT_M_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_mega.txt)
TCC_M_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_mega.txt)
HT_M_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht_mega.txt)
TCC_M_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc_mega.txt)
HT_M_TASK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_ht_mega.txt)
TCC_M_TASK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_tcc_mega.txt)
HT_M_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_ht_mega.txt)
TCC_M_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_tcc_mega.txt)
HT_M_SIZE=$(wc -c < /tmp/ht_mega_bin | tr -d ' ')
TCC_M_SIZE=$(wc -c < /tmp/tcc_mega_bin | tr -d ' ')

CYCLES_M_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_M_CYCLES / $HT_M_CYCLES}")
FAULTS_M_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_M_FAULTS / $HT_M_FAULTS}")
TASK_M_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_M_TASK / $HT_M_TASK}")
TIME_M_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_M_TIME / $HT_M_TIME}")
BRANCH_M_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_M_BRANCHES / $HT_M_BRANCHES}")

BENCHMARK_TABLE_MEGA=$(cat <<EOF
| Metric | TinyCC (\`tcc\`) | HT-Speed (\`htspeed_cib\`) | Hardware Advantage |
| :--- | :--- | :--- | :--- |
| **Elapsed Wall-Clock Time** | **${TCC_M_TIME} s** | **${HT_M_TIME} s** | **${TIME_M_RATIO}× FASTER WALL CLOCK** |
| **Active Task-Clock (CPU time)** | **${TCC_M_TASK} ms** | **${HT_M_TASK} ms** | **${TASK_M_RATIO}× FASTER CPU TIME** |
| **CPU Cycles Burned** | **$(fmt_num "$TCC_M_CYCLES")** | **$(fmt_num "$HT_M_CYCLES")** | **${CYCLES_M_RATIO}× FEWER CYCLES** |
| **Kernel Page Faults** | **$(fmt_num "$TCC_M_FAULTS")** | **$(fmt_num "$HT_M_FAULTS")** | **${FAULTS_M_RATIO}× FEWER PAGE FAULTS** |
| **Branches Evaluated** | **$(fmt_num "$TCC_M_BRANCHES")** | **$(fmt_num "$HT_M_BRANCHES")** | **${BRANCH_M_RATIO}× FEWER BRANCHES** |
| **Output Executable Size** | **$(ls -lh /tmp/tcc_mega_bin | awk '{print $5}') (${TCC_M_SIZE} B)** | **$(ls -lh /tmp/ht_mega_bin | awk '{print $5}') (${HT_M_SIZE} B)** | **Pure Static x86-64 ELF** |
EOF
)

# Extract live compiler size
COMPILER_SIZE=$(ls -lh htspeed_cib | awk '{print $5}')

# Extract total test count
bash tests/setup.sh > /dev/null
TEST_COUNT=$(ls -1 tests/*.hts | wc -l)

# Compile Hello World & Tic-Tac-Toe dynamically for exact sizes
./htspeed_cib examples/hello.hts /tmp/hello_bin >/dev/null
HELLO_SIZE=$(wc -c < /tmp/hello_bin | tr -d ' ')

./htspeed_cib examples/ttt.hts /tmp/ttt_bin >/dev/null
TTT_SIZE=$(wc -c < /tmp/ttt_bin | tr -d ' ')

# Extract clean directory tree respecting .gitignore
DIR_TREE=$(tree --gitignore -I "generate_readme.sh|README.template.md|DOCUMENTATION.md")

echo "[*] Injecting live stats into README.md..."

cp README.template.md README.md

awk -v t_micro="$BENCHMARK_TABLE_MICRO" \
    -v t_mega="$BENCHMARK_TABLE_MEGA" \
    -v size="$COMPILER_SIZE" \
    -v tests="$TEST_COUNT" \
    -v tree="$DIR_TREE" \
    -v faults="$HT_U_FAULTS" \
    -v hello_sz="$HELLO_SIZE" \
    -v ttt_sz="$TTT_SIZE" '
    /\{\{BENCHMARK_TABLE_MICRO\}\}/ { print t_micro; next }
    /\{\{BENCHMARK_TABLE_MEGA\}\}/ { print t_mega; next }
    /\{\{DIR_TREE\}\}/ { print tree; next }
    {
        gsub(/\{\{COMPILER_SIZE\}\}/, size)
        gsub(/\{\{TEST_COUNT\}\}/, tests)
        gsub(/\{\{PAGE_FAULTS\}\}/, faults)
        gsub(/\{\{HELLO_SIZE\}\}/, hello_sz)
        gsub(/\{\{TTT_SIZE\}\}/, ttt_sz)
        print
    }
' README.template.md > README.md

rm -f /tmp/ht_micro_bin /tmp/tcc_micro_bin /tmp/perf_ht_micro.txt /tmp/perf_tcc_micro.txt
rm -f /tmp/ht_mega_bin /tmp/tcc_mega_bin /tmp/perf_ht_mega.txt /tmp/perf_tcc_mega.txt
rm -f /tmp/hello_bin /tmp/ttt_bin

echo "[+] README.md generated successfully with both Live Benchmarks!"
