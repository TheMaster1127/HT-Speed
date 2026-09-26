# HT-Speed

**The Sub-Millisecond, Zero-Libc x86-64 Native Compiler**

HT-Speed is an ultra-minimalist, single-pass native compiler that translates a clean, human-friendly systems language directly into standalone Linux x86-64 static ELF executables. It operates without intermediate assembly text, without libc, and without external linkers.

Built by [TheMaster1127](https://github.com/TheMaster1127) using [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated).

---

## Table of Contents
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [Hardware Silicon Benchmarks (vs. TinyCC)](#hardware-silicon-benchmarks-vs-tinycc)
- [Directory & Project Layout](#directory--project-layout)
- [Architecture & The Unity Build Engine](#architecture--the-unity-build-engine)
- [Building HT-Speed](#building-ht-speed)
- [Running the Test Suite](#running-the-test-suite)
- [Language Reference](#language-reference)
  - [1. Program Structure & Entry Point (`main`)](#1-program-structure--entry-point-main)
  - [2. Comments (`;`, `//`, `#`, `/* */`)](#2-comments)
  - [3. Types & Memory Model](#3-types--memory-model)
  - [4. Variables & Assignment (`:=` vs `=`)](#4-variables--assignment--vs-)
  - [5. Structs & Automatic Allocation (`new`)](#5-structs--automatic-allocation-new)
  - [6. Operators & Precedence](#6-operators--precedence)
  - [7. Control Flow (`if` / `else`)](#7-control-flow-if--else)
  - [8. Counted Loops (`Loop, count` & `A_Index`)](#8-counted-loops-loop-count--a_index)
  - [9. Conditional Loops (`while`)](#9-conditional-loops-while)
  - [10. Dynamic Memory & Dereferencing (`alloc`, `[ptr]`, `byte[ptr]`)](#10-dynamic-memory--dereferencing)
  - [11. Printing (`print`)](#11-printing-print)
  - [12. Command-Line Arguments (`GetParams`)](#12-command-line-arguments-getparams)
  - [13. Kernel Syscalls (`syscall`)](#13-kernel-syscalls-syscall)
  - [14. File Inclusion (`include`)](#14-file-inclusion-include)
- [Complete Runnable Examples](#complete-runnable-examples)
  - [Minimal Hello World (198 Bytes)](#minimal-hello-world-198-bytes)
  - [Structs, Functions & String Concat](#structs-functions--string-concat)
  - [Dynamic Heap Bubble Sort](#dynamic-heap-bubble-sort)
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
* **Sub-Millisecond Speed:** The compiler runs in **~330 microseconds**, burns **34,000 CPU cycles**, incurs only **19 kernel page faults**, and outputs standalone executables starting at **198 bytes**.

---

## Hardware Silicon Benchmarks (vs. TinyCC)

Measured on **Artix Linux x86-64 physical hardware** (averaged across **100 consecutive runs back-to-back** using `perf stat -r 100` compiling identical workloads):

### Workload: `test.hts` vs `test.c` (Nested calls, loops, variables, math, branching, and I/O)

| Metric | TinyCC (`tcc`) | HT-Speed (`htspeed_cib`) | Hardware Advantage |
| :--- | :--- | :--- | :--- |
| **CPU Cycles Burned** | **5,355,076** | **33,805** | **158.4× FEWER CYCLES** |
| **Kernel Page Faults** | **327** | **19** | **17.2× FEWER PAGE FAULTS** |
| **Active Task-Clock (CPU time)** | **2.76 ms** | **0.28 ms (280 µs)** | **9.8× FASTER CPU TIME** |
| **Elapsed Wall-Clock Time** | **2.95 ms** | **0.39 ms (390 µs)** | **7.5× FASTER WALL CLOCK** |
| **Branches Evaluated** | **1,531,439** | **9,798** | **156× FEWER BRANCHES** |
| **Output Executable Size** | **4,800 bytes** | **929 bytes** | **5.1× SMALLER (Pure Static)** |

> **Demand-Paging Efficiency:** HT-Speed uses high-efficiency BSS tracking, completely avoiding bulk memory wipes. The compiler only touches the physical pages it writes, allowing it to complete entire compilations inside **19 page faults**.

---

## Directory & Project Layout

The repository is organized into a clean, modular structure:

```text
HT-Speed/
├── src/                  # Core compiler engine modules
│   ├── types.h           # Primitive integer types, sizing limits, and bounds
│   ├── syscalls.h        # Direct Linux kernel syscall wrappers (zero libc)
│   ├── string.h          # Minimal memory and string manipulation helpers
│   ├── elf.h             # Minimal 64-bit ELF header definitions
│   ├── tables.h          # Symbol tables (locals, globals, structs, fixups)
│   ├── emitter.h         # Machine code byte emission primitives
│   ├── runtimes.h        # 111-byte itoa, string concat, and ELF file writer
│   ├── expr.h            # Pratt expression parser and operator precedence
│   ├── stmt.h            # Statement parsing, control flow, loops, and calls
│   ├── parser.h          # Unified parser interface
│   └── compiler.h        # Recursive file inclusion and driver loop
├── examples/             # Complete runnable language examples
│   ├── hello.hts         # 198-byte Hello World
│   ├── numbers.hts       # Fibonacci and signed itoa demonstration
│   ├── nested.hts        # Nested counted loops with A_Index
│   ├── math_stress.hts   # Arithmetic, modulo, and precedence stress
│   ├── mega_test.hts     # Heap memory, while loops, and clean I/O
│   ├── echo.hts          # Zero-libc raw terminal echo
│   └── v4_test.hts       # Structs, bitwise, string concat, and GetParams
├── test_speed/           # Benchmark comparison workloads
│   ├── test.hts          # HT-Speed benchmark workload
│   └── test.c            # Equivalent C benchmark workload for TCC
├── tests/                # Automated test suite (50 test cases)
│   ├── setup.sh          # Test suite generator script
│   └── test.sh           # Test harness runner
├── htspeed_cib.c         # Root compiler driver (extracts [rsp + 8])
├── test.sh               # Root test runner script
└── README.md
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

Compiling HT-Speed requires [cib](https://github.com/TheMaster1127/C-is-bloated), which strips all standard library bloat and produces a **22 KB static compiler binary**:

```bash
# Compile HT-Speed with balanced bare-metal optimization (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: 22K htspeed_cib
```

> **Note on -Z flag:** Do NOT use the `-Z5` flag. It causes a segmentation fault triggered by GCC's `-O3` optimization tier in the background. Anything else is valid. Always use either `-Z4` for maximum compilation speed of your compiler, or `-Z0` (the default) for the smallest compiler binary size. Before blaming me for any issues: this is not my fault, it is 100% GCC's fault. Even the Linux kernel refuses to compile with `-O3` because it breaks when you push C to the bare metal. So do NOT use `-Z5` (`-O3`), as GCC's aggressive loop vectorization assumes 16-byte aligned glibc stack frames that break bare-metal runtimes.

---

## Running the Test Suite

The test suite contains **50 automated test cases** covering arithmetic, recursion, loops, nested breaks, mutual recursion, bitwise logic, heap bubble sorting, and command-line parsing.

```bash
# Run all 50 tests
./test.sh

# Debug a specific test (e.g. test 45) with verbose source, diff, and hexdumps
./test.sh 45
```

---

## Language Reference

### 1. Program Structure & Entry Point (`main`)

An HT-Speed program consists of top-level definitions (structs, globals, functions) followed by a mandatory **`main`** entry point. Execution begins directly at the first instruction beneath `main`.

```htvm
; Top-level function
func int double_val(int n) {
    return n * 2
}

; Entry point (Maps to 0x400078 in the physical ELF header)
main
int x := double_val(21)
print(x)
exit(0)
```

* **No Semicolons on Statements:** Statements are delimited by newlines.
* **Top-Level Ordering:** All `struct`, global variables, and `func` declarations must be declared above `main`.

---

### 2. Comments

HT-Speed supports single-line and multi-line comments:

```htvm
; AutoHotKey-style single-line comment
// C-style single-line comment
# Shell-style single-line comment

/*
   Multi-line block comment
*/
```

---

### 3. Types & Memory Model

All data primitives operate as 64-bit machine words:

| Type | Word Size | Register/Stack Layout | Description |
| :--- | :---: | :--- | :--- |
| **`int`** | 8 bytes | 64-bit signed integer | Decimal (`42`, `-10`) or Hexadecimal (`0xFF`) |
| **`str`** | 8 bytes | 64-bit memory pointer | Pointer to null-terminated ASCII bytes in pool or heap |
| **`bool`**| 8 bytes | 64-bit integer (`1` or `0`) | Boolean truth values |
| **`byte`**| 1 byte  | 8-bit unsigned integer | Used for byte dereferencing (`byte[ptr]`) |
| **`void`**| 0 bytes | None | Used for functions that return no value |

---

### 4. Variables & Assignment (`:=` vs `=`)

To eliminate syntax ambiguity, HT-Speed enforces a strict separation between assignment and equality:

* **`:=` is ALWAYS assignment:**
  ```htvm
  int count := 10
  count := count + 1
  ```
* **`=` is ALWAYS equality comparison:**
  ```htvm
  if (count = 11) {
      print("Equal!\n")
  }
  ```

#### Global Variables
Declared at the top level before `main` with constant initializers:
```htvm
int g_counter := 500
str g_banner := "Engine Initialized\n"

main
print(g_banner)
print(g_counter)
exit(0)
```

---

### 5. Structs & Automatic Allocation (`new`)

Structs define custom 64-bit word memory layouts. Each declared field receives an automatic 8-byte offset:

```htvm
struct Point {
    int x
    int y
}

main
; 'new' automatically queries the struct size and allocates via sys_mmap
int pt := new Point

pt.x := 100
pt.y := 200

print(pt.x + pt.y)
exit(0)
```

---

### 6. Operators & Precedence

HT-Speed implements a Pratt parser with 11 levels of precedence:

| Level | Operators | Description | Machine Instruction |
| :---: | :--- | :--- | :--- |
| **11** | `*`, `/`, `%` | Multiplication, Division, Modulo | `imul`, `idiv` |
| **10** | `+`, `-` | Addition, Subtraction | `add`, `sub` |
| **9** | `<<`, `>>` | Bitwise Shifts | `shl`, `sar` |
| **8** | `<`, `<=`, `>`, `>=` | Relational Comparisons | `cmp` + `setcc` |
| **7** | `=`, `!=` | Equality, Inequality | `cmp` + `sete` / `setne` |
| **6** | `&` | Bitwise AND | `and` |
| **5** | `^` | Bitwise XOR | `xor` |
| **4** | `\|` | Bitwise OR | `or` |
| **3** | `.` | Dynamic String Concatenation | Inlined `mmap` runtime |
| **2** | `and`, `&&` | Logical AND | Boolean normalization |
| **1** | `or`, `\|\|` | Logical OR | Boolean normalization |

Unary bitwise NOT (`~`) inverts all bits: `int mask := ~0`. Parentheses `(...)` override precedence arbitrarily.

#### Dynamic String Concatenation (`.`)
The `.` operator joins strings dynamically at runtime:
```htvm
main
str first := "Fast "
str second := "Compiler!\n"
str full := first . second
print(full)
exit(0)
```

---

### 7. Control Flow (`if` / `else`)

Condition expressions do not require wrapping parentheses around the entire statement:

```htvm
main
int a := 10
int b := 20

if (a = 10) or (b = 999) {
    print("Or condition matched!\n")
}

if (a = 10) and (b = 20) {
    print("And condition matched!\n")
} else {
    print("Condition failed!\n")
}
exit(0)
```

---

### 8. Counted Loops (`Loop, count` & `A_Index`)

Counted loops provide hardware-efficient iteration with the built-in variable `A_Index` (0-indexed):

```htvm
main
Loop, 5 {
    if (A_Index = 2) {
        continue ; Skip iteration 2
    }
    if (A_Index = 4) {
        break    ; Exit loop
    }
    print(A_Index)
}
exit(0)
```

* **Dynamic Limits:** Count can be an immediate integer (`Loop, 10`) or any valid expression (`Loop, count * 2`).
* **Nesting:** Supported up to 16 levels deep. `A_Index` resolves to the innermost loop's index.

---

### 9. Conditional Loops (`while`)

```htvm
main
int i := 0
while (i < 5) {
    print(i)
    i := i + 1
}
exit(0)
```

---

### 10. Dynamic Memory & Dereferencing

HT-Speed allows direct heap allocation and memory access without standard library wrappers:

#### 1. Heap Allocation (`alloc`)
Requests page-aligned memory directly from the Linux kernel using `sys_mmap`:
```htvm
main
int buf := alloc(64) ; Allocate 64 bytes
[buf] := 1337        ; 64-bit store
print([buf])         ; 64-bit load (prints 1337)
exit(0)
```

#### 2. Byte Dereferencing (`byte[ptr]`)
Reads and writes single 8-bit bytes:
```htvm
main
int buf := alloc(16)
byte[buf + 0] := 72  ; 'H'
byte[buf + 1] := 105 ; 'i'
byte[buf + 2] := 10  ; '\n'

print(buf, 3)        ; Prints raw buffer of 3 bytes
exit(0)
```

---

### 11. Printing (`print`)

The `print()` built-in handles literals, signed integers, and dynamic buffers:

```htvm
main
; 1. Print string literal (zero allocation, RIP-relative sys_write)
print("Hello!\n")

; 2. Print signed integer (invokes internal 111-byte hardware itoa)
print(10 - 50) ; Prints -40

; 3. Print raw memory buffer by pointer and length
int buf := alloc(8)
[buf] := 65
print(buf, 1) ; Prints 'A'
exit(0)
```

---

### 12. Command-Line Arguments (`GetParams`)

Access shell arguments passed to your executable via `GetParams()`:

```htvm
main
; Returns arguments (argv[1]..argv[n]) separated by newlines
str params := GetParams()
print(params)
exit(0)
```

---

### 13. Kernel Syscalls (`syscall`)

Invoke Linux x86-64 kernel syscalls directly without assembly boilerplate:

```htvm
main
int buf := alloc(64)

; sys_read(fd=0, buf=buf, count=64) -> RAX returns bytes read
int bytes_read := syscall(0, 0, buf, 64)

; sys_write(fd=1, buf=buf, count=bytes_read)
syscall(1, 1, buf, bytes_read)

exit(0)
```

* Syscall Register Mapping: `RAX` = number, `RDI` = arg1, `RSI` = arg2, `RDX` = arg3, `R10` = arg4, `R8` = arg5, `R9` = arg6.

---

### 14. File Inclusion (`include`)

Split programs into modular files using `include`:

```htvm
include "math_utils.hts"

main
int result := compute_val(10)
print(result)
exit(0)
```

* Included files are spliced directly into the source stream during lexical preprocessing.

---

## Complete Runnable Examples

### Minimal Hello World (198 Bytes)

Save as `examples/hello.hts`:
```htvm
main
print("Hello, World!\n")
exit(0)
```

Compile and run:
```bash
./htspeed_cib examples/hello.hts hello
./hello
# Output: Hello, World!

ls -lh hello
# Output: 198 bytes!
```

---

### Structs, Functions & String Concat

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
; Allocate struct dynamically
int p := new Player
p.health := 100
p.speed := 25

; Mutate via function call
p.health := heal(p.health, 50)

print("Player HP:\n")
print(p.health)

str msg := "Status: " . "Ready!\n"
print(msg)
exit(0)
```

Compile and run:
```bash
./htspeed_cib examples/demo.hts demo
./demo
```

---

### Dynamic Heap Bubble Sort

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

; Print sorted elements
int k := 0
while (k < n) {
    print([arr + k * 8])
    k := k + 1
}
exit(0)
```

Compile and run:
```bash
./htspeed_cib examples/bubble.hts bubble
./bubble
# Output:
# 10
# 20
# 30
# 40
# 50
```

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
