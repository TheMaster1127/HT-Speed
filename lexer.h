#ifndef LEXER_H
#define LEXER_H

#include "core.h"

typedef enum {
    TOK_EOF, TOK_INT_LIT, TOK_STR_LIT, TOK_IDENT,
    TOK_FUNC, TOK_MAIN, TOK_RETURN, TOK_IF, TOK_ELSE,
    TOK_LOOP, TOK_SYSCALL, TOK_PRINT, TOK_EXIT,
    TOK_TYPE_INT, TOK_TYPE_STR, TOK_TYPE_BOOL, TOK_TYPE_VOID,
    TOK_A_INDEX, TOK_ASSIGN, TOK_EQ, TOK_NE, TOK_LT, TOK_LE,
    TOK_GT, TOK_GE, TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_PERCENT, TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE, TOK_COMMA
} TokenKind;

typedef struct {
    TokenKind kind;
    int64_t int_val;
    char str_val[2048];
    size_t str_len;
} Token;

static char src_buf[MAX_SRC];
static const char *src;
static Token cur_tok;

static void skip_whitespace_and_comments(void) {
    while (*src) {
        if (*src == ' ' || *src == '\t' || *src == '\r' || *src == '\n') {
            src++;
        } else if (*src == '/' && *(src + 1) == '/') {
            src += 2;
            while (*src && *src != '\n') src++;
        } else if (*src == '#') {
            src++;
            while (*src && *src != '\n') src++;
        } else {
            break;
        }
    }
}

static void next_token(void) {
    skip_whitespace_and_comments();
    if (!*src) { cur_tok.kind = TOK_EOF; return; }

    if (m_isdigit(*src)) {
        cur_tok.int_val = m_strtoll(src, &src);
        cur_tok.kind = TOK_INT_LIT;
        return;
    }

    if (*src == '"') {
        src++;
        size_t len = 0;
        while (*src && *src != '"') {
            if (*src == '\\') {
                src++;
                if (*src == 'n') cur_tok.str_val[len++] = '\n';
                else if (*src == 't') cur_tok.str_val[len++] = '\t';
                else if (*src == 'r') cur_tok.str_val[len++] = '\r';
                else if (*src == '0') cur_tok.str_val[len++] = '\0';
                else cur_tok.str_val[len++] = *src;
                src++;
            } else {
                cur_tok.str_val[len++] = *src++;
            }
        }
        if (*src == '"') src++;
        cur_tok.str_val[len] = '\0';
        cur_tok.str_len = len;
        cur_tok.kind = TOK_STR_LIT;
        return;
    }

    if (m_isalpha(*src) || *src == '_') {
        size_t len = 0;
        while (m_isalnum(*src) || *src == '_') cur_tok.str_val[len++] = *src++;
        cur_tok.str_val[len] = '\0';

        if (m_strcmp(cur_tok.str_val, "func") == 0) cur_tok.kind = TOK_FUNC;
        else if (m_strcmp(cur_tok.str_val, "main") == 0) cur_tok.kind = TOK_MAIN;
        else if (m_strcmp(cur_tok.str_val, "return") == 0) cur_tok.kind = TOK_RETURN;
        else if (m_strcmp(cur_tok.str_val, "if") == 0) cur_tok.kind = TOK_IF;
        else if (m_strcmp(cur_tok.str_val, "else") == 0) cur_tok.kind = TOK_ELSE;
        else if (m_strcmp(cur_tok.str_val, "Loop") == 0) cur_tok.kind = TOK_LOOP;
        else if (m_strcmp(cur_tok.str_val, "syscall") == 0) cur_tok.kind = TOK_SYSCALL;
        else if (m_strcmp(cur_tok.str_val, "print") == 0) cur_tok.kind = TOK_PRINT;
        else if (m_strcmp(cur_tok.str_val, "exit") == 0) cur_tok.kind = TOK_EXIT;
        else if (m_strcmp(cur_tok.str_val, "int") == 0) cur_tok.kind = TOK_TYPE_INT;
        else if (m_strcmp(cur_tok.str_val, "str") == 0) cur_tok.kind = TOK_TYPE_STR;
        else if (m_strcmp(cur_tok.str_val, "bool") == 0) cur_tok.kind = TOK_TYPE_BOOL;
        else if (m_strcmp(cur_tok.str_val, "void") == 0) cur_tok.kind = TOK_TYPE_VOID;
        else if (m_strcmp(cur_tok.str_val, "A_Index") == 0) cur_tok.kind = TOK_A_INDEX;
        else cur_tok.kind = TOK_IDENT;
        return;
    }

    if (*src == ':' && *(src + 1) == '=') { src += 2; cur_tok.kind = TOK_ASSIGN; return; }
    if (*src == '!' && *(src + 1) == '=') { src += 2; cur_tok.kind = TOK_NE; return; }
    if (*src == '<' && *(src + 1) == '=') { src += 2; cur_tok.kind = TOK_LE; return; }
    if (*src == '>' && *(src + 1) == '=') { src += 2; cur_tok.kind = TOK_GE; return; }

    switch (*src) {
        case '=': cur_tok.kind = TOK_EQ; break;
        case '<': cur_tok.kind = TOK_LT; break;
        case '>': cur_tok.kind = TOK_GT; break;
        case '+': cur_tok.kind = TOK_PLUS; break;
        case '-': cur_tok.kind = TOK_MINUS; break;
        case '*': cur_tok.kind = TOK_STAR; break;
        case '/': cur_tok.kind = TOK_SLASH; break;
        case '%': cur_tok.kind = TOK_PERCENT; break;
        case '(': cur_tok.kind = TOK_LPAREN; break;
        case ')': cur_tok.kind = TOK_RPAREN; break;
        case '{': cur_tok.kind = TOK_LBRACE; break;
        case '}': cur_tok.kind = TOK_RBRACE; break;
        case ',': cur_tok.kind = TOK_COMMA; break;
        default:
            m_print("Unknown character in lexer\n");
            k_exit(1);
    }
    src++;
}

static void expect(TokenKind kind) {
    if (cur_tok.kind != kind) {
        m_print("Syntax Error: Unexpected token\n");
        k_exit(1);
    }
    next_token();
}

#endif
