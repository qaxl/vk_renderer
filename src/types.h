#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef unsigned __int128 u128;

typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;
typedef signed __int128 s128;

typedef size_t usize;

typedef char buf;

u32 round_to_highest_pow_of_2(u32 value);

