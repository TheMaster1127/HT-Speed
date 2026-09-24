#include "core.h"
#include "lexer.h"
#include "emitter.h"
#include "parser.h"

__attribute__((noinline))
static void run_compiler(void) {
    const char *in_path = 0;
    const char *out_path = "hello";

    if (__argc >= 2) {
        in_path = __argv[1];
        if (__argc >= 3) out_path = __argv[2];
    } else {
        m_print("Usage: htspeed <input.hts> [output_binary]\n");
        k_exit(1);
    }

    // 1. Read input file
    int in_fd = k_open(in_path, 0 /* O_RDONLY */, 0);
    if (in_fd < 0) {
        m_print("Error: Could not open input file\n");
        k_exit(1);
    }

    int64_t bytes_read = k_read(in_fd, src_buf, MAX_SRC - 1);
    k_close(in_fd);

    if (bytes_read < 0) {
        m_print("Error: Could not read file\n");
        k_exit(1);
    }
    src_buf[bytes_read] = '\0';
    src = src_buf;

    // 2. Initialize Compiler State
    m_memset(&C, 0, sizeof(C));
    emit_print_int_runtime();
    next_token();

    size_t main_entry_offset = 0;
    int found_main = 0;

    // 3. High-Level Parse Loop
    while (cur_tok.kind != TOK_EOF) {
        if (cur_tok.kind == TOK_FUNC) {
            next_token();
            next_token();
            char fn_name[64];
            m_strncpy(fn_name, cur_tok.str_val, 63);
            expect(TOK_IDENT);
            expect(TOK_LPAREN);

            Function *fn = find_func(fn_name);
            if (!fn) {
                fn = &C.funcs[C.func_count++];
                m_strncpy(fn->name, fn_name, 63);
            }
            fn->code_offset = C.code_len;
            fn->is_defined = 1;

            C.local_count = 0;
            C.current_stack_frame = 0;

            emit_u8(0x55);
            emit_u8(0x48); emit_u8(0x89); emit_u8(0xE5);
            emit_u8(0x48); emit_u8(0x81); emit_u8(0xEC);
            emit_u32(256);

            int param_idx = 0;
            while (cur_tok.kind != TOK_RPAREN) {
                next_token();
                char p_name[64];
                m_strncpy(p_name, cur_tok.str_val, 63);
                expect(TOK_IDENT);
                int off = add_local(p_name);

                switch (param_idx) {
                    case 0: emit_u8(0x48); emit_u8(0x89); emit_u8(0xBD); break;
                    case 1: emit_u8(0x48); emit_u8(0x89); emit_u8(0xB5); break;
                    case 2: emit_u8(0x48); emit_u8(0x89); emit_u8(0x95); break;
                    case 3: emit_u8(0x48); emit_u8(0x89); emit_u8(0x8D); break;
                    case 4: emit_u8(0x4C); emit_u8(0x89); emit_u8(0x85); break;
                    case 5: emit_u8(0x4C); emit_u8(0x89); emit_u8(0x8D); break;
                }
                emit_u32((uint32_t)(-off));
                param_idx++;
                if (cur_tok.kind == TOK_COMMA) next_token();
                else break;
            }
            expect(TOK_RPAREN);
            expect(TOK_LBRACE);

            while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
            expect(TOK_RBRACE);

            emit_u8(0x48); emit_u8(0x89); emit_u8(0xEC);
            emit_u8(0x5D); emit_u8(0xC3);
            continue;
        }

        if (cur_tok.kind == TOK_MAIN) {
            next_token();
            found_main = 1;
            main_entry_offset = C.code_len;

            C.local_count = 0;
            C.current_stack_frame = 0;

            emit_u8(0x55);
            emit_u8(0x48); emit_u8(0x89); emit_u8(0xE5);
            emit_u8(0x48); emit_u8(0x81); emit_u8(0xEC);
            emit_u32(4096);

            while (cur_tok.kind != TOK_EOF) parse_statement();

            // Auto-exit
            emit_u8(0x6A); emit_u8(0x3C); emit_u8(0x58);
            emit_u8(0x31); emit_u8(0xFF);
            emit_u8(0x0F); emit_u8(0x05);
            break;
        }

        m_print("Syntax Error: Top-level declaration expected\n");
        k_exit(1);
    }

    if (!found_main) {
        m_print("Error: No 'main' label\n");
        k_exit(1);
    }

    finalize_bin();
    write_elf_file(out_path, main_entry_offset);
}

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
