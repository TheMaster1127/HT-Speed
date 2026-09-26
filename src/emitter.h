#ifndef EMITTER_H
#define EMITTER_H

#include "types.h"
#include "string.h"
#include "tables.h"

static void emit_u8(uint8_t b) { C.code[C.code_len++] = b; }
static void emit_u32(uint32_t val) { m_memcpy(&C.code[C.code_len], &val, 4); C.code_len += 4; }
static void emit_u64(uint64_t val) { m_memcpy(&C.code[C.code_len], &val, 8); C.code_len += 8; }
static void emit_bytes(const uint8_t *b, size_t n) { m_memcpy(&C.code[C.code_len], b, n); C.code_len += n; }

#endif
