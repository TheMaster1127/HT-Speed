#ifndef TYPES_H
#define TYPES_H

typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef int                int32_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;
typedef long long          int64_t;
typedef unsigned long      size_t;

// Heavy-Duty 64MB Bounds (Zero physical RAM cost on Linux BSS until touched!)
#define MAX_SRC     67108864   // 64 MB source code
#define MAX_CODE    67108864   // 64 MB machine code (fits 250,000+ functions)
#define MAX_DATA    33554432   // 32 MB data pool
#define MAX_LOCALS  1024
#define MAX_GLOBALS 16384
#define MAX_FUNCS   131072     // 131,072 functions
#define MAX_STRUCTS 1024
#define MAX_FIELDS  64
#define MAX_FIXUPS  131072
#define MAX_LOOP_DEPTH 16
#define MAX_LOOP_BREAKS 64

#endif
