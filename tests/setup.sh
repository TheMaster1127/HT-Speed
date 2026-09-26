#!/bin/bash
# setup.sh — creates the full test suite in ./tests/
# Run this from the project root: bash tests/setup.sh

set -e
cd "$(dirname "$0")"

# 01 - hello world
cat > 01_hello.hts << 'EOF'
main
print("Hello, World!\n")
exit(0)
EOF
printf 'Hello, World!\n' > 01_hello.expected

# 02 - simple function
cat > 02_function.hts << 'EOF'
func int add(int a, int b) { return a + b }
func int mul(int a, int b) { return a * b }
main
int x := add(3, 4)
int y := mul(x, 2)
print(y)
exit(0)
EOF
printf '14\n' > 02_function.expected

# 03 - recursive fibonacci
cat > 03_fib.hts << 'EOF'
func int fib(int n) {
    if (n <= 1) { return n }
    return fib(n - 1) + fib(n - 2)
}
main
print(fib(10))
exit(0)
EOF
printf '55\n' > 03_fib.expected

# 04 - Loop with A_Index
cat > 04_loop.hts << 'EOF'
main
Loop, 5 { print(A_Index) }
exit(0)
EOF
printf '0\n1\n2\n3\n4\n' > 04_loop.expected

# 05 - while loop
cat > 05_while.hts << 'EOF'
main
int i := 0
while (i < 5) {
    print(i)
    i := i + 1
}
exit(0)
EOF
printf '0\n1\n2\n3\n4\n' > 05_while.expected

# 06 - break and continue
cat > 06_break_continue.hts << 'EOF'
main
Loop, 10 {
    if (A_Index = 3) { continue }
    if (A_Index = 6) { break }
    print(A_Index)
}
exit(0)
EOF
printf '0\n1\n2\n4\n5\n' > 06_break_continue.expected

# 07 - arithmetic operators
cat > 07_arithmetic.hts << 'EOF'
main
print(2 + 3)
print(10 - 4)
print(6 * 7)
print(20 / 4)
print(17 % 5)
exit(0)
EOF
printf '5\n6\n42\n5\n2\n' > 07_arithmetic.expected

# 08 - comparison operators
cat > 08_comparisons.hts << 'EOF'
main
if (5 = 5) { print("eq\n") }
if (5 != 3) { print("ne\n") }
if (3 < 5) { print("lt\n") }
if (5 > 3) { print("gt\n") }
if (3 <= 3) { print("le\n") }
if (5 >= 5) { print("ge\n") }
exit(0)
EOF
printf 'eq\nne\nlt\ngt\nle\nge\n' > 08_comparisons.expected

# 09 - negative numbers & itoa edge cases
cat > 09_negative.hts << 'EOF'
main
print(10 - 50)
print(0 - 1)
print(0)
exit(0)
EOF
printf -- '-40\n-1\n0\n' > 09_negative.expected

# 10 - nested if
cat > 10_nested_if.hts << 'EOF'
main
int x := 5
if (x > 0) {
    if (x > 3) { print("big\n") }
    else { print("small\n") }
} else {
    print("neg\n")
}
exit(0)
EOF
printf 'big\n' > 10_nested_if.expected

# 11 - nested loops with A_Index shadowing
cat > 11_nested_loops.hts << 'EOF'
main
Loop, 3 {
    Loop, 2 { print(A_Index) }
}
exit(0)
EOF
printf '0\n1\n0\n1\n0\n1\n' > 11_nested_loops.expected

# 12 - exit code
cat > 12_exit_code.hts << 'EOF'
main
print("before exit\n")
exit(42)
EOF
printf 'before exit\n' > 12_exit_code.expected
echo 42 > 12_exit_code.exitcode

# 13 - heap allocation and 64-bit dereference
cat > 13_heap.hts << 'EOF'
main
int buf := alloc(64)
[buf] := 12345
print([buf])
exit(0)
EOF
printf '12345\n' > 13_heap.expected

# 14 - byte dereference (string in heap)
cat > 14_bytes.hts << 'EOF'
main
int buf := alloc(16)
byte[buf + 0] := 72
byte[buf + 1] := 105
byte[buf + 2] := 10
print(buf, 3)
exit(0)
EOF
printf 'Hi\n' > 14_bytes.expected

# 15 - hex literals
cat > 15_hex.hts << 'EOF'
main
int x := 0xFF
int y := 0x10
print(x)
print(y)
exit(0)
EOF
printf '255\n16\n' > 15_hex.expected

# 16 - factorial via recursion
cat > 16_factorial.hts << 'EOF'
func int fact(int n) {
    if (n <= 1) { return 1 }
    return n * fact(n - 1)
}
main
print(fact(5))
print(fact(7))
exit(0)
EOF
printf '120\n5040\n' > 16_factorial.expected

