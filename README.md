# HT-Speed

**The Sub-Millisecond, Zero-Libc x86-64 Native Compiler**

HT-Speed is an ultra-minimalist, single-pass native compiler that translates a clean, human-friendly systems language directly into standalone Linux x86-64 static ELF executables. It operates without intermediate assembly text, without libc, and without external linkers.

Built by [TheMaster1127](https://github.com/TheMaster1127) using [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated).

---

## Table of Contents
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [Live Hardware Silicon Benchmarks (vs. TinyCC)](#live-hardware-silicon-benchmarks-vs-tinycc)
- [Directory & Project Layout](#directory--project-layout)
- [Architecture & The Unity Build Engine](#architecture--the-unity-build-engine)
- [Design Decisions & Hardware Philosophy](#design-decisions--hardware-philosophy)
  - [1. Why Single-Segment RWX (The 120-Byte Rule)](#1-why-single-segment-rwx-the-120-byte-rule)
  - [2. The 18 Page-Fault Breakthrough (Demand Paging Awareness)](#2-the-18-page-fault-breakthrough-demand-paging-awareness)
  - [3. Separation of Concerns: `tests/` vs `examples/`](#3-separation-of-concerns-tests-vs-examples)
  - [4. The Live Automated Benchmark & README Pipeline](#4-the-live-automated-benchmark--readme-pipeline)
- [Building HT-Speed](#building-ht-speed)
- [Running the Test Suite](#running-the-test-suite)
- [Language Overview & Documentation](#language-overview--documentation)
- [Complete Runnable Examples](#complete-runnable-examples)
  - [1. Minimal Hello World](#1-minimal-hello-world)
  - [2. Structs, Functions & String Concat](#2-structs-functions--string-concat)
  - [3. Dynamic Heap Bubble Sort](#3-dynamic-heap-bubble-sort)
  - [4. Interactive Zero-Libc Tic-Tac-Toe](#4-interactive-zero-libc-tic-tac-toe)
- [The 120-Byte ELF Layout](#the-120-byte-elf-layout)
- [Author & Ecosystem](#author--ecosystem)
- [License](#license)

---

## The Problem

Modern language toolchains suffer from massive abstraction layers:
* **Compilation Latency:** Compiling trivial programs with GCC or Clang takes 30–50 ms and burns 80–120 million CPU instructions.
* **C Runtime Baggage:** Even static C binaries carry dozens of kilobytes of startup routines (`crt1.o`), symbol tables, and unused dynamic resolution code.
* **Serialization Tax:** Traditional compilers generate an AST, serialize it to an intermediate representation (IR), emit textual assembly (`.s`), write it to disk, and invoke external assemblers and linkers to assemble it back into binary machine code.

---

## The Solution

HT-Speed flattens the entire compilation pipeline into pure memory operations:
* **Zero Assembly Text:** Source tokens are translated directly into physical x86-64 machine code bytes in a single streaming pass.
* **Zero Libc Runtime:** Output binaries communicate directly with the Linux kernel via raw hardware syscalls (`sys_write`, `sys_read`, `sys_mmap`, `sys_munmap`, `sys_exit`).
* **Sub-Millisecond Speed:** The compiler executes directly on bare-metal silicon in sub-400 microseconds, burns ~33,000 CPU cycles, and completes entire compilations inside 18 page faults.

---

## Live Hardware Silicon Benchmarks (vs. TinyCC)

Measured on **Artix Linux x86-64 physical hardware** across **100 consecutive runs back-to-back** using `perf stat -r 100` compiling identical workloads (`test_speed/test.hts` vs `test_speed/test.c`):

| Metric | TinyCC (`tcc`) | HT-Speed (`htspeed_cib`) | Hardware Advantage |
| :--- | :--- | :--- | :--- |
| **CPU Cycles Burned** | **5,675,793** | **36,248** | **156.6× FEWER CYCLES** |
| **Kernel Page Faults** | **327** | **18** | **18.2× FEWER PAGE FAULTS** |
| **Active Task-Clock (CPU time)** | **2.80 ms** | **0.27 ms** | **10.4× FASTER CPU TIME** |
| **Elapsed Wall-Clock Time** | **0.003007217 s** | **0.000400321 s** | **7.5× FASTER WALL CLOCK** |
| **Branches Evaluated** | **1,584,577** | **9,890** | **160.2× FEWER BRANCHES** |
| **Output Executable Size** | **4842 bytes** | **945 bytes** | **5.1× SMALLER (Pure Static)** |

> **Demand-Paging Efficiency:** HT-Speed uses high-efficiency BSS tracking, completely avoiding bulk memory wipes. The compiler only touches the physical pages it writes, allowing it to complete entire compilations inside **18 page faults**.

---

## Directory & Project Layout

The repository is organized into a clean, modular structure (automatically generated from `.gitignore`):

```text
.
├── examples
│   ├── bubble.hts
│   ├── demo.hts
│   ├── echo.hts
│   ├── hello.hts
│   ├── math_stress.hts
│   ├── mega_test.hts
│   ├── nested.hts
│   ├── numbers.hts
│   ├── ttt.hts
│   └── v4_test.hts
├── htspeed_cib.c
├── LICENSE
├── README.md
├── src
│   ├── compiler.h
│   ├── elf.h
│   ├── emitter.h
│   ├── expr.h
│   ├── lexer.h
│   ├── parser.h
│   ├── runtimes.h
│   ├── stmt.h
│   ├── string.h
│   ├── syscalls.h
│   ├── tables.h
│   └── types.h
├── tests
│   ├── 01_hello.expected
│   ├── 01_hello.hts
│   ├── 02_function.expected
│   ├── 02_function.hts
│   ├── 03_fib.expected
│   ├── 03_fib.hts
│   ├── 04_loop.expected
│   ├── 04_loop.hts
... more ...
│   ├── 55_nested_calls_as_args.expected
│   ├── 55_nested_calls_as_args.hts
│   ├── 56_pass_by_pointer.expected
│   ├── 56_pass_by_pointer.hts
│   ├── 57_dot_field_conflict.expected
│   ├── 57_dot_field_conflict.hts
│   ├── 58_struct_field_collision.expected
│   ├── 58_struct_field_collision.hts
│   ├── inc_helper.inc
│   └── setup.sh
├── test.sh
└── test_speed
    ├── test.c
    └── test.hts

5 directories, 148 files
```

---

## Architecture & The Unity Build Engine

HT-Speed uses a **Unity Build** architecture. Rather than compiling independent `.c` files into `.o` objects and paying linker overhead, `htspeed_cib.c` includes the headers in `src/` directly into a single translation unit.

```text
[ Source Code (.hts) ]
         │
         ▼
 ┌───────────────┐
 │  src/lexer.h  │  Single-pass streaming tokenizer
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │  src/parser.h │  Pratt parser (src/expr.h + src/stmt.h)
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │ src/emitter.h │  Direct x86-64 machine code byte emission
 └───────┬───────┘
         ▼
 ┌───────────────┐
 │ src/runtimes.h│  Backpatches relocations & appends inlined runtimes
 └───────┬───────┘
         ▼
[ 120-Byte Header + Native Machine Code ] ──► Written directly to disk
```

### Compiler Architecture Guarantees:
1. **Contiguous Cache Packing:** Hot functions (`emit_u8`, `next_token`) are inlined by GCC `-O2`, packing execution instructions into the same L1 cache lines.
2. **Dead-Code Elimination:** If a program does not print dynamic signed numbers, the 111-byte `itoa` routine is completely omitted from the binary. If `exit()` is called explicitly, the fallback exit sequence is suppressed.
3. **Zero Intermediate Disk I/O:** The compiler does not create intermediate `.s` or `.o` files. Machine code is emitted directly into RAM buffers and flushed to disk in a single write operation.

---

## Design Decisions & Hardware Philosophy

### 1. Why Single-Segment RWX (The 120-Byte Rule)

Modern compilers split binaries into multiple segments: `RX` (Read + Execute) for machine code and `RW` (Read + Write) for data to satisfy the security principle of **W^X (Write XOR Execute)**.

In HT-Speed, binaries are emitted with a single **RWX** segment (`PF_R | PF_W | PF_X`). This was a conscious architectural trade-off:

* **The 120-Byte Record:** By using a single program header (`Elf64_Phdr`), the entire ELF header footprint is exactly **120 bytes** ($64\text{B Ehdr} + 56\text{B Phdr}$).
* **Zero Page-Padding Bloat:** The Linux kernel ELF loader requires separate memory segments to be page-aligned in virtual memory ($0x1000 = 4,096\text{ bytes}$). Separating code from data into `RX` and `RW` segments forces a minimum file padding of 4 KB. 
* By using a unified RWX layout, code and data sit contiguously in the same memory page. A complete, working executable can be as small as **198 bytes** on disk without padding penalties.

### 2. The 18 Page-Fault Breakthrough (Demand Paging Awareness)

Earlier versions of the compiler suffered from ~121 kernel page faults per compilation. Profile analysis revealed that the compiler was executing:

```c
m_memset(&C, 0, sizeof(C)); // Wiping ~418 KB of static compiler buffers
```

Under Linux, static uninitialized memory (`.bss`) is mapped via **Copy-On-Write (COW)** to a system-wide read-only zero page. When you write to that memory, the MMU triggers a page fault (Hardware Trap 14), forcing the kernel to allocate a physical 4 KB RAM page.

Wiping a 418 KB struct triggered **over 100 consecutive kernel page faults** on memory that was never even used by the program being compiled. By deleting the bulk `memset` and only resetting scalar tracking integers (`code_len = 0`, `data_len = 16`, etc.), page faults instantly dropped from **121 to 18**, and CPU cycles dropped from **116,000 to ~33,600** (a 3.4× speedup).

### 3. Separation of Concerns: `tests/` vs `examples/`

To prevent repository rot and maintain strict compiler verification:

* **`tests/` (Automated Regression Suite):** Hermetic, deterministic test files generated programmatically by `tests/setup.sh`. Every test has an accompanying `.expected` output, an optional `.args` file, and an exit-code contract. Run via `./test.sh` to stress every instruction, boundary condition, and parser edge case.
* **`examples/` (Real-World Programs):** Clean, human-readable showcase programs (`hello.hts`, `bubble.hts`, and full terminal games like `ttt.hts`) designed for developers to read, inspect, and run directly.

### 4. The Live Automated Benchmark & README Pipeline

Most compiler documentation features stale or hallucinated benchmark numbers. HT-Speed eliminates documentation drift with `generate_readme.sh`:

1. It compiles the current engine and runs `perf stat -r 100` on physical silicon against TinyCC.
2. It extracts live CPU cycles, page faults, task-clock execution time, and branch counts directly from hardware performance counters.
3. It compiles the example binaries to pull live on-disk byte counts.
4. It reads `.gitignore` and generates an accurate directory tree.
5. It injects all real physical metrics into `README.template.md` to produce the final `README.md`.

---

## Building HT-Speed

Compiling HT-Speed requires [cib](https://github.com/TheMaster1127/C-is-bloated), producing a **23K static compiler binary**:

```bash
# Compile HT-Speed with balanced bare-metal optimization (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: 23K htspeed_cib
```

> **Note on -Z flag:** Do NOT use the `-Z5` flag. It causes a segmentation fault triggered by GCC's `-O3` optimization tier in the background. Anything else is valid. Always use either `-Z4` for maximum compilation speed of your compiler, or `-Z0` (the default) for the smallest compiler binary size. Even the Linux kernel refuses to compile with `-O3` because it breaks when you push C to the bare metal.

---

## Running the Test Suite

The test suite contains **58 automated test cases** covering arithmetic, recursion, loops, nested breaks, mutual recursion, bitwise logic, heap bubble sorting, and command-line parsing.

```bash
# Run all 58 tests
./test.sh

# Debug a specific test (e.g. test 45) with verbose source, diff, and hexdumps
./test.sh 45
```

---

## Language Overview & Documentation

For the complete in-depth language manual, see [DOCUMENTATION.md](DOCUMENTATION.md).

### Quick Syntax Snapshot

```htvm
; AutoHotKey-style semicolon comments
struct Player {
    int health
    int speed
}

func int double_val(int n) {
    return n * 2
}

main
; Automatic heap allocation with 'new'
int p := new Player
p.health := 100
p.speed := double_val(25)

; Strict separation: ':=' is assignment, '=' is equality
if (p.health = 100) and (p.speed = 50) {
    print("Hero ready!\n")
}

; Dynamic string concatenation with '.'
str greeting := "Status: " . "Operational\n"
print(greeting)

exit(0)
```

---

## Complete Runnable Examples

### 1. Minimal Hello World

`examples/hello.hts`:
```htvm
main
print("Hello, World!\n")
exit(0)
```

Compile and inspect:
```bash
./htspeed_cib examples/hello.hts hello
./hello
# Output: Hello, World!

ls -lh hello
# Output: 198 bytes!
```

---

### 2. Structs, Functions & String Concat

`examples/demo.hts`:
```htvm
struct Player {
    int health
    int speed
}

func int heal(int current_hp, int amount) {
    return current_hp + amount
}

main
int p := new Player
p.health := 100
p.speed := 25

p.health := heal(p.health, 50)
print("Player HP:\n")
print(p.health)

str msg := "Status: " . "Ready!\n"
print(msg)
exit(0)
```

---

### 3. Dynamic Heap Bubble Sort

`examples/bubble.hts`:
```htvm
main
int arr := alloc(40) ; Space for 5 integers (5 * 8 bytes)
[arr + 0] := 50
[arr + 8] := 20
[arr + 16] := 40
[arr + 24] := 10
[arr + 32] := 30

int n := 5
int i := 0
while (i < n) {
    int j := 0
    while (j < n - 1) {
        int a := [arr + j * 8]
        int b := [arr + (j + 1) * 8]
        if (a > b) {
            [arr + j * 8] := b
            [arr + (j + 1) * 8] := a
        }
        j := j + 1
    }
    i := i + 1
}

int k := 0
while (k < n) {
    print([arr + k * 8])
    k := k + 1
}
exit(0)
```

---

### 4. Interactive Zero-Libc Tic-Tac-Toe

See `examples/ttt.hts` for a complete, two-player interactive terminal Tic-Tac-Toe featuring real-time board rendering, input reading via `sys_read`, 8-way line checking, and draw evaluation:

```bash
./htspeed_cib examples/ttt.hts ttt
./ttt
```

Output binary size: **3782 bytes** (statically linked, zero libc).

---

## The 120-Byte ELF Layout

HT-Speed emits static Linux ELF executables using a minimal single RWX segment:

```text
┌────────────────────────────────────────────────────────┐
│ Elf64_Ehdr (64 bytes)                                  │ ◄── e_entry: 0x400078
├────────────────────────────────────────────────────────┤
│ Elf64_Phdr (56 bytes, PT_LOAD, PF_R | PF_W | PF_X)     │ ◄── Maps executable to 0x400000
├────────────────────────────────────────────────────────┤
│ Machine Code (Direct x86-64 machine instructions)      │ ◄── Entry point execution
├────────────────────────────────────────────────────────┤
│ Data Pool (String literals & Global variables)         │ ◄── Position-independent [RIP + disp]
└────────────────────────────────────────────────────────┘
```

* **ELF Header:** 64 bytes
* **Program Header:** 56 bytes
* **Combined Headers:** Exactly 120 bytes ($64 + 56$).
* **Section Headers:** 0 (Completely omitted; unnecessary for direct execution).
* **Dependencies:** 0 (`ldd` reports `not a dynamic executable`).

---

## Author & Ecosystem

Developed by **TheMaster1127** (aka *Mr. Compiler*), a low-level systems programmer and reverse engineer.

* **GitHub:** [@TheMaster1127](https://github.com/TheMaster1127)
* **Related Projects:**
  * [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated) — Strips C binaries down to 169 bytes with zero libc.
  * [binpatch](https://github.com/TheMaster1127/binpatch) — Binary patching and inspection utility.
  * [HT-RE](https://github.com/TheMaster1127/HT-RE) — Reverse engineering suite for Linux.

---

## License

This project is open-source software licensed under the **GNU General Public License v3.0 (GPLv3)**.
