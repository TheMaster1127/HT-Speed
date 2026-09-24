#ifndef PARSER_H
#define PARSER_H

#include "core.h"
#include "lexer.h"
#include "emitter.h"

static void parse_expression(void);

static void parse_syscall_call(void) {
    expect(TOK_LPAREN);
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
            case 0: emit_u8(0x58); break;
            case 1: emit_u8(0x5F); break;
            case 2: emit_u8(0x5E); break;
            case 3: emit_u8(0x5A); break;
            case 4: emit_u8(0x41); emit_u8(0x5A); break;
            case 5: emit_u8(0x41); emit_u8(0x58); break;
            case 6: emit_u8(0x41); emit_u8(0x59); break;
            default: emit_u8(0x58); break;
        }
    }
    emit_u8(0x0F); emit_u8(0x05);
}

static void parse_primary(void) {
    if (cur_tok.kind == TOK_INT_LIT) {
        emit_u8(0x48); emit_u8(0xB8);
        emit_u64((uint64_t)cur_tok.int_val);
        next_token();
    } else if (cur_tok.kind == TOK_STR_LIT) {
        size_t str_offset = add_string_literal(cur_tok.str_val, cur_tok.str_len);
        emit_u8(0x48); emit_u8(0x8D); emit_u8(0x05);
        C.str_relocs[C.str_reloc_count].patch_site = C.code_len;
        C.str_relocs[C.str_reloc_count].data_offset = str_offset;
        C.str_reloc_count++;
        emit_u32(0);
        next_token();
    } else if (cur_tok.kind == TOK_SYSCALL) {
        next_token();
        parse_syscall_call();
    } else if (cur_tok.kind == TOK_ALLOC) {
        next_token();
        expect(TOK_LPAREN);
        parse_expression();
        expect(TOK_RPAREN);

        emit_u8(0x48); emit_u8(0x89); emit_u8(0xC6);
        emit_u8(0x31); emit_u8(0xFF);
        emit_u8(0xBA); emit_u32(3);
        emit_u8(0x41); emit_u8(0xBA); emit_u32(34);
        emit_u8(0x49); emit_u8(0xC7); emit_u8(0xC0); emit_u32(0xFFFFFFFF);
        emit_u8(0x45); emit_u8(0x31); emit_u8(0xC9);
        emit_u8(0xB8); emit_u32(9);
        emit_u8(0x0F); emit_u8(0x05);
    } else if (cur_tok.kind == TOK_LBRACKET) {
        next_token();
        parse_expression();
        expect(TOK_RBRACKET);
        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x00);
    } else if (cur_tok.kind == TOK_BYTE) {
        next_token();
        expect(TOK_LBRACKET);
        parse_expression();
        expect(TOK_RBRACKET);
        emit_u8(0x48); emit_u8(0x0F); emit_u8(0xB6); emit_u8(0x00);
    } else if (cur_tok.kind == TOK_A_INDEX) {
        if (C.loop_depth == 0) {
            m_print("Error: A_Index used outside of a loop\n");
            k_exit(1);
        }
        int offset = C.loops[C.loop_depth - 1].index_offset;
        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85);
        emit_u32((uint32_t)(-offset));
        next_token();
    } else if (cur_tok.kind == TOK_LPAREN) {
        next_token();
        parse_expression();
        expect(TOK_RPAREN);
    } else if (cur_tok.kind == TOK_IDENT) {
        char name[64];
        m_strncpy(name, cur_tok.str_val, 63);
        next_token();

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
        } else {
            int offset = find_local(name);
            if (offset < 0) {
                m_print("Undeclared identifier\n");
                k_exit(1);
            }
            emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85);
            emit_u32((uint32_t)(-offset));
        }
    } else {
        m_print("Syntax Error in expression\n");
        k_exit(1);
    }
}

static int get_precedence(TokenKind k) {
    switch (k) {
        case TOK_EQ: case TOK_NE: case TOK_LT:
        case TOK_LE: case TOK_GT: case TOK_GE: return 1;
        case TOK_PLUS: case TOK_MINUS: return 2;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 3;
        default: return 0;
    }
}