# 17 - parentheses grouping
cat > 17_parens.hts << 'EOF'
main
int a := 5
int b := 3
print((a + b) * 2)
print(a + (b * 2))
exit(0)
EOF
printf '16\n11\n' > 17_parens.expected

# 18 - function calls as arguments
cat > 18_func_args.hts << 'EOF'
func int one() { return 1 }
func int two() { return 2 }
func int add(int a, int b) { return a + b }
main
print(add(one(), two()))
exit(0)
EOF
printf '3\n' > 18_func_args.expected

# 19 - variable mutation
cat > 19_mutate.hts << 'EOF'
main
int x := 10
int y := 20
int z := x + y
print(z)
x := x + 100
print(x)
y := y * 2
print(y)
exit(0)
EOF
printf '30\n110\n40\n' > 19_mutate.expected

# 20 - bool type
cat > 20_bool.hts << 'EOF'
main
bool flag := 1
if (flag) { print("yes\n") } else { print("no\n") }
flag := 0
if (flag) { print("still yes\n") } else { print("now no\n") }
exit(0)
EOF
printf 'yes\nnow no\n' > 20_bool.expected

# 21 - dynamic loop count from variable
cat > 21_dyn_loop.hts << 'EOF'
main
int n := 4
Loop, n { print(A_Index) }
exit(0)
EOF
printf '0\n1\n2\n3\n' > 21_dyn_loop.expected

# 22 - string deduplication
cat > 22_dedup.hts << 'EOF'
main
print("repeat\n")
print("repeat\n")
print("repeat\n")
exit(0)
EOF
printf 'repeat\nrepeat\nrepeat\n' > 22_dedup.expected

# 23 - auto-exit
cat > 23_autoexit.hts << 'EOF'
main
print("no explicit exit\n")
EOF
printf 'no explicit exit\n' > 23_autoexit.expected

# 24 - deep expression
cat > 24_deep_expr.hts << 'EOF'
main
int r := ((2 + 3) * (4 + 5)) - ((10 - 2) * 2)
print(r)
exit(0)
EOF
printf '29\n' > 24_deep_expr.expected

# 25 - many locals
cat > 25_many_locals.hts << 'EOF'
main
int a := 1
int b := 2
int c := 3
int d := 4
int e := 5
int f := 6
int g := 7
int h := 8
print(a + b + c + d + e + f + g + h)
exit(0)
EOF
printf '36\n' > 25_many_locals.expected

# 26 - forward reference
cat > 26_forward_ref.hts << 'EOF'
func int a(int x) { return b(x) + 1 }
func int b(int x) { return x * 2 }
main
print(a(5))
exit(0)
EOF
printf '11\n' > 26_forward_ref.expected

# 27 - multiple return paths
cat > 27_multi_return.hts << 'EOF'
func int classify(int x) {
    if (x < 0) { return 0 }
    if (x = 0) { return 1 }
    if (x < 100) { return 2 }
    return 3
}
main
print(classify(0 - 5))
print(classify(0))
print(classify(50))
print(classify(1000))
exit(0)
EOF
printf '0\n1\n2\n3\n' > 27_multi_return.expected

# 28 - six-argument function
cat > 28_six_args.hts << 'EOF'
func int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f
}
main
print(sum6(1, 2, 3, 4, 5, 6))
exit(0)
EOF
printf '21\n' > 28_six_args.expected

# 29 - long string literal (1501 chars)
{
    printf 'main\nprint("'
    for i in $(seq 1 150); do printf 'ABCDEFGHIJ'; done
    printf '\\n")\nexit(0)\n'
} > 29_long_string.hts
{
    for i in $(seq 1 150); do printf 'ABCDEFGHIJ'; done
    printf '\n'
} > 29_long_string.expected

# 30 - boundary test: 127 chars
{
    printf 'main\nprint("'
    for i in $(seq 1 12); do printf 'ABCDEFGHIJ'; done
    printf 'ABCDEF\\n")\nexit(0)\n'
} > 30_len127.hts
{
    for i in $(seq 1 12); do printf 'ABCDEFGHIJ'; done
    printf 'ABCDEF\n'
} > 30_len127.expected

# 31 - boundary test: 128 chars
{
    printf 'main\nprint("'
    for i in $(seq 1 12); do printf 'ABCDEFGHIJ'; done
    printf 'ABCDEFG\\n")\nexit(0)\n'
} > 31_len128.hts
{
    for i in $(seq 1 12); do printf 'ABCDEFGHIJ'; done
    printf 'ABCDEFG\n'
} > 31_len128.expected

# ============================================================
# NEW FEATURE TESTS (32 to 41)
# ============================================================

# 32 - Top-level global variables
cat > 32_global_vars.hts << 'EOF'
int g_num := 777

main
print(g_num)
exit(0)
EOF
printf '777\n' > 32_global_vars.expected

