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
* **Sub-Millisecond Speed:** The compiler executes directly on bare-metal silicon in sub-400 microseconds, burns ~33,000 CPU cycles, and completes entire compilations inside {{PAGE_FAULTS}} page faults.

---

## Live Hardware Silicon Benchmarks (vs. TinyCC)

Measured on **Artix Linux x86-64 physical hardware** across **100 consecutive runs back-to-back** using `perf stat -r 100` compiling identical workloads (`test_speed/test.hts` vs `test_speed/test.c`):

{{BENCHMARK_TABLE}}

> **Demand-Paging Efficiency:** HT-Speed uses high-efficiency BSS tracking, completely avoiding bulk memory wipes. The compiler only touches the physical pages it writes, allowing it to complete entire compilations inside **{{PAGE_FAULTS}} page faults**.

---

## Directory & Project Layout

The repository is organized into a clean, modular structure (automatically generated from `.gitignore`):

```text
{{DIR_TREE}}
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

## Building HT-Speed

Compiling HT-Speed requires [cib](https://github.com/TheMaster1127/C-is-bloated), producing a **{{COMPILER_SIZE}} static compiler binary**:

```bash
# Compile HT-Speed with balanced bare-metal optimization (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: {{COMPILER_SIZE}} htspeed_cib
```

> **Note on -Z flag:** Do NOT use the `-Z5` flag. It causes a segmentation fault triggered by GCC's `-O3` optimization tier in the background. Anything else is valid. Always use either `-Z4` for maximum compilation speed of your compiler, or `-Z0` (the default) for the smallest compiler binary size. Even the Linux kernel refuses to compile with `-O3` because it breaks when you push C to the bare metal.

---

## Running the Test Suite

The test suite contains **{{TEST_COUNT}} automated test cases** covering arithmetic, recursion, loops, nested breaks, mutual recursion, bitwise logic, heap bubble sorting, and command-line parsing.

```bash
# Run all {{TEST_COUNT}} tests
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

Save as `examples/hello.hts`:
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
# Output: {{HELLO_SIZE}} bytes!
```

---

### 2. Structs, Functions & String Concat

Save as `examples/demo.hts`:
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

Save as `examples/bubble.hts`:
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

Output binary size: **{{TTT_SIZE}} bytes** (statically linked, zero libc).

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