static void parse_binary_expr(int min_prec) {
    parse_primary();
    while (get_precedence(cur_tok.kind) >= min_prec) {
        TokenKind op = cur_tok.kind;
        int prec = get_precedence(op);
        next_token();

        emit_u8(0x50);
        parse_binary_expr(prec + 1);
        emit_u8(0x5B);

        switch (op) {
            case TOK_PLUS:  emit_u8(0x48); emit_u8(0x01); emit_u8(0xD8); break;
            case TOK_MINUS: emit_u8(0x48); emit_u8(0x29); emit_u8(0xC3); emit_u8(0x48); emit_u8(0x89); emit_u8(0xD8); break;
            case TOK_STAR:  emit_u8(0x48); emit_u8(0x0F); emit_u8(0xAF); emit_u8(0xC3); break;
            case TOK_SLASH:
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC1);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xD8);
                emit_u8(0x48); emit_u8(0x99);
                emit_u8(0x48); emit_u8(0xF7); emit_u8(0xF9);
                break;
            case TOK_PERCENT:
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC1);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xD8);
                emit_u8(0x48); emit_u8(0x99);
                emit_u8(0x48); emit_u8(0xF7); emit_u8(0xF9);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xD0);
                break;
            case TOK_EQ: case TOK_NE: case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE:
                emit_u8(0x48); emit_u8(0x39); emit_u8(0xC3);
                emit_u8(0x0F);
                if (op == TOK_EQ) emit_u8(0x94);
                else if (op == TOK_NE) emit_u8(0x95);
                else if (op == TOK_LT) emit_u8(0x9C);
                else if (op == TOK_LE) emit_u8(0x9E);
                else if (op == TOK_GT) emit_u8(0x9F);
                else if (op == TOK_GE) emit_u8(0x9D);
                emit_u8(0xC0);
                emit_u8(0x48); emit_u8(0x0F); emit_u8(0xB6); emit_u8(0xC0);
                break;
            default: break;
        }
    }
}

static void parse_expression(void) { parse_binary_expr(1); }

static void parse_statement(void) {
    if (cur_tok.kind == TOK_TYPE_INT || cur_tok.kind == TOK_TYPE_STR || cur_tok.kind == TOK_TYPE_BOOL) {
        next_token();
        char var_name[64];
        m_strncpy(var_name, cur_tok.str_val, 63);
        expect(TOK_IDENT);
        expect(TOK_ASSIGN);
        parse_expression();
        int offset = add_local(var_name);
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
                emit_u8(0x6A); emit_u8((uint8_t)str_len); emit_u8(0x5A); // push imm8; pop rdx (3 bytes)
            } else {
                emit_u8(0xBA); emit_u32((uint32_t)str_len);               // mov edx, imm32 (5 bytes, zero-extends!)
            }
            emit_u8(0x0F); emit_u8(0x05);
        } else {
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
            } else {
                // Number printing: trigger lazy emission!
                C.needs_print_int = 1;
                emit_u8(0xE8); // call disp32
                C.int_print_patches[C.int_print_patch_count++] = C.code_len;
                emit_u32(0); // placeholder
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
        C.has_exited = 1; // Mark that code already called exit!
        return;
    }

    if (cur_tok.kind == TOK_IF) {
        next_token();
        expect(TOK_LPAREN);
        parse_expression();
        expect(TOK_RPAREN);

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
        expect(TOK_LPAREN);

        if (C.loop_depth >= MAX_LOOP_DEPTH) { m_print("Error: Max loop nesting exceeded\n"); k_exit(1); }
        LoopContext *l = &C.loops[C.loop_depth++];
        l->is_counted = 0;
        l->break_count = 0;
        l->continue_count = 0;
        l->start_offset = C.code_len;

        parse_expression();
        expect(TOK_RPAREN);

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

        int limit_off = add_local("__limit");
        int index_off = add_local("__index");
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
        if (cur_tok.kind == TOK_ASSIGN) {
            next_token();
            parse_expression();
            int offset = find_local(name);
            if (offset < 0) { m_print("Undeclared variable\n"); k_exit(1); }
            emit_u8(0x48); emit_u8(0x89); emit_u8(0x85);
            emit_u32((uint32_t)(-offset));
            return;
        }
    }

    m_print("Syntax Error in statement\n");
    k_exit(1);
}

#endif