# 33 - Struct with braces and semicolon comments
cat > 33_struct_osp.hts << 'EOF'
; AutoHotKey-style semicolon comment!
struct Entity {
    int health ; inline field comment
    int speed
}

main
int e := alloc(16)
e.health := 100
e.speed := 25
print(e.health)
print(e.speed)
exit(0)
EOF
printf '100\n25\n' > 33_struct_osp.expected

# 34 - Struct curly-brace syntax
cat > 34_struct_braces.hts << 'EOF'
struct Point {
    int x
    int y
}

main
int pt := alloc(16)
pt.x := 12
pt.y := 34
print(pt.x + pt.y)
exit(0)
EOF
printf '46\n' > 34_struct_braces.expected

# 35 - Flexible IF with 'or' and '||'
cat > 35_logical_or.hts << 'EOF'
main
int a := 10
int b := 20
if (a = 10) or (b = 999) {
    print("or_passed\n")
}
if (a = 999) || (b = 20) {
    print("pipe_passed\n")
}
if (a = 999) or (b = 999) {
    print("fail\n")
} else {
    print("else_passed\n")
}
exit(0)
EOF
printf 'or_passed\npipe_passed\nelse_passed\n' > 35_logical_or.expected

# 36 - Flexible IF with 'and' and '&&'
cat > 36_logical_and.hts << 'EOF'
main
int x := 5
int y := 10
if (x = 5) and (y = 10) {
    print("and_passed\n")
}
if (x = 5) && (y = 99) {
    print("fail\n")
} else {
    print("and_false_passed\n")
}
exit(0)
EOF
printf 'and_passed\nand_false_passed\n' > 36_logical_and.expected

# 37 - Bitwise operators (&, |, ^, ~, <<, >>)
cat > 37_bitwise.hts << 'EOF'
main
print(1 << 5)
print(64 >> 2)
print(0xF0 & 0x30)
print(0xF0 | 0x0F)
print(0xAA ^ 0xFF)
exit(0)
EOF
printf '32\n16\n48\n255\n85\n' > 37_bitwise.expected

# 38 - String concatenation (.)
cat > 38_str_concat.hts << 'EOF'
main
str a := "Hello, "
str b := "HTSpeed!\n"
str combined := a . b
print(combined)
exit(0)
EOF
printf 'Hello, HTSpeed!\n' > 38_str_concat.expected

# 39 - Chained string concatenation
cat > 39_str_concat_chain.hts << 'EOF'
main
str first := "A"
str second := "B"
str third := "C\n"
str full := first . second . third
print(full)
exit(0)
EOF
printf 'ABC\n' > 39_str_concat_chain.expected

# 40 - Include external file
cat > inc_helper.inc << 'EOF'
func int inc_val(int n) {
    return n + 100
}
EOF

cat > 40_include.hts << 'EOF'
include "tests/inc_helper.inc"

main
int res := inc_val(50)
print(res)
exit(0)
EOF
printf '150\n' > 40_include.expected

# 41 - Command-line arguments via GetParams()
cat > 41_getparams.hts << 'EOF'
main
str params := GetParams()
print(params)
exit(0)
EOF
printf 'arg1\narg2\n1337\n' > 41_getparams.expected
printf 'arg1\narg2\n1337\n' > 41_getparams.args

# 42 - Struct allocation using 'new'
cat > 42_struct_new.hts << 'EOF'
struct Player {
    int health
    int mana
}

main
int p := new Player
p.health := 500
p.mana := 150
print(p.health)
print(p.mana)
exit(0)
EOF
printf '500\n150\n' > 42_struct_new.expected

# 43 - Multiple struct instances
cat > 43_struct_multiple.hts << 'EOF'
struct Point {
    int x
    int y
}

main
int p1 := new Point
int p2 := new Point
p1.x := 10
p1.y := 20
p2.x := 90
p2.y := 100
print(p1.x)
print(p1.y)
print(p2.x)
print(p2.y)
exit(0)
EOF
printf '10\n20\n90\n100\n' > 43_struct_multiple.expected



# 44 - Nested flow: while inside Loop with break and continue
cat > 44_nested_flow.hts << 'EOF'
main
int total := 0
Loop, 3 {
    int w := 0
    while (w < 5) {
        w := w + 1
        if (w = 2) {
            continue
        }
        if (w = 4) {
            break
        }
        total := total + 1
    }
}
print(total)
exit(0)
EOF
printf '6\n' > 44_nested_flow.expected

# 45 - Heap Bubble Sort
cat > 45_heap_bubblesort.hts << 'EOF'
main
int arr := alloc(40)
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
EOF
printf '10\n20\n30\n40\n50\n' > 45_heap_bubblesort.expected

# 46 - Complex nested boolean logic
cat > 46_complex_logic.hts << 'EOF'
main
int a := 1
int b := 0
int c := 1
int d := 1

