#ifndef SYSCALLS_H
#define SYSCALLS_H

#include "types.h"

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

#endif
