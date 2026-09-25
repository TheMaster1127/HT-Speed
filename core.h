#ifndef CORE_H
#define CORE_H

// Minimal Types
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
typedef unsigned long      size_t;

#define MAX_SRC     262144
#define MAX_CODE    131072
#define MAX_DATA    65536
#define MAX_LOCALS  256
#define MAX_GLOBALS 256
#define MAX_FUNCS   128
#define MAX_STRUCTS 64
#define MAX_FIELDS  32
#define MAX_FIXUPS  512

// --- Raw Linux Syscall Wrappers (Zero Libc) ---
static inline int64_t k_write(int fd, const void *buf, size_t count) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(1), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11", "memory");
    return ret;
}

static inline int64_t k_read(int fd, void *buf, size_t count) {
    int64_t ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(0), "D"(fd), "S"(buf), "d"(count) : "rcx", "r11", "memory");
    return ret;
}

static inline int k_open(const char *path, int flags, int mode) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(2), "D"(path), "S"(flags), "d"(mode) : "rcx", "r11", "memory");
    return ret;
}

static inline int k_close(int fd) {
    int ret;
    __asm__ volatile ("syscall" : "=a"(ret) : "a"(3), "D"(fd) : "rcx", "r11", "memory");
    return ret;
}

static inline void k_exit(int code) {
    __asm__ volatile ("syscall" :: "a"(60), "D"(code) : "memory");
}

// --- Minimal String & Memory Helpers ---
static inline size_t m_strlen(const char *s) {
    size_t l = 0;
    while (s[l]) l++;
    return l;
}

static inline void m_print(const char *s) {
    k_write(1, s, m_strlen(s));
}

static inline void m_memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
}

static inline void m_memset(void *dst, int val, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (size_t i = 0; i < n; i++) d[i] = (uint8_t)val;
}

static inline int m_memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return (int)p1[i] - (int)p2[i];
    }
    return 0;
}

static inline int m_strcmp(const char *a, const char *b) {
    while (*a && (*a == *b)) { a++; b++; }
    return *(const unsigned char *)a - *(const unsigned char *)b;
}

static inline int m_strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == '\0')
            return (unsigned char)a[i] - (unsigned char)b[i];
    }
    return 0;
}

static inline void m_strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    if (i < n) dst[i] = '\0';
}

static inline int m_isdigit(char c) { return c >= '0' && c <= '9'; }
static inline int m_isalpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static inline int m_isalnum(char c) { return m_isalpha(c) || m_isdigit(c); }

static inline int64_t m_strtoll(const char *p, const char **end) {
    int64_t val = 0;
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    else if (*p == '+') { p++; }
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
        while (1) {
            if (*p >= '0' && *p <= '9') val = val * 16 + (*p - '0');
            else if (*p >= 'a' && *p <= 'f') val = val * 16 + (*p - 'a' + 10);
            else if (*p >= 'A' && *p <= 'F') val = val * 16 + (*p - 'A' + 10);
            else break;
            p++;
        }
    } else {
        while (m_isdigit(*p)) {
            val = val * 10 + (*p - '0');
            p++;
        }
    }
    if (end) *end = p;
    return val * sign;
}

// --- Minimal ELF Structs ---
typedef struct {
    unsigned char e_ident[16];
    uint16_t      e_type;
    uint16_t      e_machine;
    uint32_t      e_version;
    uint64_t      e_entry;
    uint64_t      e_phoff;
    uint64_t      e_shoff;
    uint32_t      e_flags;
    uint16_t      e_ehsize;
    uint16_t      e_phentsize;
    uint16_t      e_phnum;
    uint16_t      e_shentsize;
    uint16_t      e_shnum;
    uint16_t      e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} Elf64_Phdr;

#endif
