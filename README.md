# HT-Speed

**The Sub-Millisecond, Zero-Libc x86-64 Native Compiler (v0.3)**

HT-Speed is an ultra-minimalist, single-pass compiler that translates a clean, human-friendly systems language directly into standalone Linux x86-64 ELF executables without intermediate assembly text, without libc, and without external linkers.

Built by [TheMaster1127](https://github.com/TheMaster1127) using [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated).

---

## Table of Contents
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [Benchmarks](#benchmarks)
- [Architecture & Modular Split](#architecture--modular-split)
- [Building HT-Speed](#building-ht-speed)
  - [Option 1: Build with CIB (Recommended — 16 KB Static Binary)](#option-1-build-with-cib-recommended)
  - [Option 2: Build with GCC](#option-2-build-with-gcc)
- [Language Reference (v0.3)](#language-reference-v03)
  - [Program Structure (`func` and `main`)](#program-structure)
  - [Types & Memory Model](#types--memory-model)
  - [Variables & Assignment (`:=`)](#variables--assignment-)
  - [The Equality Rule (`=` vs `:=`) & Operator Precedence](#the-equality-rule--operator-precedence)
  - [Control Flow (`if` / `else`)](#control-flow-if--else)
  - [Loops (`Loop, count`, `while`, `A_Index`, `break`, `continue`)](#loops)
  - [Dynamic Memory & Dereferencing (`alloc`, `[ptr]`, `byte[ptr]`)](#dynamic-memory--dereferencing)
  - [Memory Deallocation (`free` / `sys_munmap`)](#memory-deallocation)
  - [Printing (`print` with Strings, Numbers, and Buffers)](#printing)
  - [First-Class Syscalls (`syscall`)](#first-class-syscalls)
- [Complete Examples](#complete-examples)
  - [1. Minimal Hello World (181 bytes)](#1-minimal-hello-world-181-bytes)
  - [2. Recursive Fibonacci & Dynamic itoa (686 bytes)](#2-recursive-fibonacci--dynamic-itoa-686-bytes)
  - [3. Dynamic Heap Arrays & Byte-by-Byte Strings (1.1 KB)](#3-dynamic-heap-arrays--byte-by-byte-strings-11-kb)
  - [4. Interactive Terminal Echo (Exact Byte Count)](#4-interactive-terminal-echo-exact-byte-count)
  - [5. Heap Allocation & Explicit Freeing (`sys_munmap`)](#5-heap-allocation--explicit-freeing-sys_munmap)
- [The 120-Byte ELF Layout](#the-120-byte-elf-layout)
- [Current Limits](#current-limits)
- [Author & Ecosystem](#author--ecosystem)
- [License](#license)

---

## The Problem

Modern compilers are trapped in abstraction bloat:
* **Compilation Latency:** Compiling a simple program in GCC or Clang takes 30–50 ms and burns 80–120 million CPU instructions.
* **Binary Bloat:** Standard toolchains link hundreds of kilobytes of C runtime boilerplate (`crt1.o`, dynamic linkers, libc tables).
* **Intermediate Representation Tax:** Compilers serialize ASTs into intermediate assembly text (`.s`), write it to disk, and then spawn an assembler subprocess to parse it back into binary opcodes.

---

## The Solution

HT-Speed eliminates the entire pipeline:
* **Zero Assembly Text:** Translates source tokens directly into raw x86-64 machine code bytes in memory.
* **Zero Libc Runtime:** Output binaries communicate directly with the Linux kernel via raw syscalls (`sys_write`, `sys_read`, `sys_mmap`, `sys_open`, `sys_exit`).
* **Microscopic Footprint:** The compiler itself is a **16 KB** static binary, compiles user programs in **440 microseconds**, and outputs standalone executables starting at **181 bytes**.

---

## Benchmarks

Measured on **Artix Linux x86-64**, physical silicon, averaged across **100 runs back-to-back** using Linux `perf stat -r 100`:

### Workload: Multi-Function Program with Math, Loops, Calls & Syscalls

| Metric | TinyCC (`tcc`) | HT-Speed v0.3 (`htspeed_cib`) | Advantage |
| :--- | :--- | :--- | :--- |
| **Elapsed Wall-Clock Time** | **2.87 ms** | **0.44 ms (441 µs)** | **6.5× faster** |
| **Active Task-Clock (CPU time)**| **2.67 ms** | **0.30 ms (300 µs)** | **8.9× faster** |
| **CPU Cycles Burned** | **5,526,875** | **58,910** | **93.8× FEWER CYCLES** |
| **Instructions Executed** | **13,714,995** | **~70,000** | **Over 190× FEWER INSTRUCTIONS** |
| **Branches Taken** | **1,630,076** | **11,351** | **143.6× FEWER BRANCHES** |
| **Kernel Page Faults** | **327** | **51** | **6.4× fewer faults** |
| **Output Binary Size** | **4.8 KB (4,800 B)** | **929 bytes** | **5.2× smaller (pure static)** |

*(Against standard GCC, HT-Speed compiles in ~1.5% of GCC's build time and executes over 1,000× fewer instructions).*

> **Note on Scope:** TCC is measured compiling the equivalent C program (`test.c`); the comparison is apples-to-apples in program behavior (functions, loops, arithmetic, branches, and syscalls), not total language surface area.

---

## Architecture & Modular Split

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
         │   parser.h    │  Pratt expression parser, statements, while/Loop
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

Compiling HT-Speed with [cib](https://github.com/TheMaster1127/C-is-bloated) strips all glibc baggage from the compiler itself, yielding a **16 KB static compiler binary**:

```bash
# Compile with CIB balanced optimization tier (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: ~16K htspeed_cib
```

### Option 2: Build with GCC

You can also build HT-Speed using standard GCC (zero external libraries needed):

```bash
gcc -O3 htspeed_cib.c -o htspeed
```

---

## Language Reference (v0.3)

### Program Structure

Programs consist of zero or more **`func`** definitions followed by the mandatory **`main`** label.

```htvm
func int multiply(int a, int b) {
    return a * b
}

main
int result := multiply(6, 7)
exit(0)
```

* **No semicolons:** Statements are delimited by newlines or statement boundaries.
* **The `main` label:** Maps directly to physical entry offset `0x400078` in the ELF header. Execution begins immediately at the first statement beneath `main`.
* **Functions before `main`:** All functions must be defined before `main`.
* **Dead-Code Elimination:** If the program does not print dynamic numbers, the internal 111-byte `itoa` routine is completely omitted from the binary. If `exit()` was explicitly called, the automatic fallback exit is suppressed.
* **Comments:** Supports `// single-line`, `# single-line`, and `/* multi-line */`.

---

### Types & Memory Model

All data primitives map directly to 64-bit hardware registers or stack slots:

| Type | Size | Internal Representation | Description |
| :--- | :---: | :---: | :--- |
| **`int`** | 8 bytes | 64-bit signed integer | Decimal (`42`, `-10`) and Hexadecimal (`0x2A`) |
| **`str`** | 8 bytes | 64-bit pointer | Pointer to an ASCII string in the data pool |
| **`bool`**| 8 bytes | 64-bit integer (`1` or `0`) | Boolean values |
| **`void`**| 0 bytes | None | Used for functions returning no value |
| **`byte`**| 1 byte  | 8-bit unsigned value | Used for raw byte memory access: `byte[ptr]` |

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
* **Storage:** Variables live on the stack relative to `rbp` (`[rbp - 8]`, `[rbp - 16]`). Up to 256 locals per function scope.

---

### The Equality Rule (`=` vs `:=`) & Operator Precedence

HT-Speed eliminates C’s assignment/comparison footgun:
* **`:=` is ALWAYS assignment:** `x := 10`
* **`=` is ALWAYS equality comparison:** `if (x = 10)`

#### Precedence Table

| Precedence | Operators | Operation | Machine Code Emitted |
| :---: | :---: | :--- | :--- |
| **3 (Highest)** | `*`, `/`, `%` | Multiply, Divide, Modulo | `imul`, `idiv`, `idiv` (remainder in RDX) |
| **2** | `+`, `-` | Add, Subtract | `add`, `sub` |
| **1 (Lowest)** | `=`, `!=`, `<`, `<=`, `>`, `>=` | Comparisons | `cmp` + `setcc` $\rightarrow$ `movzx` |

Parentheses `()` can arbitrarily group expressions: `int val := (a + b) * (c - d)`.

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

Branches support full nesting and single-pass relative displacement backpatching.

---

### Loops

HT-Speed provides two loop constructs, both supporting **`break`** and **`continue`**:

#### 1. AutoHotKey-Style Counted Loop (`Loop, count`)
```htvm
Loop, 5 {
    if (A_Index = 2) {
        continue // Skip to next iteration
    }
    if (A_Index = 4) {
        break    // Exit loop
    }
    print(A_Index)
}
```
* **Dynamic Limits:** Count can be an immediate integer (`Loop, 10`) or an expression (`Loop, count * 2`).
* **`A_Index`:** Built-in keyword representing the current 0-based iteration index.
* **Nesting:** Supported up to 16 levels deep. `A_Index` always references the current innermost active loop counter.

#### 2. Conditional While Loop (`while`)
```htvm
int i := 0
while (i < 10) {
    i := i + 1
    print(i)
}
```

---

### Dynamic Memory & Dereferencing

HT-Speed provides bare-metal memory manipulation without libc `malloc`:

#### 1. Heap Allocation (`alloc`)
Requests page-aligned memory directly from the Linux kernel using `sys_mmap`:
```htvm
int ptr := alloc(1024) // Allocates 1024 bytes on the heap
```

#### 2. 64-Bit Memory Dereference (`[ptr]`)
* **Store 8 bytes:** `[ptr + offset] := 1337`
* **Load 8 bytes:** `int val := [ptr + offset]`

#### 3. 8-Bit Byte Dereference (`byte[ptr]`)
* **Store 1 byte:** `byte[ptr + offset] := 65`  // 'A'
* **Load 1 byte:** `int c := byte[ptr + offset]`
* **Silent Truncation Rule:** `byte[ptr] := value` stores only the lowest 8 bits of the value (`value & 0xFF`). Values exceeding 255 are truncated silently without warning, matching standard x86 `mov byte ptr` behavior.

#### 4. Dynamic Arrays
Arrays are contiguous memory blocks. Index $i$ of a 64-bit integer array sits at offset `i * 8`:
```htvm
int arr := alloc(800) // Array of 100 integers

// arr[i] := 42
[arr + i * 8] := 42

// val := arr[i]
int val := [arr + i * 8]
```

---

### Memory Deallocation

Memory allocated with `alloc()` can be returned to the Linux kernel using **`sys_munmap` (Syscall 11)**:

```htvm
func void free(int ptr, int size) {
    syscall(11, ptr, size)
}
```

* **Address Alignment Requirement:** The Linux kernel strictly requires `ptr` to be page-aligned (a multiple of 4,096 bytes). Because `alloc()` calls `mmap()`, all base pointers returned by `alloc()` are guaranteed to be page-aligned. Attempting to free an unaligned address inside a page (e.g., `free(ptr + 16, 100)`) will fail with `EINVAL`.
* **Length Rounding:** The kernel automatically rounds `size` up to the nearest page boundary (`PAGE_ALIGN(size)`). Freeing 100 bytes from a base pointer unmaps the entire 4,096-byte page containing those bytes.

---

### Printing

HT-Speed provides an adaptive, zero-libc **`print()`** built-in:

1. **Printing String Literals:**
   ```htvm
   print("Hello, World!\n")
   ```
   Emits `sys_write(1, rip_rel_ptr, length)`. Identical string literals are deduplicated in the ELF data pool.

2. **Printing Dynamic Numbers (Inline `itoa`):**
   ```htvm
   print(A_Index)        // Prints 0, 1, 2, ...
   print(fib(10))        // Prints 55
   print(10 - 50)        // Prints -40
   ```
   Calls an internal 111-byte hardware-division `itoa` routine that converts 64-bit signed integers (zero, positive, negative) to ASCII and flushes them to `stdout`.

3. **Printing Raw Memory Buffers:**
   ```htvm
   print(buffer_ptr, byte_count)
   ```
   Flushes exactly `byte_count` bytes from memory to `stdout` without trailing spaces or buffer overflows.

---

### First-Class Syscalls

The **`syscall()`** primitive exposes the hardware kernel trap directly as both a statement and an expression:

```htvm
// Use as an expression (captures kernel return value in RAX!)
int bytes_read := syscall(0, 0, buffer, 64)

// Use as a statement
syscall(1, 1, buffer, bytes_read)
```

#### Register Mapping (Linux x86-64 ABI)
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

### 1. Minimal Hello World (181 bytes)

Save as `hello.hts`:
```htvm
main
print("Hello, World!\n")
exit(69)
```

Compile and inspect:
```bash
./htspeed_cib hello.hts hello
./hello
echo $?
# Output: 69

ls -lh hello
# Output: 181 bytes!
```

---

### 2. Recursive Fibonacci & Dynamic itoa (686 bytes)

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
ls -lh fib_app
# Output: 686 bytes!
```

---

### 3. Dynamic Heap Arrays & Byte-by-Byte Strings (1.1 KB)

Save as `arrays.hts`:
```htvm
main
print("=== 1. Dynamic Integer Array (Heap) ===\n")
int arr := alloc(80) // 10 integers
int count := 5

// Populate array: arr[i] = (i + 1) * 10
Loop, count {
    int val := (A_Index + 1) * 10
    [arr + A_Index * 8] := val
}

// Read back from heap
Loop, count {
    int item := [arr + A_Index * 8]
    print(item)
}

print("=== 2. Mutating Element at Index 2 ===\n")
[arr + 2 * 8] := 999
print([arr + 2 * 8])

print("=== 3. Dynamic Byte Array (String in RAM) ===\n")
int str_buf := alloc(32)
byte[str_buf + 0] := 72  // 'H'
byte[str_buf + 1] := 84  // 'T'
byte[str_buf + 2] := 83  // 'S'
byte[str_buf + 3] := 80  // 'P'
byte[str_buf + 4] := 69  // 'E'
byte[str_buf + 5] := 69  // 'E'
byte[str_buf + 6] := 68  // 'D'
byte[str_buf + 7] := 10  // '\n'

print(str_buf, 8)
exit(0)
```

---

### 4. Interactive Terminal Echo (Exact Byte Count)

Save as `echo.hts`:
```htvm
main
print("Type something and press ENTER:\n")

int mem := alloc(128)

// sys_read returns the exact number of bytes typed into 'typed_bytes'
int typed_bytes := syscall(0, 0, mem, 128)

print("You typed (exact bytes, zero trailing spaces):\n")
print(mem, typed_bytes)

print("Exact byte count:\n")
print(typed_bytes)

exit(0)
```

---

### 5. Heap Allocation & Explicit Freeing (`sys_munmap`)

Save as `free_test.hts`:
```htvm
func void free(int ptr, int size) {
    syscall(11, ptr, size) // sys_munmap
}

main
print("Allocating 4096 bytes...\n")
int buf := alloc(4096)

[buf] := 42
print("Value: \n")
print([buf])

print("Freeing back to Linux kernel...\n")
free(buf, 4096)
print("Memory returned successfully!\n")

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
* **Section Headers:** 0 (omitted for size).
* **Dynamic Libraries:** 0. `ldd` reports `not a dynamic executable`.

---

## Current Limits

HT-Speed v0.3 is designed for maximum compilation throughput and a minimal binary footprint:

1. **Integer-Only Math:** Arithmetic is performed on 64-bit signed integers. Floating-point registers (SSE/AVX) are not emitted.
2. **Platform Specific:** Hardcoded to emit Linux x86-64 syscalls and ELF64 headers.

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
