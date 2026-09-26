#ifndef TYPES_H
#define TYPES_H

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
#define MAX_LOOP_DEPTH 16
#define MAX_LOOP_BREAKS 64

#endif
