#include "src/types.h"
#include "src/syscalls.h"
#include "src/string.h"
#include "src/elf.h"
#include "src/tables.h"
#include "src/emitter.h"
#include "src/lexer.h"
#include "src/runtimes.h"
#include "src/parser.h"
#include "src/compiler.h"

int main(void) {
    __asm__ volatile (
        "mov rax, [rsp + 8]\n"
        "mov [__argc], rax\n"
        "lea rax, [rsp + 16]\n"
        "mov [__argv], rax\n"
    );
    run_compiler();
    return 0;
}