if ((a = 1) and (b = 1)) or ((c = 1) and (d = 1)) {
    print("pass_or\n")
} else {
    print("fail_or\n")
}

if ((a = 1) or (b = 1)) and ((c = 0) or (d = 0)) {
    print("fail_and\n")
} else {
    print("pass_and\n")
}
exit(0)
EOF
printf 'pass_or\npass_and\n' > 46_complex_logic.expected

# 47 - Deep nested function call expressions
cat > 47_deep_calls.hts << 'EOF'
func int add(int a, int b) { return a + b }
func int mul(int a, int b) { return a * b }

main
int res := add(mul(2, 3), add(4, mul(5, 2)))
print(res)
exit(0)
EOF
printf '20\n' > 47_deep_calls.expected

# 48 - Dynamic string concatenation accumulation
cat > 48_str_accumulate.hts << 'EOF'
main
str msg := "START:"
msg := msg . "A"
msg := msg . "B"
msg := msg . "END\n"
print(msg)
exit(0)
EOF
printf 'START:ABEND\n' > 48_str_accumulate.expected

# 49 - Mutual recursion with forward fixups
cat > 49_mutual_recursion.hts << 'EOF'
func int is_even(int n) {
    if (n = 0) { return 1 }
    return is_odd(n - 1)
}

func int is_odd(int n) {
    if (n = 0) { return 0 }
    return is_even(n - 1)
}

main
print(is_even(6))
print(is_even(7))
print(is_odd(9))
print(is_odd(10))
exit(0)
EOF
printf '1\n0\n1\n0\n' > 49_mutual_recursion.expected

# 50 - Bitwise operator precedence stress
cat > 50_bitwise_stress.hts << 'EOF'
main
int val := ((0xFF & 0x0F) << 4) | (0xAA ^ 0xAF)
print(val)
exit(0)
EOF
printf '245\n' > 50_bitwise_stress.expected

# 51 - Standalone function calls as statements
cat > 51_func_stmt.hts << 'EOF'
func void say_hi() {
    print("hello from void func\n")
}

func int mutate(int a) {
    print(a * 2)
    return a * 2
}

main
say_hi()
mutate(21)
exit(0)
EOF
printf 'hello from void func\n42\n' > 51_func_stmt.expected


# 52 - Unary minus expressions
cat > 52_unary_minus.hts << 'EOF'
main
int a := -1
int b := -5
int c := -(10 + 20)
int d := - -42
print(a)
print(b)
print(c)
print(d)
exit(0)
EOF
printf -- '-1\n-5\n-30\n42\n' > 52_unary_minus.expected

# 53 - Global variable mutation from inside a function
cat > 53_global_mutate.hts << 'EOF'
int g_hits := 0

func void strike() {
    g_hits := g_hits + 5
}

main
print(g_hits)
strike()
strike()
print(g_hits)
exit(0)
EOF
printf '0\n10\n' > 53_global_mutate.expected

# 54 - Zero-iteration loops
cat > 54_zero_loops.hts << 'EOF'
main
int executed := 0

Loop, 0 {
    executed := 1
}

while (0) {
    executed := 2
}

print(executed)
exit(0)
EOF
printf '0\n' > 54_zero_loops.expected

# 55 - Nested function call expressions as arguments
cat > 55_nested_calls_as_args.hts << 'EOF'
func int sqr(int x) { return x * x }
func int add(int a, int b) { return a + b }

main
int res := add(sqr(3) + 1, sqr(4))
print(res)
exit(0)
EOF
printf '26\n' > 55_nested_calls_as_args.expected

# 56 - Pass-by-pointer mutation on heap
cat > 56_pass_by_pointer.hts << 'EOF'
func void fill_coords(int pt) {
    [pt + 0] := 111
    [pt + 8] := 222
}

main
int p := alloc(16)
fill_coords(p)
print([p + 0])
print([p + 8])
exit(0)
EOF
printf '111\n222\n' > 56_pass_by_pointer.expected

# 57 - String concat when variable name matches a struct field name
cat > 57_dot_field_conflict.hts << 'EOF'
struct Player {
    int health
    int name
}

main
str prefix := "Hello, "
str name := "World!\n"
str result := prefix . name
print(result)
exit(0)
EOF
printf 'Hello, World!\n' > 57_dot_field_conflict.expected

# 58 - Multiple structs sharing a field name at different offsets
cat > 58_struct_field_collision.hts << 'EOF'
struct Weapon {
    int damage
    int durability
    int value
}

struct Item {
    int value
    int weight
}

main
int it := new Item
it.value := 999
print(it.value)
exit(0)
EOF
printf '999\n' > 58_struct_field_collision.expected




echo "Created $(ls *.hts | wc -l) test files in $(pwd)"
