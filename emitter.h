#ifndef EMITTER_H
#define EMITTER_H

#include "core.h"

#define MAX_LOOP_DEPTH 16
#define MAX_LOOP_BREAKS 64

typedef struct {
    char name[64];
    int stack_offset;
} LocalVar;

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

    Function funcs[MAX_FUNCS];
    size_t func_count;

    FuncFixup func_fixups[MAX_FIXUPS];
    size_t fixup_count;

    struct {
        size_t patch_site;
        size_t data_offset;
    } str_relocs[MAX_FIXUPS];
    size_t str_reloc_count;

    size_t int_print_patches[MAX_FIXUPS];
    size_t int_print_patch_count;

    StringEntry strings[MAX_FIXUPS];
    size_t string_count;

    int needs_print_int;
    int has_exited;

    LoopContext loops[MAX_LOOP_DEPTH];
    int loop_depth;
} Compiler;

static Compiler C;

static void emit_u8(uint8_t b) { C.code[C.code_len++] = b; }
static void emit_u32(uint32_t val) { m_memcpy(&C.code[C.code_len], &val, 4); C.code_len += 4; }
static void emit_u64(uint64_t val) { m_memcpy(&C.code[C.code_len], &val, 8); C.code_len += 8; }
static void emit_bytes(const uint8_t *b, size_t n) { m_memcpy(&C.code[C.code_len], b, n); C.code_len += n; }

static int add_local(const char *name) {
    C.current_stack_frame += 8;
    m_strncpy(C.locals[C.local_count].name, name, 63);
    C.locals[C.local_count].stack_offset = C.current_stack_frame;
    C.local_count++;
    return C.current_stack_frame;
}

static int find_local(const char *name) {
    for (int i = (int)C.local_count - 1; i >= 0; i--) {
        if (m_strcmp(C.locals[i].name, name) == 0) return C.locals[i].stack_offset;
    }
    return -1;
}

static Function *find_func(const char *name) {
    for (size_t i = 0; i < C.func_count; i++) {
        if (m_strcmp(C.funcs[i].name, name) == 0) return &C.funcs[i];
    }
    return 0;
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
    C.data_len += len;
    C.strings[C.string_count].offset = off;
    C.strings[C.string_count].len = len;
    C.string_count++;
    return off;
}

// Emits the 111-byte itoa runtime only when needed
static size_t emit_print_int_runtime(void) {
    size_t offset = C.code_len;

    static const uint8_t print_int_bytes[] = {
        0x48, 0x83, 0xEC, 0x28,
        0x48, 0x8D, 0x74, 0x24, 0x27,
        0xC6, 0x06, 0x0A,
        0x41, 0xB8, 0x01, 0x00, 0x00, 0x00,
        0x48, 0xBB, 0x0A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x48, 0x85, 0xC0,
        0x75, 0x0B,
        0x48, 0xFF, 0xCE,
        0xC6, 0x06, 0x30,
        0x41, 0xFF, 0xC0,
        0xEB, 0x33,
        0x45, 0x31, 0xC9,
        0x48, 0x85, 0xC0,
        0x79, 0x06,
        0x41, 0xFF, 0xC1,
        0x48, 0xF7, 0xD8,
        0x48, 0x85, 0xC0,
        0x74, 0x12,
        0x31, 0xD2,
        0x48, 0xF7, 0xF3,
        0x80, 0xC2, 0x30,
        0x48, 0xFF, 0xCE,
        0x88, 0x16,
        0x41, 0xFF, 0xC0,
        0xEB, 0xE9,
        0x45, 0x85, 0xC9,
        0x74, 0x09,
        0x48, 0xFF, 0xCE,
        0xC6, 0x06, 0x2D,
        0x41, 0xFF, 0xC0,
        0x6A, 0x01, 0x58,
        0x6A, 0x01, 0x5F,
        0x4C, 0x89, 0xC2,
        0x0F, 0x05,
        0x48, 0x83, 0xC4, 0x28,
        0xC3
    };

    emit_bytes(print_int_bytes, sizeof(print_int_bytes));
    return offset;
}

static void finalize_bin(void) {
    // 1. If program printed integers, emit runtime at the end and patch calls
    if (C.needs_print_int) {
        size_t runtime_offset = emit_print_int_runtime();
        for (size_t i = 0; i < C.int_print_patch_count; i++) {
            size_t patch = C.int_print_patches[i];
            int32_t disp = (int32_t)(runtime_offset - (patch + 4));
            m_memcpy(&C.code[patch], &disp, 4);
        }
    }

    // 2. Resolve function calls
    for (size_t i = 0; i < C.fixup_count; i++) {
        Function *fn = find_func(C.func_fixups[i].target_func);
        if (!fn || !fn->is_defined) { m_print("Link Error: Undefined function\n"); k_exit(1); }
        size_t patch = C.func_fixups[i].patch_site;
        int32_t disp = (int32_t)(fn->code_offset - (patch + 4));
        m_memcpy(&C.code[patch], &disp, 4);
    }

    // 3. Resolve string displacements
    for (size_t i = 0; i < C.str_reloc_count; i++) {
        size_t patch = C.str_relocs[i].patch_site;
        size_t target = C.code_len + C.str_relocs[i].data_offset;
        int32_t disp = (int32_t)(target - (patch + 4));
        m_memcpy(&C.code[patch], &disp, 4);
    }
}

static void write_elf_file(const char *out_path, size_t main_entry_offset) {
    uint64_t base_vaddr = 0x400000;
    uint64_t header_size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    uint64_t payload_size = C.code_len + C.data_len;
    uint64_t total_size = header_size + payload_size;

    Elf64_Ehdr ehdr;
    m_memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[0] = 0x7F;
    ehdr.e_ident[1] = 'E';
    ehdr.e_ident[2] = 'L';
    ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = 2;
    ehdr.e_ident[5] = 1;
    ehdr.e_ident[6] = 1;
    ehdr.e_ident[7] = 0;
    ehdr.e_type = 2;
    ehdr.e_machine = 62;
    ehdr.e_version = 1;
    ehdr.e_entry = base_vaddr + header_size + main_entry_offset;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 1;

    Elf64_Phdr phdr;
    m_memset(&phdr, 0, sizeof(phdr));
    phdr.p_type = 1;
    phdr.p_flags = 7;
    phdr.p_offset = 0;
    phdr.p_vaddr = base_vaddr;
    phdr.p_paddr = base_vaddr;
    phdr.p_filesz = total_size;
    phdr.p_memsz = total_size;
    phdr.p_align = 0x1000;

    int out_fd = k_open(out_path, 577, 0755);
    if (out_fd < 0) {
        m_print("Error: Could not create output binary\n");
        k_exit(1);
    }

    k_write(out_fd, &ehdr, sizeof(ehdr));
    k_write(out_fd, &phdr, sizeof(phdr));
    k_write(out_fd, C.code, C.code_len);
    k_write(out_fd, C.data, C.data_len);
    k_close(out_fd);

    m_print("[+] HTSpeed compiled standalone binary successfully!\n");
}

#endif
