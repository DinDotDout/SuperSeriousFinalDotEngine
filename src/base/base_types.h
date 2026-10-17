#ifndef BASE_TYPES_H
#define BASE_TYPES_H

////////////////////////////////////////////////////////////////
//
// Sane type renames

// size_t
#include <stddef.h>
// precise int
typedef size_t usize;
#include <stdint.h>
#include <inttypes.h>

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t uptr;
typedef ptrdiff_t iptr;

typedef u32 b32;
typedef u16 b16;
typedef u8  b8;

typedef float f32;
typedef double f64;

typedef struct String8 String8;
typedef struct Arena Arena;

#endif // !BASE_TYPES_H
