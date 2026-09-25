# HT-Speed

**The Sub-Millisecond, Zero-Libc x86-64 Native Compiler (v0.4)**

HT-Speed is an ultra-minimalist, single-pass compiler that translates a clean, human-friendly systems language directly into standalone Linux x86-64 ELF executables without intermediate assembly text, without libc, and without external linkers.

Built by [TheMaster1127](https://github.com/TheMaster1127) using [cib (C-Is-Bloated)](https://github.com/TheMaster1127/C-is-bloated).

---

## Table of Contents
- [The Problem](#the-problem)
- [The Solution](#the-solution)
- [Benchmarks](#benchmarks)
- [Architecture & Modular Split](#architecture--modular-split)
- [Building HT-Speed](#building-ht-speed)
  - [Build with CIB (21 KB Static Binary)](#build-with-cib)
- [Language Reference](#language-reference)
  - [Program Structure (`func` and `main`)](#program-structure)
  - [File Inclusion (`include`)](#file-inclusion-include)
  - [Types & Memory Model](#types--memory-model)
  - [Variables: Local (`:=`) and Global](#variables-local-and-global)
  - [Structs & OSP Syntax (`struct` and `subout`)](#structs--osp-syntax)
  - [Operators & Precedence](#operators--precedence)
    - [Arithmetic & Bitwise](#arithmetic--bitwise)
    - [The Equality Rule (`=` vs `:=`)](#the-equality-rule--vs-)
    - [Logical Operators & Flexible Conditions (`and`, `or`)](#logical-operators--flexible-conditions)
    - [String Concatenation (`.`)](#string-concatenation-)
  - [Control Flow (`if` / `else`)](#control-flow-if--else)
  - [Loops (`Loop, count`, `while`, `A_Index`, `break`, `continue`)](#loops)
  - [Dynamic Heap Memory & Dereferencing (`alloc`, `[ptr]`, `byte[ptr]`)](#dynamic-heap-memory--dereferencing)
  - [Memory Deallocation (`sys_munmap`)](#memory-deallocation)
  - [Printing (`print` with Strings, Numbers, and Buffers)](#printing)
  - [Command-Line Arguments (`GetParams`)](#command-line-arguments-getparams)
  - [First-Class Kernel Syscalls (`syscall`)](#first-class-kernel-syscalls)
- [Complete Examples](#complete-examples)
  - [1. Minimal Hello World (198 bytes)](#1-minimal-hello-world-198-bytes)
  - [2. Recursive Fibonacci & Dynamic itoa (686 bytes)](#2-recursive-fibonacci--dynamic-itoa-686-bytes)
  - [3. Structs, Bitwise & Command-Line Arguments](#3-structs-bitwise--command-line-arguments)
  - [4. Dynamic Heap Arrays & Byte-by-Byte Strings](#4-dynamic-heap-arrays--byte-by-byte-strings)
  - [5. Interactive Terminal Echo](#5-interactive-terminal-echo)
  - [6. Heap Allocation & Explicit Freeing (`sys_munmap`)](#6-heap-allocation--explicit-freeing-sys_munmap)
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
* **Microscopic Footprint:** The compiler itself is a **21 KB** static binary, compiles user programs in **~500 microseconds**, and outputs standalone executables starting at **198 bytes**.

---

## Benchmarks

Measured on **Artix Linux x86-64**, physical silicon, averaged across **100 runs back-to-back** using Linux `perf stat -r 100`:

### Workload: Multi-Function Program (`test.hts` vs `test.c`)

```text
Functions, nested calls, loops, variables, comparisons, arithmetic, and syscalls
```

| Metric | TinyCC (`tcc`) | HT-Speed (`htspeed_cib`) | Advantage |
| :--- | :--- | :--- | :--- |
| **Elapsed Wall-Clock Time** | **2.90 ms** | **0.55 ms (554 µs)** | **5.2× faster** |
| **Active Task-Clock (CPU time)**| **2.70 ms** | **0.40 ms (400 µs)** | **6.75× faster** |
| **CPU Cycles Burned** | **5,303,993** | **116,973** | **45.3× FEWER CYCLES** |
| **Instructions Executed** | **13,494,201** | **~80,000** | **Over 160× FEWER INSTRUCTIONS** |
| **Branches Taken** | **1,523,718** | **19,831** | **76.8× FEWER BRANCHES** |
| **Kernel Page Faults** | **327** | **121** | **2.7× fewer faults** |
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

### Build with CIB

Compiling HT-Speed with [cib](https://github.com/TheMaster1127/C-is-bloated) strips all glibc baggage from the compiler itself, yielding a **21 KB static compiler binary**:

```bash
# Compile with CIB balanced optimization tier (-Z4)
cib htspeed_cib.c -Z4

# Verify the compiler is completely standalone
file htspeed_cib
# Output: ELF 64-bit LSB executable, x86-64, statically linked, no section header

ls -lh htspeed_cib
# Output: ~21K htspeed_cib
```

> **Note on -Z flag:** Do NOT use the `-Z5` flag. It causes a segmentation fault triggered by GCC's `-O3` optimization tier in the background. Anything else is valid. Always use either `-Z4` for maximum compilation speed of your compiler, or `-Z0` (the default) for the smallest compiler binary size. Before blaming me for any issues: this is not my fault, it is 100% GCC's fault. Even the Linux kernel refuses to compile with `-O3` because it breaks when you push C to the bare metal.

---

## Language Reference

### Program Structure

Programs consist of optional `include` directives, struct definitions, top-level global variables, function definitions, and a mandatory **`main`** entry point.

```htvm
include "math_utils.hts"

int global_counter := 0

struct Player
    int health
    int mana
subout

func int multiply(int a, int b) {
    return a * b
}

main
int result := multiply(6, 7)
exit(0)
```

* **No semicolons:** Statements are delimited by newlines or statement boundaries.
* **The `main` label:** Maps directly to physical entry offset `0x400078` in the ELF header. Execution begins immediately at the first statement beneath `main`.
* **Definitions before `main`:** All structs, globals, and functions must be defined before `main`.
* **Dead-Code Elimination:** If the program does not print dynamic numbers, the internal 111-byte `itoa` routine is completely omitted from the binary. If `exit()` was explicitly called, the automatic fallback exit is suppressed.
* **Comments:** Supports `// single-line`, `# single-line`, and `/* multi-line */`.

---

### File Inclusion (`include`)

Split your codebase across multiple files using the `include` directive:

```htvm
include "constants.hts"
include "modules/player.hts"
```

* Files are recursively loaded and spliced directly into memory at compile time before parsing begins.

---

### Types & Memory Model

All data primitives map directly to 64-bit hardware registers or stack slots:

| Type | Size | Internal Representation | Description |
| :--- | :---: | :---: | :--- |
| **`int`** | 8 bytes | 64-bit signed integer | Decimal (`42`, `-10`) and Hexadecimal (`0x2A`) |
| **`str`** | 8 bytes | 64-bit pointer | Pointer to an ASCII string in the data pool or heap |
| **`bool`**| 8 bytes | 64-bit integer (`1` or `0`) | Boolean values |
| **`void`**| 0 bytes | None | Used for functions returning no value |
| **`byte`**| 1 byte  | 8-bit unsigned value | Used for raw byte memory access: `byte[ptr]` |

---

### Variables: Local and Global

#### Local Variables (`:=`)
Declared inside functions or `main` using the **`:=`** assignment operator:
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

#### Top-Level Global Variables
Declared at the file level outside functions:
```htvm
int global_base := 100
str global_title := "HT-Speed Engine\n"
```
* Stored in the writable ELF data pool. Accessed natively using position-independent `[rip + disp32]` addressing.

---

### Structs & OSP Syntax

Define custom structured data layouts using either OSP syntax (`subout`) or standard curly braces (`{}`):

#### 1. OSP Style (Ordinal Struct Programming)
```htvm
struct Player
    int health
    int mana
subout
```

#### 2. Brace Style
```htvm
struct Point {
    int x
    int y
}
```

#### Property Access
Each field occupies an 8-byte word offset. Access and mutate properties via dot syntax:
```htvm
int p := alloc(16)
p.health := 250
p.mana := 80

print(p.health) // Prints 250
```

---

### Operators & Precedence

#### Arithmetic & Bitwise
HT-Speed supports a complete set of 64-bit arithmetic and bitwise operators:

| Precedence | Operators | Operation | Machine Instruction |
| :---: | :---: | :--- | :--- |
| **11 (Highest)** | `*`, `/`, `%` | Multiply, Divide, Modulo | `imul`, `idiv`, `idiv` |
| **10** | `+`, `-` | Add, Subtract | `add`, `sub` |
| **9** | `<<`, `>>` | Shift Left, Arithmetic Shift Right | `shl`, `sar` |
| **8** | `<`, `<=`, `>`, `>=` | Relational Comparisons | `cmp` + `setcc` |
| **7** | `=`, `!=` | Equality / Inequality | `cmp` + `sete` / `setne` |
| **6** | `&` | Bitwise AND | `and` |
| **5** | `^` | Bitwise XOR | `xor` |
| **4** | `\|` | Bitwise OR | `or` |
| **3** | `.` | Dynamic String Concatenation | Runtime `__str_concat` |
| **2** | `and`, `&&` | Logical AND | Boolean normalization |
| **1 (Lowest)** | `or`, `\|\|` | Logical OR | Boolean normalization |

Unary bitwise NOT (`~`) inverts all bits: `int inverted := ~0`. Parentheses `()` override precedence arbitrarily.

#### The Equality Rule (`=` vs `:=`)
* **`:=` is ALWAYS assignment:** `x := 10`
* **`=` is ALWAYS equality comparison:** `if (x = 10)`

#### Logical Operators & Flexible Conditions
You can write natural conditions without forcing a single outer parenthetical wrapper:
```htvm
if (a = 45) or (b = 5) {
    print("Matched or!\n")
}

if (x = 10) and (y = 20) {
    print("Matched and!\n")
}
```
Both word keywords (`and`, `or`) and symbols (`&&`, `||`) are supported interchangeably.

#### String Concatenation (`.`)
Join strings dynamically with the `.` operator:
```htvm
str s1 := "Fast "
str s2 := "Compiler!\n"
str combined := s1 . s2
print(combined)
```
* Concatenation dynamically allocates a new heap buffer via `sys_mmap`, copies both strings, null-terminates, and returns the pointer. Supports arbitrary chaining: `a . b . c`.

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
Loop, 10 {
    if (A_Index = 3) {
        continue // Skip to next iteration
    }
    if (A_Index = 7) {
        break    // Exit loop
    }
    print(A_Index)
}
```
* **Dynamic Limits:** Count can be an immediate integer (`Loop, 10`) or an expression (`Loop, count * 2`).
* **`A_Index`:** Built-in keyword representing the current 0-based iteration index.
* **Nesting:** Supported up to 16 levels deep. `A_Index` always references the innermost active loop counter.

#### 2. Conditional While Loop (`while`)
```htvm
int i := 0
while (i < 10) {
    i := i + 1
    print(i)
}
```

---

### Dynamic Heap Memory & Dereferencing

Manipulate raw memory directly without libc `malloc`:

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

4. **Printing String Variables:**
   ```htvm
   str msg := "Dynamic message\n"
   print(msg)
   ```
   The compiler detects `str` variables and automatically prints the null-terminated string text.

---

### Command-Line Arguments (`GetParams`)

Access command-line arguments passed from the shell using the built-in **`GetParams()`** function:

```htvm
main
str params := GetParams()
print(params)
exit(0)
```

* Returns a single heap-allocated string containing all arguments (`argv[1]` through `argv[argc-1]`) separated by newlines (`\n`).
* If no arguments are passed, it returns an empty string `""`.

---

### First-Class Kernel Syscalls

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

### 1. Minimal Hello World (198 bytes)

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
# Output: 198 bytes!
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

### 3. Structs, Bitwise & Command-Line Arguments

Save as `demo.hts`:
```htvm
int global_base := 100

struct Player
    int health
    int mana
subout

main
// Structs
int p := alloc(16)
p.health := 250
p.mana := 80
print(p.health)

// Bitwise
print(1 << 5)        // 32
print(0xFF & 0x0F)   // 15

// Conditionals
if (p.health = 250) or (p.mana = 0) {
    print("Condition passed!\n")
}

// String Concat
str greeting := "Hello " . "World!\n"
print(greeting)

// Command Line Arguments
str args := GetParams()
print(args)

exit(0)
```

Compile and run with arguments:
```bash
./htspeed_cib demo.hts demo_app
./demo_app foo bar 1337
```

---

### 4. Dynamic Heap Arrays & Byte-by-Byte Strings

Save as `arrays.hts`:
```htvm
main
print("=== Dynamic Integer Array ===\n")
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

// Mutate element
[arr + 2 * 8] := 999
print([arr + 2 * 8])

print("=== Dynamic Byte String ===\n")
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

### 5. Interactive Terminal Echo

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

### 6. Heap Allocation & Explicit Freeing (`sys_munmap`)

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
│ Embedded Data Pool (String literals & Globals)         │ ◄── RIP-relative addressing
└────────────────────────────────────────────────────────┘
```

* **Header Size:** Exactly 120 bytes ($64 + 56$).
* **Section Headers:** 0 (omitted for size).
* **Dynamic Libraries:** 0. `ldd` reports `not a dynamic executable`.

---

## Current Limits

HT-Speed v0.4 is designed for maximum compilation throughput and a minimal binary footprint:

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
