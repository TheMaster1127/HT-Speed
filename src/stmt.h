#ifndef STMT_H
#define STMT_H

#include "types.h"
#include "lexer.h"
#include "tables.h"
#include "emitter.h"

static void parse_expression(void);

static void parse_statement(void) {
    if (cur_tok.kind == TOK_TYPE_INT || cur_tok.kind == TOK_TYPE_STR || cur_tok.kind == TOK_TYPE_BOOL) {
        int is_str = (cur_tok.kind == TOK_TYPE_STR);
        next_token();
        char var_name[64];
        m_strncpy(var_name, cur_tok.str_val, 63);
        expect(TOK_IDENT);
        expect(TOK_ASSIGN);
        parse_expression();
        int offset = add_local(var_name, is_str);
        emit_u8(0x48); emit_u8(0x89); emit_u8(0x85);
        emit_u32((uint32_t)(-offset));
        return;
    }

    if (cur_tok.kind == TOK_LBRACKET) {
        next_token();
        parse_expression();
        expect(TOK_RBRACKET);
        emit_u8(0x50);
        expect(TOK_ASSIGN);
        parse_expression();
        emit_u8(0x5B);
        emit_u8(0x48); emit_u8(0x89); emit_u8(0x03);
        return;
    }

    if (cur_tok.kind == TOK_BYTE) {
        next_token();
        expect(TOK_LBRACKET);
        parse_expression();
        expect(TOK_RBRACKET);
        emit_u8(0x50);
        expect(TOK_ASSIGN);
        parse_expression();
        emit_u8(0x5B);
        emit_u8(0x88); emit_u8(0x03);
        return;
    }

    if (cur_tok.kind == TOK_RETURN) {
        next_token();
        if (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_expression();
        emit_u8(0x48); emit_u8(0x89); emit_u8(0xEC);
        emit_u8(0x5D); emit_u8(0xC3);
        return;
    }

    if (cur_tok.kind == TOK_BREAK) {
        next_token();
        if (C.loop_depth == 0) { m_print("Error: 'break' outside loop\n"); k_exit(1); }
        LoopContext *l = &C.loops[C.loop_depth - 1];
        if (l->break_count >= MAX_LOOP_BREAKS) { m_print("Error: Too many breaks\n"); k_exit(1); }
        emit_u8(0xE9);
        l->break_patches[l->break_count++] = C.code_len;
        emit_u32(0);
        return;
    }

    if (cur_tok.kind == TOK_CONTINUE) {
        next_token();
        if (C.loop_depth == 0) { m_print("Error: 'continue' outside loop\n"); k_exit(1); }
        LoopContext *l = &C.loops[C.loop_depth - 1];
        if (l->is_counted) {
            if (l->continue_count >= MAX_LOOP_BREAKS) { m_print("Error: Too many continues\n"); k_exit(1); }
            emit_u8(0xE9);
            l->continue_patches[l->continue_count++] = C.code_len;
            emit_u32(0);
        } else {
            emit_u8(0xE9);
            int32_t disp = (int32_t)(l->start_offset - (C.code_len + 4));
            emit_u32(disp);
        }
        return;
    }

    if (cur_tok.kind == TOK_SYSCALL) {
        next_token();
        parse_syscall_call();
        return;
    }

    if (cur_tok.kind == TOK_PRINT) {
        next_token();
        expect(TOK_LPAREN);
        if (cur_tok.kind == TOK_STR_LIT) {
            size_t str_len = cur_tok.str_len;
            size_t str_offset = add_string_literal(cur_tok.str_val, str_len);
            next_token();

            emit_u8(0x6A); emit_u8(0x01); emit_u8(0x58);
            emit_u8(0x6A); emit_u8(0x01); emit_u8(0x5F);
            emit_u8(0x48); emit_u8(0x8D); emit_u8(0x35);
            C.str_relocs[C.str_reloc_count].patch_site = C.code_len;
            C.str_relocs[C.str_reloc_count].data_offset = str_offset;
            C.str_reloc_count++;
            emit_u32(0);

            if (str_len <= 127) {
                emit_u8(0x6A); emit_u8((uint8_t)str_len); emit_u8(0x5A);
            } else {
                emit_u8(0xBA); emit_u32((uint32_t)str_len);
            }
            emit_u8(0x0F); emit_u8(0x05);
        } else {
            int is_str_var = 0;
            if (cur_tok.kind == TOK_IDENT) {
                is_str_var = find_local_is_str(cur_tok.str_val) || find_global_is_str(cur_tok.str_val);
            }

            parse_expression();
            if (cur_tok.kind == TOK_COMMA) {
                next_token();
                emit_u8(0x50);
                parse_expression();
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC2);
                emit_u8(0x5E);
                emit_u8(0x6A); emit_u8(0x01); emit_u8(0x58);
                emit_u8(0x6A); emit_u8(0x01); emit_u8(0x5F);
                emit_u8(0x0F); emit_u8(0x05);
            } else if (is_str_var) {
                C.needs_print_str = 1;
                emit_u8(0xE8);
                C.str_print_patches[C.str_print_patch_count++] = C.code_len;
                emit_u32(0);
            } else {
                C.needs_print_int = 1;
                emit_u8(0xE8);
                C.int_print_patches[C.int_print_patch_count++] = C.code_len;
                emit_u32(0);
            }
        }
        expect(TOK_RPAREN);
        return;
    }

    if (cur_tok.kind == TOK_EXIT) {
        next_token();
        expect(TOK_LPAREN);
        parse_expression();
        expect(TOK_RPAREN);
        emit_u8(0x48); emit_u8(0x89); emit_u8(0xC7);
        emit_u8(0x6A); emit_u8(0x3C); emit_u8(0x58);
        emit_u8(0x0F); emit_u8(0x05);
        C.has_exited = 1;
        return;
    }

    if (cur_tok.kind == TOK_IF) {
        next_token();
        parse_expression();

        emit_u8(0x48); emit_u8(0x85); emit_u8(0xC0);
        emit_u8(0x0F); emit_u8(0x84);
        size_t jz_patch = C.code_len;
        emit_u32(0);

        expect(TOK_LBRACE);
        while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
        expect(TOK_RBRACE);

        if (cur_tok.kind == TOK_ELSE) {
            next_token();
            emit_u8(0xE9);
            size_t jmp_patch = C.code_len;
            emit_u32(0);

            int32_t jz_disp = (int32_t)(C.code_len - (jz_patch + 4));
            m_memcpy(&C.code[jz_patch], &jz_disp, 4);

            expect(TOK_LBRACE);
            while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
            expect(TOK_RBRACE);

            int32_t jmp_disp = (int32_t)(C.code_len - (jmp_patch + 4));
            m_memcpy(&C.code[jmp_patch], &jmp_disp, 4);
        } else {
            int32_t jz_disp = (int32_t)(C.code_len - (jz_patch + 4));
            m_memcpy(&C.code[jz_patch], &jz_disp, 4);
        }
        return;
    }

    if (cur_tok.kind == TOK_WHILE) {
        next_token();
        if (C.loop_depth >= MAX_LOOP_DEPTH) { m_print("Error: Max loop nesting exceeded\n"); k_exit(1); }
        LoopContext *l = &C.loops[C.loop_depth++];
        l->is_counted = 0;
        l->break_count = 0;
        l->continue_count = 0;
        l->start_offset = C.code_len;

        parse_expression();

        emit_u8(0x48); emit_u8(0x85); emit_u8(0xC0);
        emit_u8(0x0F); emit_u8(0x84);
        size_t exit_patch = C.code_len;
        emit_u32(0);

        expect(TOK_LBRACE);
        while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
        expect(TOK_RBRACE);

        emit_u8(0xE9);
        int32_t loop_disp = (int32_t)(l->start_offset - (C.code_len + 4));
        emit_u32(loop_disp);

        int32_t exit_disp = (int32_t)(C.code_len - (exit_patch + 4));
        m_memcpy(&C.code[exit_patch], &exit_disp, 4);

        for (size_t i = 0; i < l->break_count; i++) {
            int32_t b_disp = (int32_t)(C.code_len - (l->break_patches[i] + 4));
            m_memcpy(&C.code[l->break_patches[i]], &b_disp, 4);
        }

        C.loop_depth--;
        return;
    }

    if (cur_tok.kind == TOK_LOOP) {
        next_token();
        expect(TOK_COMMA);
        parse_expression();

        if (C.loop_depth >= MAX_LOOP_DEPTH) { m_print("Error: Max loop nesting exceeded\n"); k_exit(1); }
        LoopContext *l = &C.loops[C.loop_depth++];
        l->is_counted = 1;
        l->break_count = 0;
        l->continue_count = 0;

        int limit_off = add_local("__limit", 0);
        int index_off = add_local("__index", 0);
        l->index_offset = index_off;
        l->limit_offset = limit_off;

        emit_u8(0x48); emit_u8(0x89); emit_u8(0x85); emit_u32((uint32_t)(-limit_off));
        emit_u8(0x48); emit_u8(0xC7); emit_u8(0x85); emit_u32((uint32_t)(-index_off)); emit_u32(0);

        size_t loop_start = C.code_len;
        l->start_offset = loop_start;

        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85); emit_u32((uint32_t)(-index_off));
        emit_u8(0x48); emit_u8(0x3B); emit_u8(0x85); emit_u32((uint32_t)(-limit_off));
        emit_u8(0x0F); emit_u8(0x8D);
        size_t exit_patch = C.code_len;
        emit_u32(0);

        expect(TOK_LBRACE);
        while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
        expect(TOK_RBRACE);

        size_t step_offset = C.code_len;
        for (size_t i = 0; i < l->continue_count; i++) {
            int32_t c_disp = (int32_t)(step_offset - (l->continue_patches[i] + 4));
            m_memcpy(&C.code[l->continue_patches[i]], &c_disp, 4);
        }

        emit_u8(0x48); emit_u8(0xFF); emit_u8(0x85); emit_u32((uint32_t)(-index_off));
        emit_u8(0xE9);
        int32_t loop_disp = (int32_t)(loop_start - (C.code_len + 4));
        emit_u32(loop_disp);

        int32_t exit_disp = (int32_t)(C.code_len - (exit_patch + 4));
        m_memcpy(&C.code[exit_patch], &exit_disp, 4);

        for (size_t i = 0; i < l->break_count; i++) {
            int32_t b_disp = (int32_t)(C.code_len - (l->break_patches[i] + 4));
            m_memcpy(&C.code[l->break_patches[i]], &b_disp, 4);
        }

        C.loop_depth--;
        return;
    }

    if (cur_tok.kind == TOK_IDENT) {
        char name[64];
        m_strncpy(name, cur_tok.str_val, 63);
        next_token();

        // 1. Function Call Statement: draw_board(board, scr)
        if (cur_tok.kind == TOK_LPAREN) {
            next_token();
            int arg_count = 0;
            while (cur_tok.kind != TOK_RPAREN) {
                parse_expression();
                emit_u8(0x50);
                arg_count++;
                if (cur_tok.kind == TOK_COMMA) next_token();
                else break;
            }
            expect(TOK_RPAREN);

            for (int i = arg_count - 1; i >= 0; i--) {
                switch (i) {
                    case 0: emit_u8(0x5F); break;
                    case 1: emit_u8(0x5E); break;
                    case 2: emit_u8(0x5A); break;
                    case 3: emit_u8(0x59); break;
                    case 4: emit_u8(0x41); emit_u8(0x58); break;
                    case 5: emit_u8(0x41); emit_u8(0x59); break;
                    default: emit_u8(0x58); break;
                }
            }

            emit_u8(0xE8);
            Function *fn = find_func(name);
            if (fn && fn->is_defined) {
                int32_t disp = (int32_t)(fn->code_offset - (C.code_len + 4));
                emit_u32(disp);
            } else {
                C.func_fixups[C.fixup_count].patch_site = C.code_len;
                m_strncpy(C.func_fixups[C.fixup_count].target_func, name, 63);
                C.fixup_count++;
                emit_u32(0);
            }
            return;
        }

        // 2. Struct Field Assignment: p.x := 10
        if (cur_tok.kind == TOK_DOT) {
            next_token();
            char field_name[64];
            m_strncpy(field_name, cur_tok.str_val, 63);
            expect(TOK_IDENT);
            expect(TOK_ASSIGN);

            int f_off = find_field_offset(field_name);
            if (f_off < 0) {
                m_print("Unknown struct field: ");
                m_print(field_name);
                m_print("\n");
                k_exit(1);
            }

            int l_off = find_local(name);
            if (l_off < 0) { m_print("Undeclared struct pointer\n"); k_exit(1); }

            emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85); emit_u32((uint32_t)(-l_off));
            emit_u8(0x48); emit_u8(0x05); emit_u32((uint32_t)f_off);
            emit_u8(0x50);
            parse_expression();
            emit_u8(0x5B);
            emit_u8(0x48); emit_u8(0x89); emit_u8(0x03);
            return;
        }

        // 3. Variable Assignment: x := 10
        if (cur_tok.kind == TOK_ASSIGN) {
            next_token();
            parse_expression();

            int offset = find_local(name);
            if (offset >= 0) {
                emit_u8(0x48); emit_u8(0x89); emit_u8(0x85);
                emit_u32((uint32_t)(-offset));
                return;
            }

            int64_t g_off = find_global(name);
            if (g_off >= 0) {
                emit_u8(0x48); emit_u8(0x89); emit_u8(0x05);
                C.glob_relocs[C.glob_reloc_count].patch_site = C.code_len;
                C.glob_relocs[C.glob_reloc_count].data_offset = (size_t)g_off;
                C.glob_reloc_count++;
                emit_u32(0);
                return;
            }

            m_print("Undeclared variable\n");
            k_exit(1);
        }
    }

    m_print("Syntax Error in statement\n");
    k_exit(1);
}

#endif
