#ifndef COMPILER_H
#define COMPILER_H

#include "types.h"
#include "syscalls.h"
#include "string.h"
#include "tables.h"
#include "lexer.h"
#include "emitter.h"
#include "runtimes.h"
#include "parser.h"

static void load_source_with_includes(const char *path, char *dest, size_t *dest_len) {
    int fd = k_open(path, 0, 0);
    if (fd < 0) {
        m_print("Error: Could not open file\n");
        k_exit(1);
    }
    char buf[MAX_SRC / 2];
    int64_t n = k_read(fd, buf, sizeof(buf) - 1);
    k_close(fd);
    if (n < 0) { m_print("Error: Could not read file\n"); k_exit(1); }
    buf[n] = '\0';

    const char *p = buf;
    while (*p) {
        if (*p == 'i' && m_strncmp(p, "include", 7) == 0 && (p[7] == ' ' || p[7] == '\t')) {
            p += 7;
            while (*p == ' ' || *p == '\t') p++;
            if (*p == '"') {
                p++;
                char inc_path[128];
                size_t l = 0;
                while (*p && *p != '"' && l < 127) inc_path[l++] = *p++;
                inc_path[l] = '\0';
                if (*p == '"') p++;
                load_source_with_includes(inc_path, dest, dest_len);
                continue;
            }
        }
        dest[(*dest_len)++] = *p++;
    }
    dest[*dest_len] = '\0';
}

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

    size_t total_src_len = 0;
    load_source_with_includes(in_path, src_buf, &total_src_len);
    src = src_buf;

    // Reset state counters only (DO NOT memset entire 200KB BSS)
    C.code_len = 0;
    C.data_len = 16;
    C.local_count = 0;
    C.current_stack_frame = 0;
    C.global_count = 0;
    C.func_count = 0;
    C.struct_count = 0;
    C.fixup_count = 0;
    C.str_reloc_count = 0;
    C.glob_reloc_count = 0;
    C.int_print_patch_count = 0;
    C.str_print_patch_count = 0;
    C.concat_patch_count = 0;
    C.getparams_patch_count = 0;
    C.string_count = 0;
    C.needs_print_int = 0;
    C.needs_print_str = 0;
    C.needs_concat = 0;
    C.needs_getparams = 0;
    C.has_exited = 0;
    C.loop_depth = 0;

    next_token();

    size_t main_entry_offset = 0;
    int found_main = 0;

    while (cur_tok.kind != TOK_EOF) {
        if (cur_tok.kind == TOK_STRUCT) {
            next_token();
            char s_name[64];
            m_strncpy(s_name, cur_tok.str_val, 63);
            expect(TOK_IDENT);

            StructDef *st = &C.structs[C.struct_count++];
            m_strncpy(st->name, s_name, 63);
            st->field_count = 0;
            st->total_size = 0;

            expect(TOK_LBRACE);

            while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) {
                if (cur_tok.kind == TOK_TYPE_INT || cur_tok.kind == TOK_TYPE_STR || cur_tok.kind == TOK_TYPE_BOOL) {
                    next_token();
                }
                char f_name[64];
                m_strncpy(f_name, cur_tok.str_val, 63);
                expect(TOK_IDENT);
                m_strncpy(st->fields[st->field_count].name, f_name, 63);
                st->fields[st->field_count].offset = st->total_size;
                st->field_count++;
                st->total_size += 8;
            }

            expect(TOK_RBRACE);
            continue;
        }

        if (cur_tok.kind == TOK_TYPE_INT || cur_tok.kind == TOK_TYPE_STR || cur_tok.kind == TOK_TYPE_BOOL) {
            int is_str = (cur_tok.kind == TOK_TYPE_STR);
            next_token();
            char g_name[64];
            m_strncpy(g_name, cur_tok.str_val, 63);
            expect(TOK_IDENT);
            expect(TOK_ASSIGN);

            size_t off = add_global(g_name, is_str);
            if (cur_tok.kind == TOK_INT_LIT) {
                *(int64_t *)(&C.data[off]) = cur_tok.int_val;
                next_token();
            } else if (cur_tok.kind == TOK_STR_LIT) {
                size_t s_off = add_string_literal(cur_tok.str_val, cur_tok.str_len);
                *(int64_t *)(&C.data[off]) = (int64_t)s_off;
                next_token();
            } else {
                m_print("Global variable requires constant initializer\n");
                k_exit(1);
            }
            continue;
        }

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
                int p_is_str = (cur_tok.kind == TOK_TYPE_STR);
                next_token();
                char p_name[64];
                m_strncpy(p_name, cur_tok.str_val, 63);
                expect(TOK_IDENT);
                int off = add_local(p_name, p_is_str);

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

            if (!C.has_exited) {
                emit_u8(0x6A); emit_u8(0x3C); emit_u8(0x58);
                emit_u8(0x31); emit_u8(0xFF);
                emit_u8(0x0F); emit_u8(0x05);
            }
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

#endif
