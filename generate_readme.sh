#!/bin/bash
# generate_readme.sh — Automated README generator for HT-Speed
# Measures physical silicon metrics live via perf stat -r 100

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

echo "[*] Profiling HT-Speed on physical silicon (perf stat -r 100)..."
perf stat -r 100 ./htspeed_cib test_speed/test.hts /tmp/ht_bench_bin >/dev/null 2> /tmp/perf_ht.txt

echo "[*] Profiling TinyCC on physical silicon (perf stat -r 100)..."
perf stat -r 100 tcc test_speed/test.c -o /tmp/tcc_bench_bin >/dev/null 2> /tmp/perf_tcc.txt

# Extract exact hardware metrics
HT_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht.txt)
TCC_CYCLES=$(awk '/cpu-cycles/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc.txt)

HT_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht.txt)
TCC_FAULTS=$(awk '/page-faults/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc.txt)

HT_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_ht.txt)
TCC_BRANCHES=$(awk '/branches/ && !/branch-misses/ {gsub(/,/, ""); print $1; exit}' /tmp/perf_tcc.txt)

HT_TASK_CLOCK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_ht.txt)
TCC_TASK_CLOCK=$(awk '/task-clock/ {print $1; exit}' /tmp/perf_tcc.txt)

HT_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_ht.txt)
TCC_TIME=$(awk '/seconds time elapsed/ {print $1; exit}' /tmp/perf_tcc.txt)

HT_SIZE=$(wc -c < /tmp/ht_bench_bin | tr -d ' ')
TCC_SIZE=$(wc -c < /tmp/tcc_bench_bin | tr -d ' ')

# Compute live ratios
CYCLES_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_CYCLES / $HT_CYCLES}")
FAULTS_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_FAULTS / $HT_FAULTS}")
TASK_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_TASK_CLOCK / $HT_TASK_CLOCK}")
TIME_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_TIME / $HT_TIME}")
BRANCH_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_BRANCHES / $HT_BRANCHES}")
SIZE_RATIO=$(awk "BEGIN {printf \"%.1f\", $TCC_SIZE / $HT_SIZE}")

fmt_num() {
    printf "%'d" "$1" 2>/dev/null || echo "$1"
}

HT_CYCLES_FMT=$(fmt_num "$HT_CYCLES")
TCC_CYCLES_FMT=$(fmt_num "$TCC_CYCLES")
HT_BRANCHES_FMT=$(fmt_num "$HT_BRANCHES")
TCC_BRANCHES_FMT=$(fmt_num "$TCC_BRANCHES")

BENCHMARK_TABLE=$(cat <<EOF
| Metric | TinyCC (\`tcc\`) | HT-Speed (\`htspeed_cib\`) | Hardware Advantage |
| :--- | :--- | :--- | :--- |
| **CPU Cycles Burned** | **${TCC_CYCLES_FMT}** | **${HT_CYCLES_FMT}** | **${CYCLES_RATIO}× FEWER CYCLES** |
| **Kernel Page Faults** | **${TCC_FAULTS}** | **${HT_FAULTS}** | **${FAULTS_RATIO}× FEWER PAGE FAULTS** |
| **Active Task-Clock (CPU time)** | **${TCC_TASK_CLOCK} ms** | **${HT_TASK_CLOCK} ms** | **${TASK_RATIO}× FASTER CPU TIME** |
| **Elapsed Wall-Clock Time** | **${TCC_TIME} s** | **${HT_TIME} s** | **${TIME_RATIO}× FASTER WALL CLOCK** |
| **Branches Evaluated** | **${TCC_BRANCHES_FMT}** | **${HT_BRANCHES_FMT}** | **${BRANCH_RATIO}× FEWER BRANCHES** |
| **Output Executable Size** | **${TCC_SIZE} bytes** | **${HT_SIZE} bytes** | **${SIZE_RATIO}× SMALLER (Pure Static)** |
EOF
)

# Extract live compiler size
COMPILER_SIZE=$(ls -lh htspeed_cib | awk '{print $5}')

# Extract total test count
bash tests/setup.sh > /dev/null
TEST_COUNT=$(ls -1 tests/*.hts | wc -l)

# Compile Hello World & Tic-Tac-Toe dynamically to measure exact byte sizes
./htspeed_cib examples/hello.hts /tmp/hello_bin >/dev/null
HELLO_SIZE=$(wc -c < /tmp/hello_bin | tr -d ' ')

./htspeed_cib examples/ttt.hts /tmp/ttt_bin >/dev/null
TTT_SIZE=$(wc -c < /tmp/ttt_bin | tr -d ' ')

# Extract clean directory tree respecting .gitignore
DIR_TREE=$(tree --gitignore -I "generate_readme.sh|README.template.md|DOCUMENTATION.md")

echo "[*] Injecting live stats into README.md..."

# Create README.md from template
cp README.template.md README.md

awk -v table="$BENCHMARK_TABLE" \
    -v size="$COMPILER_SIZE" \
    -v tests="$TEST_COUNT" \
    -v tree="$DIR_TREE" \
    -v faults="$HT_FAULTS" \
    -v hello_sz="$HELLO_SIZE" \
    -v ttt_sz="$TTT_SIZE" '
    /\{\{BENCHMARK_TABLE\}\}/ { print table; next }
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

rm -f /tmp/ht_bench_bin /tmp/tcc_bench_bin /tmp/perf_ht.txt /tmp/perf_tcc.txt /tmp/hello_bin /tmp/ttt_bin
echo "[+] README.md generated successfully with 100-run live stats!"
