#ifndef PARSER_H
#define PARSER_H

#include "core.h"
#include "lexer.h"
#include "emitter.h"

static void parse_expression(void);

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
    } else if (cur_tok.kind == TOK_A_INDEX) {
        int offset = C.loop_index_offset[C.loop_depth - 1];
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

    if (cur_tok.kind == TOK_RETURN) {
        next_token();
        if (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_expression();
        emit_u8(0x48); emit_u8(0x89); emit_u8(0xEC);
        emit_u8(0x5D); emit_u8(0xC3);
        return;
    }

    if (cur_tok.kind == TOK_SYSCALL) {
        next_token();
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
        return;
    }

    // --- Dynamic print(): Handles Strings OR Numbers! ---
    if (cur_tok.kind == TOK_PRINT) {
        next_token();
        expect(TOK_LPAREN);
        if (cur_tok.kind == TOK_STR_LIT) {
            size_t str_len = cur_tok.str_len;
            size_t str_offset = add_string_literal(cur_tok.str_val, str_len);
            next_token();

            emit_u8(0x6A); emit_u8(0x01); emit_u8(0x58); // push 1; pop rax
            emit_u8(0x6A); emit_u8(0x01); emit_u8(0x5F); // push 1; pop rdi
            emit_u8(0x48); emit_u8(0x8D); emit_u8(0x35); // lea rsi, [rip + disp32]
            C.str_relocs[C.str_reloc_count].patch_site = C.code_len;
            C.str_relocs[C.str_reloc_count].data_offset = str_offset;
            C.str_reloc_count++;
            emit_u32(0);
            emit_u8(0x6A); emit_u8((uint8_t)str_len); emit_u8(0x5A); // push len; pop rdx
            emit_u8(0x0F); emit_u8(0x05); // syscall
        } else {
            // It's a dynamic expression / number!
            parse_expression(); // Evaluates to RAX
            // call __print_int runtime
            emit_u8(0xE8);
            int32_t disp = (int32_t)(C.print_int_offset - (C.code_len + 4));
            emit_u32(disp);
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

    if (cur_tok.kind == TOK_LOOP) {
        next_token();
        expect(TOK_COMMA);
        parse_expression();

        if (C.loop_depth >= 16) {
            m_print("Error: Max loop nesting (16) exceeded\n");
            k_exit(1);
        }

        int limit_off = add_local("__limit");
        int index_off = add_local("__index");

        emit_u8(0x48); emit_u8(0x89); emit_u8(0x85); emit_u32((uint32_t)(-limit_off));
        emit_u8(0x48); emit_u8(0xC7); emit_u8(0x85); emit_u32((uint32_t)(-index_off)); emit_u32(0);

        C.loop_index_offset[C.loop_depth] = index_off;
        C.loop_limit_offset[C.loop_depth] = limit_off;
        C.loop_depth++;

        size_t loop_start = C.code_len;

        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85); emit_u32((uint32_t)(-index_off));
        emit_u8(0x48); emit_u8(0x3B); emit_u8(0x85); emit_u32((uint32_t)(-limit_off));
        emit_u8(0x0F); emit_u8(0x8D);
        size_t exit_patch = C.code_len;
        emit_u32(0);

        expect(TOK_LBRACE);
        while (cur_tok.kind != TOK_RBRACE && cur_tok.kind != TOK_EOF) parse_statement();
        expect(TOK_RBRACE);

        emit_u8(0x48); emit_u8(0xFF); emit_u8(0x85); emit_u32((uint32_t)(-index_off));
        emit_u8(0xE9);
        int32_t loop_disp = (int32_t)(loop_start - (C.code_len + 4));
        emit_u32(loop_disp);

        int32_t exit_disp = (int32_t)(C.code_len - (exit_patch + 4));
        m_memcpy(&C.code[exit_patch], &exit_disp, 4);

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
