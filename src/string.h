#ifndef STRING_H
#define STRING_H

#include "types.h"
#include "syscalls.h"

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

static inline void m_print_u64(uint64_t val) {
    char buf[24];
    int i = 0;
    if (val == 0) { k_write(1, "0", 1); return; }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
    k_write(1, buf, i);
}

#endif
