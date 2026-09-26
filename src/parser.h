#ifndef PARSER_H
#define PARSER_H

#include "types.h"
#include "lexer.h"
#include "tables.h"
#include "emitter.h"

static void parse_expression(void);
static void parse_statement(void);

#include "expr.h"
#include "stmt.h"

#endif
