# HT-Speed Language Reference Manual

This is the complete, official language specification for HT-Speed.

---

## Table of Contents
1. [Program Structure & Execution Flow](#1-program-structure--execution-flow)
2. [Comments](#2-comments)
3. [Types & Memory Model](#3-types--memory-model)
4. [Variables & Assignment Rules (`:=` vs `=`)](#4-variables--assignment-rules--vs-)
5. [Functions & Calling Conventions](#5-functions--calling-conventions)
6. [Structs & Automatic Allocation (`new`)](#6-structs--automatic-allocation-new)
7. [Operators & Precedence](#7-operators--precedence)
8. [Control Flow (`if` / `else`)](#8-control-flow-if--else)
9. [Loops (`Loop, count` & `while`)](#9-loops-loop-count--while)
10. [Dynamic Memory & Pointer Dereferencing](#10-dynamic-memory--pointer-dereferencing)
11. [Printing Built-in (`print`)](#11-printing-built-in-print)
12. [Command-Line Arguments (`GetParams`)](#12-command-line-arguments-getparams)
13. [Kernel Syscalls (`syscall`)](#13-kernel-syscalls-syscall)
14. [File Inclusion (`include`)](#14-file-inclusion-include)

---

## 1. Program Structure & Execution Flow

Programs consist of top-level definitions followed by a mandatory `main` entry point label.

```htvm
; Top-level definitions go here (structs, globals, functions)
func int multiply(int a, int b) {
    return a * b
}

; The main label maps directly to 0x400078 in the ELF header
main
int result := multiply(6, 7)
print(result)
exit(0)
```

* **No Statement Semicolons:** Statements are delimited by newlines or closing braces.
* **Top-Level Ordering:** All `struct`, global variables, and `func` declarations must appear before `main`.
* **Auto-Exit:** If an explicit `exit(code)` is not called, HT-Speed automatically appends `sys_exit(0)`.

---

## 2. Comments

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

## 3. Types & Memory Model

All data primitives operate as 64-bit machine words:

| Type | Word Size | Internal Representation | Description |
| :--- | :---: | :--- | :--- |
| **`int`** | 8 bytes | 64-bit signed integer | Decimal (`42`, `-10`) or Hexadecimal (`0xFF`) |
| **`str`** | 8 bytes | 64-bit memory pointer | Pointer to null-terminated ASCII string in pool/heap |
| **`bool`**| 8 bytes | 64-bit integer (`1` or `0`) | Boolean truth values |
| **`byte`**| 1 byte  | 8-bit unsigned integer | Used for raw byte memory access (`byte[ptr]`) |
| **`void`**| 0 bytes | None | Used for functions that return no value |

---

## 4. Variables & Assignment Rules (`:=` vs `=`)

To eliminate math footguns, assignment and equality testing are strictly separated:

* **`:=` is ALWAYS assignment:**
  ```htvm
  int a := 10
  a := a + 5
  ```
* **`=` is ALWAYS equality comparison:**
  ```htvm
  if (a = 15) {
      print("Matches!\n")
  }
  ```

### Local Variables
Declared inside functions or `main` relative to `[rbp - offset]`:
```htvm
int counter := 0
str name := "HT-Speed\n"
bool ready := 1
```

### Top-Level Global Variables
Declared at file scope before `main` with constant initializers. Globals live in the ELF data pool and are accessed via RIP-relative displacement:
```htvm
int g_counter := 500
str g_title := "Bare-Metal Engine\n"

main
print(g_title)
print(g_counter)
exit(0)
```

---

## 5. Functions & Calling Conventions

Functions can return values or `void`. They can be called inside expressions or as standalone statements.

```htvm
func void notify() {
    print("Action executed!\n")
}

func int sum(int a, int b) {
    return a + b
}

main
; 1. Standalone function call statement
notify()

; 2. Expression assignment
int total := sum(10, 20)
print(total)
exit(0)
```

* **ABI Mapping:** Up to 6 arguments are passed via standard System V registers: `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`.
* **Return Value:** Passed back in `rax`.
* **Forward References:** Functions can be called before they are defined; fixup relocations are patched automatically.

---

## 6. Structs & Automatic Allocation (`new`)

Structs define custom 64-bit word memory layouts using curly braces `{ ... }`. Every field receives an 8-byte offset:

```htvm
struct Player {
    int health
    int mana
    int score
}

main
; 'new' queries the total struct size and allocates via sys_mmap
int p := new Player

p.health := 100
p.mana := 50
p.score := 1337

print(p.health)
print(p.score)
exit(0)
```

---

## 7. Operators & Precedence

HT-Speed implements an 11-level Pratt precedence engine:

| Level | Operators | Description | Machine Instruction |
| :---: | :--- | :--- | :--- |
| **11** | `*`, `/`, `%` | Multiplication, Division, Modulo | `imul`, `idiv` |
| **10** | `+`, `-` | Addition, Subtraction | `add`, `sub` |
| **9** | `<<`, `>>` | Bitwise Shift Left, Arithmetic Shift Right | `shl`, `sar` |
| **8** | `<`, `<=`, `>`, `>=` | Relational Comparisons | `cmp` + `setcc` |
| **7** | `=`, `!=` | Equality, Inequality | `cmp` + `sete` / `setne` |
| **6** | `&` | Bitwise AND | `and` |
| **5** | `^` | Bitwise XOR | `xor` |
| **4** | `\|` | Bitwise OR | `or` |
| **3** | `.` | Dynamic String Concatenation | Inlined `mmap` runtime |
| **2** | `and`, `&&` | Logical AND | Boolean normalization |
| **1** | `or`, `\|\|` | Logical OR | Boolean normalization |

Unary bitwise NOT (`~`) inverts all bits: `int mask := ~0`. Parentheses `(...)` override precedence arbitrarily.

### String Concatenation (`.`)
The `.` operator dynamically allocates a buffer via `sys_mmap`, copies both strings, null-terminates, and returns the pointer:
```htvm
main
str first := "Ultra "
str second := "Fast!\n"
str combined := first . second
print(combined)
exit(0)
```

---

## 8. Control Flow (`if` / `else`)

Condition expressions do not require wrapping outer parentheses:

```htvm
main
int a := 10
int b := 20

if (a = 10) or (b = 999) {
    print("Or matched!\n")
}

if (a = 10) and (b = 20) {
    print("And matched!\n")
} else {
    print("Else branch!\n")
}
exit(0)
```

---

## 9. Loops (`Loop, count` & `while`)

Both loop constructs support **`break`** and **`continue`**:

### AutoHotKey-Style Counted Loop (`Loop, count`)
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
* `A_Index` is the built-in 0-indexed loop counter, isolated up to 16 levels of nesting.

### Conditional While Loop (`while`)
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

## 10. Dynamic Memory & Pointer Dereferencing

HT-Speed manipulates raw memory without libc:

### 1. Heap Allocation (`alloc`)
Requests page-aligned memory from the Linux kernel using `sys_mmap`:
```htvm
main
int buf := alloc(64)
[buf] := 1337
print([buf])
exit(0)
```

### 2. 64-Bit Memory Dereference (`[ptr + offset]`)
* Store 8 bytes: `[buf + 8] := 42`
* Load 8 bytes: `int x := [buf + 8]`

### 3. 8-Bit Byte Dereference (`byte[ptr + offset]`)
* Store 1 byte: `byte[buf + 0] := 65` ; 'A'
* Load 1 byte: `int c := byte[buf + 0]`

---

## 11. Printing Built-in (`print`)

The `print()` built-in dynamically adapts:
1. **String Literal:** `print("Literal\n")` emits direct RIP-relative `sys_write`.
2. **Signed Integer:** `print(10 - 50)` calls an internal inlined 111-byte hardware `itoa` routine.
3. **Memory Buffer:** `print(buffer_ptr, count)` flushes exact byte counts to `stdout`.
4. **String Variable:** `print(str_var)` prints null-terminated strings automatically.

---

## 12. Command-Line Arguments (`GetParams`)

Returns all command-line arguments separated by newlines:

```htvm
main
str params := GetParams()
print(params)
exit(0)
```

---

## 13. Kernel Syscalls (`syscall`)

Invoke hardware syscall traps directly as statements or expressions:

```htvm
main
int buf := alloc(64)

; Expression: captures kernel return value in RAX
int bytes_read := syscall(0, 0, buf, 64)

; Statement: sys_write(stdout=1, buf, bytes_read)
syscall(1, 1, buf, bytes_read)
exit(0)
```

* ABI: `RAX` = number, `RDI` = a1, `RSI` = a2, `RDX` = a3, `R10` = a4, `R8` = a5, `R9` = a6.

---

## 14. File Inclusion (`include`)

Slices external files recursively at compile-time:

```htvm
include "constants.hts"
include "engine/player.hts"
```

