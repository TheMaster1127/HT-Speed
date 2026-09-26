#ifndef TABLES_H
#define TABLES_H

#include "types.h"
#include "string.h"

typedef struct {
    char name[64];
    int stack_offset;
    int is_str;
} LocalVar;

typedef struct {
    char name[64];
    size_t data_offset;
    int is_str;
} GlobalVar;

typedef struct {
    char name[64];
    size_t code_offset;
    int is_defined;
} Function;

typedef struct {
    size_t patch_site;
    char target_func[64];
} FuncFixup;

typedef struct {
    size_t offset;
    size_t len;
} StringEntry;

typedef struct {
    char name[64];
    int offset;
} StructField;

typedef struct {
    char name[64];
    StructField fields[MAX_FIELDS];
    size_t field_count;
    size_t total_size;
} StructDef;

typedef struct {
    size_t start_offset;
    size_t break_patches[MAX_LOOP_BREAKS];
    size_t break_count;
    size_t continue_patches[MAX_LOOP_BREAKS];
    size_t continue_count;
    int is_counted;
    int index_offset;
    int limit_offset;
} LoopContext;

typedef struct {
    uint8_t code[MAX_CODE];
    size_t code_len;
    uint8_t data[MAX_DATA];
    size_t data_len;

    LocalVar locals[MAX_LOCALS];
    size_t local_count;
    int current_stack_frame;

    GlobalVar globals[MAX_GLOBALS];
    size_t global_count;

    Function funcs[MAX_FUNCS];
    size_t func_count;

    StructDef structs[MAX_STRUCTS];
    size_t struct_count;

    FuncFixup func_fixups[MAX_FIXUPS];
    size_t fixup_count;

    struct { size_t patch_site; size_t data_offset; } str_relocs[MAX_FIXUPS];
    size_t str_reloc_count;

    struct { size_t patch_site; size_t data_offset; } glob_relocs[MAX_FIXUPS];
    size_t glob_reloc_count;

    size_t int_print_patches[MAX_FIXUPS];
    size_t int_print_patch_count;

    size_t str_print_patches[MAX_FIXUPS];
    size_t str_print_patch_count;

    size_t concat_patches[MAX_FIXUPS];
    size_t concat_patch_count;

    size_t getparams_patches[MAX_FIXUPS];
    size_t getparams_patch_count;

    StringEntry strings[MAX_FIXUPS];
    size_t string_count;

    int needs_print_int;
    int needs_print_str;
    int needs_concat;
    int needs_getparams;
    int has_exited;

    LoopContext loops[MAX_LOOP_DEPTH];
    int loop_depth;
} Compiler;

static Compiler C;

static int add_local(const char *name, int is_str) {
    C.current_stack_frame += 8;
    m_strncpy(C.locals[C.local_count].name, name, 63);
    C.locals[C.local_count].stack_offset = C.current_stack_frame;
    C.locals[C.local_count].is_str = is_str;
    C.local_count++;
    return C.current_stack_frame;
}

static int find_local(const char *name) {
    for (int i = (int)C.local_count - 1; i >= 0; i--) {
        if (m_strcmp(C.locals[i].name, name) == 0) return C.locals[i].stack_offset;
    }
    return -1;
}

static int find_local_is_str(const char *name) {
    for (int i = (int)C.local_count - 1; i >= 0; i--) {
        if (m_strcmp(C.locals[i].name, name) == 0) return C.locals[i].is_str;
    }
    return 0;
}

static size_t add_global(const char *name, int is_str) {
    size_t off = C.data_len;
    C.data_len += 8;
    m_strncpy(C.globals[C.global_count].name, name, 63);
    C.globals[C.global_count].data_offset = off;
    C.globals[C.global_count].is_str = is_str;
    C.global_count++;
    return off;
}

static int64_t find_global(const char *name) {
    for (size_t i = 0; i < C.global_count; i++) {
        if (m_strcmp(C.globals[i].name, name) == 0) return (int64_t)C.globals[i].data_offset;
    }
    return -1;
}

static int find_global_is_str(const char *name) {
    for (size_t i = 0; i < C.global_count; i++) {
        if (m_strcmp(C.globals[i].name, name) == 0) return C.globals[i].is_str;
    }
    return 0;
}

static Function *find_func(const char *name) {
    for (size_t i = 0; i < C.func_count; i++) {
        if (m_strcmp(C.funcs[i].name, name) == 0) return &C.funcs[i];
    }
    return 0;
}

static StructDef *find_struct(const char *name) {
    for (size_t i = 0; i < C.struct_count; i++) {
        if (m_strcmp(C.structs[i].name, name) == 0) return &C.structs[i];
    }
    return 0;
}

static int find_field_offset(const char *field_name) {
    for (size_t s = 0; s < C.struct_count; s++) {
        for (size_t f = 0; f < C.structs[s].field_count; f++) {
            if (m_strcmp(C.structs[s].fields[f].name, field_name) == 0) {
                return C.structs[s].fields[f].offset;
            }
        }
    }
    return -1;
}

static size_t add_string_literal(const char *str, size_t len) {
    for (size_t i = 0; i < C.string_count; i++) {
        if (C.strings[i].len == len) {
            if (m_memcmp(&C.data[C.strings[i].offset], str, len) == 0) {
                return C.strings[i].offset;
            }
        }
    }
    size_t off = C.data_len;
    m_memcpy(&C.data[C.data_len], str, len);
    C.data[C.data_len + len] = '\0';
    C.data_len += len + 1;
    C.strings[C.string_count].offset = off;
    C.strings[C.string_count].len = len;
    C.string_count++;
    return off;
}

#endif
