# HT-Speed

**The Sub-Millisecond, Zero-Libc x86-64 Native Compiler**

HT-Speed is an ultra-minimalist, single-pass compiler that compiles a clean, human-friendly systems language directly into standalone Linux x86-64 ELF executables without intermediate assembly text, without libc, and without external linkers.

Built by [TheMaster1127](https://github.com/TheMaster1127) using [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated).

---

## Table of Contents
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [Benchmarks](#benchmarks)
- [Architecture](#architecture)
- [Building HT-Speed](#building-ht-speed)
  - [Option 1: Build with CIB (Recommended — 12 KB Binary)](#option-1-build-with-cib-recommended)
  - [Option 2: Build with GCC](#option-2-build-with-gcc)
- [Language Reference](#language-reference)
  - [Program Structure (`func` and `main`)](#program-structure)
  - [Types & Literals](#types--literals)
  - [Variables & Assignment (`:=`)](#variables--assignment-)
  - [Operators & The Equality Rule (`=` vs `:=`)](#operators--the-equality-rule)
  - [Control Flow (`if` / `else`)](#control-flow-if--else)
  - [Loops (`Loop, count` and `A_Index`)](#loops-loop-count-and-a_index)
  - [Printing (`print` with Strings & Dynamic Numbers)](#printing)
  - [Raw Linux Syscalls (`syscall`)](#raw-linux-syscalls)
- [Complete Examples](#complete-examples)
  - [1. Hello World](#1-hello-world)
  - [2. Recursive Fibonacci](#2-recursive-fibonacci)
  - [3. Direct Syscall File Creation](#3-direct-syscall-file-creation)
- [The 120-Byte ELF Layout](#the-120-byte-elf-layout)
- [Current Limits](#current-limits)
- [Author & Ecosystem](#author--ecosystem)

---

## The Problem

Modern compilers are trapped in abstraction bloat:
* **Compilation Latency:** Compiling a 10-line program in GCC or Clang takes 30–50ms and burns 80–120 million CPU instructions.
* **Binary Bloat:** Standard toolchains link hundreds of kilobytes of C runtime boilerplate (`crt1.o`, dynamic linkers, libc tables).
* **Intermediate Representation Tax:** Compilers serialize ASTs into intermediate assembly text (`.s`), write it to disk, and then spawn an assembler subprocess to parse it back into binary opcodes.

## The Solution

HT-Speed eliminates the entire pipeline:
* **Zero Assembly Text:** Translates source tokens directly into raw x86-64 machine code bytes in memory.
* **Zero Libc Runtime:** Output binaries communicate directly with the Linux kernel via raw syscalls (`sys_write`, `sys_exit`, `sys_open`).
* **Microscopic Footprint:** The compiler itself is an **11.9 KB** static binary, compiles user programs in **400 microseconds**, and outputs standalone executables as small as **159 bytes**.

---

## Benchmarks

Measured on **Artix Linux x86-64**, physical silicon, averaged across **100 runs back-to-back** using Linux `perf stat -r 100`:

### Workload: Multi-Function Program with Math, Loops, Calls & Syscalls

| Metric | TinyCC (`tcc`) | HT-Speed (`htspeed_cib`) | Advantage |
| :--- | :--- | :--- | :--- |
| **Elapsed Wall-Clock Time** | **2.86 ms** | **0.42 ms (427 µs)** | **6.7× faster** |
| **Active Task-Clock (CPU time)**| **2.67 ms** | **0.29 ms (290 µs)** | **9.2× faster** |
| **CPU Cycles Burned** | **5,526,875** | **50,841** | **108.7× FEWER CYCLES** |
| **Instructions Executed** | **13,714,995** | **~60,000** | **Over 200× FEWER INSTRUCTIONS** |
| **Branches Taken** | **1,630,076** | **11,645** | **140× FEWER BRANCHES** |
| **Kernel Page Faults** | **327** | **44** | **7.4× fewer faults** |
| **Output Binary Size** | **4.8 KB (4,800 B)** | **929 bytes** | **5.2× smaller (pure static)** |

*(Against standard GCC, HT-Speed compiles in ~1.5% of GCC's build time and executes over 1,000× fewer instructions).*

> **Note on Scope:** TCC is measured compiling the equivalent C program (`test.c`); the comparison is apples-to-apples in program behavior (functions, loops, arithmetic, branches, and syscalls), not total language surface area.

---

## Architecture

HT-Speed operates as a single-pass streaming compiler split into 5 clean modules:

```text
       [ Source Code (.hts) ]
                 │
                 ▼
         ┌───────────────┐
         │    lexer.h    │  Token scanner, whitespace & comment skimmer
         └───────────────┘
                 │
                 ▼
         ┌───────────────┐
         │   parser.h    │  Pratt expression parser, statements, if/Loop
         └───────────────┘
                 │
                 ▼
         ┌───────────────┐
         │   emitter.h   │  Raw x86-64 opcodes, inline itoa, fixup table
         └───────────────┘
                 │
                 ▼
         ┌───────────────┐
         │    core.h     │  Zero-libc syscalls, string helpers, ELF structs
         └───────────────┘
                 │
                 ▼
    [ 120-Byte ELF Header + Opcode Buffer ] ──► Stamped directly to disk
```

---

## Building HT-Speed

### Option 1: Build with CIB (Recommended)

Compiling HT-Speed with [cib](https://github.com/TheMaster1127/C-is-bloated) strips all glibc baggage from the compiler itself, yielding an **13 KB static compiler binary**:

```bash
# Compile with CIB balanced optimization tier (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: ~12K htspeed_cib
```

### Option 2: Build with GCC

You can also build HT-Speed using standard GCC (zero libraries needed):

```bash
gcc -O3 htspeed_cib.c -o htspeed
```

---

## Language Reference

### Program Structure

HT-Speed programs consist of zero or more **`func`** declarations followed by a mandatory **`main`** label.

```htvm
func int add(int a, int b) {
    return a + b
}

main
int result := add(10, 20)
exit(0)
```

* **No semicolons:** Statements are terminated by newlines or statement boundaries.
* **The `main` label:** Maps directly to physical entry offset `0x400078` in the ELF header. Execution begins immediately at the first statement beneath `main`.
* **Auto-Exit:** If `main` reaches the end of the file without an explicit `exit()`, the compiler automatically inserts an exit syscall with status `0`.
* **Comments:** Supports `// single-line`, `# single-line`, and `/* multi-line */`.

---

### Types & Literals

All primitives map directly to 64-bit hardware registers or stack slots:

| Type | Size | Internal Representation | Description |
| :--- | :---: | :---: | :--- |
| **`int`** | 8 bytes | 64-bit signed integer | Decimal (`42`, `-10`) and Hexadecimal (`0x2A`) |
| **`str`** | 8 bytes | 64-bit pointer | Pointer to an ASCII string in the embedded data pool |
| **`bool`**| 8 bytes | 64-bit integer (`1` or `0`) | Boolean values |
| **`void`**| 0 bytes | None | Used for functions returning no value |

---

### Variables & Assignment (`:=`)

* **Declaration:** Variables are declared with a type prefix and the **`:=`** assignment operator:
  ```htvm
  int count := 5
  str greeting := "Hello!\n"
  bool active := 1
  ```
* **Re-assignment:** Updating an existing variable uses `:=` without the type keyword:
  ```htvm
  count := count + 1
  ```
* **Storage:** Every variable is assigned an 8-byte slot relative to `rbp` (`[rbp - 8]`, `[rbp - 16]`, etc.). Up to 256 locals per scope.

---

### Operators & The Equality Rule

HT-Speed completely eliminates the C `=`/`==` footgun:
* **`:=` is ALWAYS assignment:** `x := 10`
* **`=` is ALWAYS equality comparison:** `if (x = 10)`

#### Operator Precedence

| Tier | Operators | Operation | Emitted Instructions |
| :---: | :---: | :--- | :--- |
| **3 (Highest)** | `*`, `/`, `%` | Multiply, Divide, Modulo | `imul`, `idiv`, `idiv` (remainder in RDX) |
| **2** | `+`, `-` | Add, Subtract | `add`, `sub` |
| **1 (Lowest)** | `=`, `!=`, `<`, `<=`, `>`, `>=` | Comparisons | `cmp` + `setcc` $\rightarrow$ `movzx` |

Parentheses `()` can be used to arbitrarily group expressions: `int val := (a + b) * (c - d)`.

---

### Control Flow (`if` / `else`)

Evaluates any condition expression. `0` is false; non-zero is true.

```htvm
if (answer = 42) {
    print("Correct!\n")
} else {
    print("Wrong value!\n")
}
```

Displacements for both branches are automatically resolved via a single-pass backpatching table.

---

### Loops (`Loop, count` and `A_Index`)

Inspired by AutoHotKey, HT-Speed supports simple, hardware-native counting loops:

```htvm
int iterations := 5

Loop, iterations {
    // A_Index starts at 0 and increments every iteration
    if (A_Index = 2) {
        print("Hit index 2!\n")
    }
}
```

* **Dynamic Limits:** The loop count can be an immediate integer (`Loop, 10`) or an expression/variable (`Loop, count * 2`).
* **`A_Index`:** Built-in keyword representing the current 0-based iteration index. Can be used inside math, conditions, and print calls.
* **Nesting:** Supported up to 16 levels deep. `A_Index` automatically references the innermost active loop counter.

---

### Printing

HT-Speed provides a zero-libc, dynamic **`print()`** built-in that handles both string literals and numeric expressions:

1. **Printing Strings:**
   ```htvm
   print("Hello from HT-Speed!\n")
   ```
   Emits `sys_write(1, rip_relative_ptr, length)`. Duplicate string literals are automatically deduplicated in the ELF data pool.

2. **Printing Dynamic Numbers (Inline `itoa`):**
   ```htvm
   print(A_Index)        // Prints 0, 1, 2, ...
   print(fib(10))        // Prints 55
   print(10 - 50)        // Prints -40
   ```
   Emits a call to an internal, 111-byte hardware-division `itoa` routine that converts 64-bit signed integers (handling zero, positive, and negative numbers) and flushes them to `stdout` via `sys_write`.

---

### Raw Linux Syscalls

Because HT-Speed has no libc, the Linux kernel is your standard library. The **`syscall()`** primitive exposes the hardware kernel trap directly:

```htvm
syscall(number, arg1, arg2, arg3, arg4, arg5, arg6)
```

Register mapping adheres strictly to the Linux x86-64 syscall ABI:
* `number` $\rightarrow$ `RAX`
* `arg1` $\rightarrow$ `RDI`
* `arg2` $\rightarrow$ `RSI`
* `arg3` $\rightarrow$ `RDX`
* `arg4` $\rightarrow$ `R10` *(Kernel uses R10 instead of RCX)*
* `arg5` $\rightarrow$ `R8`
* `arg6` $\rightarrow$ `R9`

Returns the kernel's result code in `RAX`.

---

## Complete Examples

### 1. Hello World (159-byte Binary)

Save as `hello.hts`:

```htvm
main
print("Hello, World!\n")
exit(0)
```

Compile and run:
```bash
./htspeed_cib hello.hts hello
./hello
# Output: Hello, World!
```

---

### 2. Recursive Fibonacci with Loops & Number Printing (686-byte Binary)

Save as `fib.hts`:

```htvm
func int fib(int n) {
    if (n <= 1) {
        return n
    }
    return fib(n - 1) + fib(n - 2)
}

main
print("Loop counter A_Index:\n")
Loop, 5 {
    print(A_Index)
}

print("Computing fib(10):\n")
int result := fib(10)
print(result)

print("Negative number test:\n")
int neg := 10 - 50
print(neg)

exit(0)
```

Compile and run:
```bash
./htspeed_cib fib.hts fib_app
./fib_app
```

Output:
```text
Loop counter A_Index:
0
1
2
3
4
Computing fib(10):
55
Negative number test:
-40
```

---

### 3. Direct Syscall File Creation (Zero Libc File I/O)

Save as `file_write.hts`:

```htvm
main
// sys_open("output.txt", O_CREAT|O_WRONLY|O_TRUNC=577, 0755)
int fd := syscall(2, "output.txt", 577, 493)

// sys_write(fd, "Raw Linux Syscall Output!\n", 26)
syscall(1, fd, "Raw Linux Syscall Output!\n", 26)

// sys_close(fd)
syscall(3, fd)

print("File 'output.txt' written successfully!\n")
exit(0)
```

---

## The 120-Byte ELF Layout

HT-Speed outputs a minimal, compliant Linux ELF64 binary using a **single RWX segment** (the `cib` layout):

```text
┌────────────────────────────────────────────────────────┐
│ Elf64_Ehdr (64 bytes)                                  │ ◄── e_entry: 0x400078
├────────────────────────────────────────────────────────┤
│ Elf64_Phdr (56 bytes, PT_LOAD, PF_R | PF_W | PF_X)     │ ◄── Maps file to 0x400000
├────────────────────────────────────────────────────────┤
│ Machine Code (Direct x86-64 opcodes)                  │ ◄── Executes immediately
├────────────────────────────────────────────────────────┤
│ Embedded Data Pool (String literals)                  │ ◄── RIP-relative addressing
└────────────────────────────────────────────────────────┘
```

* **Header Size:** Exactly 120 bytes ($64 + 56$).
* **Section Headers:** 0 (chopped off for size).
* **Dynamic Libraries:** 0. `ldd` reports `not a dynamic executable`.

---

## Current Limits

HT-Speed v0.2 is designed for maximum compilation throughput and minimal binary footprint. As such, several enterprise language features are deliberately omitted:

1. **Integer-Only Math:** All numbers are 64-bit signed integers. Floating-point types (`float`, `double`) and SSE/AVX registers are not currently emitted.
2. **No Heap/Garbage Collector:** Memory storage consists of stack frames (`[rbp - off]`) and read-only string pool data. Dynamic allocations must use `syscall(12, ...)` (`sys_brk`) or `syscall(9, ...)` (`sys_mmap`).
3. **No `break` or `continue`:** Loops run for their designated iteration count.
4. **Target Platform:** Hardcoded to Linux x86-64.

---

## Author & Ecosystem

Created by **TheMaster1127** (aka *Mr. Compiler*), a low-level programmer, reverse engineer, and language designer.

* **GitHub:** [@TheMaster1127](https://github.com/TheMaster1127)
* **Related Projects:**
  * [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated) — Strips C binaries down to 169 bytes.
  * [binpatch](https://github.com/TheMaster1127/binpatch) — Binary patching and analysis tool.
  * [HT-RE](https://github.com/TheMaster1127/HT-RE) — Reverse engineering suite for Linux.

---

## License

This project is open-source under the **GNU General Public License v3.0 (GPLv3)**.
