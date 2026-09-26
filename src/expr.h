#ifndef EXPR_H
#define EXPR_H

#include "types.h"
#include "lexer.h"
#include "tables.h"
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
    } else if (cur_tok.kind == TOK_GETPARAMS) {
        next_token();
        expect(TOK_LPAREN);
        expect(TOK_RPAREN);
        C.needs_getparams = 1;
        emit_u8(0xE8);
        C.getparams_patches[C.getparams_patch_count++] = C.code_len;
        emit_u32(0);
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
    } else if (cur_tok.kind == TOK_NEW) {
        next_token();
        char s_name[64];
        m_strncpy(s_name, cur_tok.str_val, 63);
        expect(TOK_IDENT);

        StructDef *st = find_struct(s_name);
        if (!st) {
            m_print("Error: Unknown struct type in 'new'\n");
            k_exit(1);
        }

        emit_u8(0x48); emit_u8(0xC7); emit_u8(0xC6); emit_u32((uint32_t)st->total_size);
        emit_u8(0x31); emit_u8(0xFF);
        emit_u8(0xBA); emit_u32(3);
        emit_u8(0x41); emit_u8(0xBA); emit_u32(34);
        emit_u8(0x49); emit_u8(0xC7); emit_u8(0xC0); emit_u32(0xFFFFFFFF);
        emit_u8(0x45); emit_u8(0x31); emit_u8(0xC9);
        emit_u8(0xB8); emit_u32(9);
        emit_u8(0x0F); emit_u8(0x05);
    } else if (cur_tok.kind == TOK_BIT_NOT) {
        next_token();
        parse_primary();
        emit_u8(0x48); emit_u8(0xF7); emit_u8(0xD0);
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
        if (C.loop_depth == 0) { m_print("Error: A_Index outside loop\n"); k_exit(1); }
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

        if (cur_tok.kind == TOK_DOT) {
            const char *p = src;
            while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
            if (m_isalpha(*p) || *p == '_') {
                char peek_field[64];
                size_t pl = 0;
                while ((m_isalnum(*p) || *p == '_') && pl < 63) peek_field[pl++] = *p++;
                peek_field[pl] = '\0';

                int f_off = find_field_offset(peek_field);
                if (f_off >= 0) {
                    next_token();
                    next_token();
                    int l_off = find_local(name);
                    if (l_off >= 0) {
                        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85); emit_u32((uint32_t)(-l_off));
                        emit_u8(0x48); emit_u8(0x8B); emit_u8(0x80); emit_u32((uint32_t)f_off);
                        return;
                    }
                }
            }
        }

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
            if (offset >= 0) {
                emit_u8(0x48); emit_u8(0x8B); emit_u8(0x85);
                emit_u32((uint32_t)(-offset));
                return;
            }

            int64_t g_off = find_global(name);
            if (g_off >= 0) {
                emit_u8(0x48); emit_u8(0x8B); emit_u8(0x05);
                C.glob_relocs[C.glob_reloc_count].patch_site = C.code_len;
                C.glob_relocs[C.glob_reloc_count].data_offset = (size_t)g_off;
                C.glob_reloc_count++;
                emit_u32(0);
                return;
            }

            m_print("Undeclared identifier\n");
            k_exit(1);
        }
    } else {
        m_print("Syntax Error in expression\n");
        k_exit(1);
    }
}

static int get_precedence(TokenKind k) {
    switch (k) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_DOT: return 3;
        case TOK_BIT_OR: return 4;
        case TOK_BIT_XOR: return 5;
        case TOK_BIT_AND: return 6;
        case TOK_EQ: case TOK_NE: return 7;
        case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE: return 8;
        case TOK_SHL: case TOK_SHR: return 9;
        case TOK_PLUS: case TOK_MINUS: return 10;
        case TOK_STAR: case TOK_SLASH: case TOK_PERCENT: return 11;
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
            case TOK_BIT_AND: emit_u8(0x48); emit_u8(0x21); emit_u8(0xD8); break;
            case TOK_BIT_OR:  emit_u8(0x48); emit_u8(0x09); emit_u8(0xD8); break;
            case TOK_BIT_XOR: emit_u8(0x48); emit_u8(0x31); emit_u8(0xD8); break;
            case TOK_SHL:
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC1);
                emit_u8(0x48); emit_u8(0xD3); emit_u8(0xE3);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xD8);
                break;
            case TOK_SHR:
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC1);
                emit_u8(0x48); emit_u8(0xD3); emit_u8(0xFB);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xD8);
                break;
            case TOK_AND:
                emit_u8(0x48); emit_u8(0x85); emit_u8(0xDB);
                emit_u8(0x0F); emit_u8(0x95); emit_u8(0xC3);
                emit_u8(0x48); emit_u8(0x85); emit_u8(0xC0);
                emit_u8(0x0F); emit_u8(0x95); emit_u8(0xC0);
                emit_u8(0x20); emit_u8(0xD8);
                emit_u8(0x48); emit_u8(0x0F); emit_u8(0xB6); emit_u8(0xC0);
                break;
            case TOK_OR:
                emit_u8(0x48); emit_u8(0x85); emit_u8(0xDB);
                emit_u8(0x0F); emit_u8(0x95); emit_u8(0xC3);
                emit_u8(0x48); emit_u8(0x85); emit_u8(0xC0);
                emit_u8(0x0F); emit_u8(0x95); emit_u8(0xC0);
                emit_u8(0x08); emit_u8(0xD8);
                emit_u8(0x48); emit_u8(0x0F); emit_u8(0xB6); emit_u8(0xC0);
                break;
            case TOK_DOT:
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xF7);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xDF);
                emit_u8(0x48); emit_u8(0x89); emit_u8(0xC6);
                C.needs_concat = 1;
                emit_u8(0xE8);
                C.concat_patches[C.concat_patch_count++] = C.code_len;
                emit_u32(0);
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

#endif
