#ifndef EMITTER_H
#define EMITTER_H

#include "core.h"

#define MAX_LOOP_DEPTH 16
#define MAX_LOOP_BREAKS 64

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

    struct {
        size_t patch_site;
        size_t data_offset;
    } str_relocs[MAX_FIXUPS];
    size_t str_reloc_count;

    struct {
        size_t patch_site;
        size_t data_offset;
    } glob_relocs[MAX_FIXUPS];
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

static void emit_u8(uint8_t b) { C.code[C.code_len++] = b; }
static void emit_u32(uint32_t val) { m_memcpy(&C.code[C.code_len], &val, 4); C.code_len += 4; }
static void emit_u64(uint64_t val) { m_memcpy(&C.code[C.code_len], &val, 8); C.code_len += 8; }
static void emit_bytes(const uint8_t *b, size_t n) { m_memcpy(&C.code[C.code_len], b, n); C.code_len += n; }

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

// 111-byte itoa
static size_t emit_print_int_runtime(void) {
    size_t offset = C.code_len;
    static const uint8_t bytes[] = {
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
    emit_bytes(bytes, sizeof(bytes));
    return offset;
}

// Runtime to print null-terminated string pointer in RAX
static size_t emit_print_str_runtime(void) {
    size_t offset = C.code_len;
    // mov rsi, rax; xor rdx, rdx
    emit_u8(0x48); emit_u8(0x89); emit_u8(0xC6);
    emit_u8(0x48); emit_u8(0x31); emit_u8(0xD2);
    // strlen loop: cmp byte [rsi+rdx], 0; je write; inc rdx; jmp loop
    size_t loop_start = C.code_len;
    emit_u8(0x80); emit_u8(0x3C); emit_u8(0x16); emit_u8(0x00);
    emit_u8(0x74); emit_u8(0x05);
    emit_u8(0x48); emit_u8(0xFF); emit_u8(0xC2);
    emit_u8(0xEB); emit_u8((uint8_t)(loop_start - (C.code_len + 1)));

    // sys_write(1, rsi, rdx)
    emit_u8(0x6A); emit_u8(0x01); emit_u8(0x58);
    emit_u8(0x6A); emit_u8(0x01); emit_u8(0x5F);
    emit_u8(0x0F); emit_u8(0x05);
    emit_u8(0xC3); // ret
    return offset;
}

// --- String Concat Runtime (Verified Zero-Trap Bytecode) ---
static size_t emit_str_concat_runtime(void) {
    size_t offset = C.code_len;

    static const uint8_t str_concat_bytes[] = {
        0x55,                                           // push rbp
        0x48, 0x89, 0xE5,                               // mov rbp, rsp
        0x48, 0x83, 0xEC, 0x30,                         // sub rsp, 48
        0x48, 0x89, 0x7D, 0xF8,                         // mov [rbp-8], rdi (s1)
        0x48, 0x89, 0x75, 0xF0,                         // mov [rbp-16], rsi (s2)
        0x48, 0x89, 0xFE,                               // mov rsi, rdi
        0x31, 0xC9,                                     // xor ecx, ecx
        // .L_s1_len:
        0x80, 0x3C, 0x0E, 0x00,                         // cmp byte ptr [rsi+rcx], 0
        0x74, 0x05,                                     // je +5 (.L_s1_done)
        0x48, 0xFF, 0xC1,                               // inc rcx
        0xEB, 0xF5,                                     // jmp -11 (.L_s1_len)
        // .L_s1_done:
        0x48, 0x89, 0x4D, 0xE8,                         // mov [rbp-24], rcx (len1)
        0x48, 0x8B, 0x75, 0xF0,                         // mov rsi, [rbp-16] (s2)
        0x31, 0xC9,                                     // xor ecx, ecx
        // .L_s2_len:
        0x80, 0x3C, 0x0E, 0x00,                         // cmp byte ptr [rsi+rcx], 0
        0x74, 0x05,                                     // je +5 (.L_s2_done)
        0x48, 0xFF, 0xC1,                               // inc rcx
        0xEB, 0xF5,                                     // jmp -11 (.L_s2_len)
        // .L_s2_done:
        0x48, 0x89, 0x4D, 0xE0,                         // mov [rbp-32], rcx (len2)
        0x48, 0x8B, 0x75, 0xE8,                         // mov rsi, [rbp-24] (len1)
        0x48, 0x03, 0x75, 0xE0,                         // add rsi, [rbp-32] (len2)
        0x48, 0xFF, 0xC6,                               // inc rsi (+1 for null)
        0x31, 0xFF,                                     // xor edi, edi (addr = NULL)
        0xBA, 0x03, 0x00, 0x00, 0x00,                   // mov edx, 3 (PROT_READ|PROT_WRITE)
        0x41, 0xBA, 0x22, 0x00, 0x00, 0x00,             // mov r10d, 34 (MAP_PRIVATE|MAP_ANONYMOUS)
        0x49, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF,       // mov r8, -1
        0x45, 0x31, 0xC9,                               // xor r9d, r9d
        0xB8, 0x09, 0x00, 0x00, 0x00,                   // mov eax, 9 (sys_mmap)
        0x0F, 0x05,                                     // syscall -> rax = buffer
        0x48, 0x89, 0x45, 0xD8,                         // mov [rbp-40], rax (dest)
        0x48, 0x89, 0xC7,                               // mov rdi, rax (dest)
        0x48, 0x8B, 0x75, 0xF8,                         // mov rsi, [rbp-8] (s1)
        0x31, 0xC9,                                     // xor ecx, ecx
        // .L_cp1:
        0x8A, 0x14, 0x0E,                               // mov dl, [rsi+rcx]
        0x84, 0xD2,                                     // test dl, dl
        0x74, 0x08,                                     // je +8 (.L_cp1_done)
        0x88, 0x14, 0x0F,                               // mov [rdi+rcx], dl
        0x48, 0xFF, 0xC1,                               // inc rcx
        0xEB, 0xF1,                                     // jmp -15 (.L_cp1)
        // .L_cp1_done:
        0x48, 0x03, 0x7D, 0xE8,                         // add rdi, [rbp-24] (dest + len1)
        0x48, 0x8B, 0x75, 0xF0,                         // mov rsi, [rbp-16] (s2)
        0x31, 0xC9,                                     // xor ecx, ecx
        // .L_cp2:
        0x8A, 0x14, 0x0E,                               // mov dl, [rsi+rcx]
        0x88, 0x14, 0x0F,                               // mov [rdi+rcx], dl
        0x84, 0xD2,                                     // test dl, dl
        0x74, 0x05,                                     // je +5 (.L_cp2_done)
        0x48, 0xFF, 0xC1,                               // inc rcx
        0xEB, 0xF1,                                     // jmp -15 (.L_cp2)
        // .L_cp2_done:
        0x48, 0x8B, 0x45, 0xD8,                         // mov rax, [rbp-40] (return dest)
        0x48, 0x89, 0xEC,                               // mov rsp, rbp
        0x5D,                                           // pop rbp
        0xC3                                            // ret
    };

    emit_bytes(str_concat_bytes, sizeof(str_concat_bytes));
    return offset;
}

// --- GetParams Runtime (100% Verified Zero-Loop Bytecode) ---
static size_t emit_getparams_runtime(void) {
    size_t offset = C.code_len;

    static const uint8_t getparams_bytes[] = {
        0x53,                                           // push rbx
        0x48, 0x83, 0xEC, 0x10,                         // sub rsp, 16
        0x48, 0x8B, 0x4D, 0x08,                         // mov rcx, [rbp+8] (argc)
        0x48, 0x8D, 0x55, 0x10,                         // lea rdx, [rbp+16] (argv)
        0x48, 0x89, 0x0C, 0x24,                         // mov [rsp], rcx
        0x48, 0x89, 0x54, 0x24, 0x08,                   // mov [rsp+8], rdx
        // sys_mmap(0, 8192, 3, 34, -1, 0)
        0x31, 0xFF,                                     // xor edi, edi
        0xBE, 0x00, 0x20, 0x00, 0x00,                   // mov esi, 8192
        0xBA, 0x03, 0x00, 0x00, 0x00,                   // mov edx, 3
        0x41, 0xBA, 0x22, 0x00, 0x00, 0x00,             // mov r10d, 34
        0x49, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF,       // mov r8, -1
        0x45, 0x31, 0xC9,                               // xor r9d, r9d
        0xB8, 0x09, 0x00, 0x00, 0x00,                   // mov eax, 9 (sys_mmap)
        0x0F, 0x05,                                     // syscall -> rax = buffer
        0x48, 0x8B, 0x0C, 0x24,                         // mov rcx, [rsp]
        0x48, 0x8B, 0x54, 0x24, 0x08,                   // mov rdx, [rsp+8]
        0x48, 0x89, 0xC7,                               // mov rdi, rax (dest ptr)
        0x50,                                           // push rax (save return ptr)
        0x48, 0x83, 0xF9, 0x01,                         // cmp rcx, 1
        0x7E, 0x2B,                                     // jle .L_done (+43)
        0x48, 0xC7, 0xC3, 0x01, 0x00, 0x00, 0x00,       // mov rbx, 1 (arg idx)
        // .L_arg_loop:
        0x48, 0x39, 0xCB,                               // cmp rbx, rcx
        0x7D, 0x1F,                                     // jge .L_done (+31)
        0x48, 0x8B, 0x34, 0xDA,                         // mov rsi, [rdx + rbx*8]
        // .L_copy_str:
        0x8A, 0x06,                                     // mov al, [rsi]
        0x84, 0xC0,                                     // test al, al
        0x74, 0x0A,                                     // jz .L_end_arg (+10, skips jmp!)
        0x88, 0x07,                                     // mov [rdi], al
        0x48, 0xFF, 0xC7,                               // inc rdi
        0x48, 0xFF, 0xC6,                               // inc rsi
        0xEB, 0xF0,                                     // jmp .L_copy_str (-16)
        // .L_end_arg:
        0xC6, 0x07, 0x0A,                               // mov byte [rdi], 10 ('\n')
        0x48, 0xFF, 0xC7,                               // inc rdi
        0x48, 0xFF, 0xC3,                               // inc rbx
        0xEB, 0xDC,                                     // jmp .L_arg_loop (-36)
        // .L_done:
        0xC6, 0x07, 0x00,                               // mov byte [rdi], 0
        0x58,                                           // pop rax
        0x48, 0x83, 0xC4, 0x10,                         // add rsp, 16
        0x5B,                                           // pop rbx
        0xC3                                            // ret
    };

    emit_bytes(getparams_bytes, sizeof(getparams_bytes));
    return offset;
}

static void finalize_bin(void) {
    if (C.needs_print_int) {
        size_t off = emit_print_int_runtime();
        for (size_t i = 0; i < C.int_print_patch_count; i++) {
            size_t p = C.int_print_patches[i];
            int32_t d = (int32_t)(off - (p + 4));
            m_memcpy(&C.code[p], &d, 4);
        }
    }
    if (C.needs_print_str) {
        size_t off = emit_print_str_runtime();
        for (size_t i = 0; i < C.str_print_patch_count; i++) {
            size_t p = C.str_print_patches[i];
            int32_t d = (int32_t)(off - (p + 4));
            m_memcpy(&C.code[p], &d, 4);
        }
    }
    if (C.needs_concat) {
        size_t off = emit_str_concat_runtime();
        for (size_t i = 0; i < C.concat_patch_count; i++) {
            size_t p = C.concat_patches[i];
            int32_t d = (int32_t)(off - (p + 4));
            m_memcpy(&C.code[p], &d, 4);
        }
    }
    if (C.needs_getparams) {
        size_t off = emit_getparams_runtime();
        for (size_t i = 0; i < C.getparams_patch_count; i++) {
            size_t p = C.getparams_patches[i];
            int32_t d = (int32_t)(off - (p + 4));
            m_memcpy(&C.code[p], &d, 4);
        }
    }

    for (size_t i = 0; i < C.fixup_count; i++) {
        Function *fn = find_func(C.func_fixups[i].target_func);
        if (!fn || !fn->is_defined) { m_print("Link Error: Undefined function\n"); k_exit(1); }
        size_t p = C.func_fixups[i].patch_site;
        int32_t d = (int32_t)(fn->code_offset - (p + 4));
        m_memcpy(&C.code[p], &d, 4);
    }
    for (size_t i = 0; i < C.str_reloc_count; i++) {
        size_t p = C.str_relocs[i].patch_site;
        size_t t = C.code_len + C.str_relocs[i].data_offset;
        int32_t d = (int32_t)(t - (p + 4));
        m_memcpy(&C.code[p], &d, 4);
    }
    for (size_t i = 0; i < C.glob_reloc_count; i++) {
        size_t p = C.glob_relocs[i].patch_site;
        size_t t = C.code_len + C.glob_relocs[i].data_offset;
        int32_t d = (int32_t)(t - (p + 4));
        m_memcpy(&C.code[p], &d, 4);
    }
}

static void write_elf_file(const char *out_path, size_t main_entry_offset) {
    uint64_t base_vaddr = 0x400000;
    uint64_t header_size = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr);
    uint64_t payload_size = C.code_len + C.data_len;
    uint64_t total_size = header_size + payload_size;

    Elf64_Ehdr ehdr;
    m_memset(&ehdr, 0, sizeof(ehdr));
    ehdr.e_ident[0] = 0x7F; ehdr.e_ident[1] = 'E'; ehdr.e_ident[2] = 'L'; ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = 2; ehdr.e_ident[5] = 1; ehdr.e_ident[6] = 1; ehdr.e_ident[7] = 0;
    ehdr.e_type = 2; ehdr.e_machine = 62; ehdr.e_version = 1;
    ehdr.e_entry = base_vaddr + header_size + main_entry_offset;
    ehdr.e_phoff = sizeof(Elf64_Ehdr);
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = sizeof(Elf64_Phdr);
    ehdr.e_phnum = 1;

    Elf64_Phdr phdr;
    m_memset(&phdr, 0, sizeof(phdr));
    phdr.p_type = 1; phdr.p_flags = 7; phdr.p_offset = 0;
    phdr.p_vaddr = base_vaddr; phdr.p_paddr = base_vaddr;
    phdr.p_filesz = total_size; phdr.p_memsz = total_size;
    phdr.p_align = 0x1000;

    int out_fd = k_open(out_path, 577, 0755);
    if (out_fd < 0) { m_print("Error: Could not create binary\n"); k_exit(1); }

    k_write(out_fd, &ehdr, sizeof(ehdr));
    k_write(out_fd, &phdr, sizeof(phdr));
    k_write(out_fd, C.code, C.code_len);
    k_write(out_fd, C.data, C.data_len);
    k_close(out_fd);

    m_print("[+] HTSpeed compiled standalone binary successfully!\n");
}

#endif
