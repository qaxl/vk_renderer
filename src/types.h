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

typedef char byte;

u32 round_to_highest_pow_of_2(u32 value);

#if defined(NDEBUG)
#define ZULK_RELELASE 1
#else
#define ZULK_DEBUG 1
#endif

#if defined(_WIN32) || defined(_WIN64)
#define ZULK_WIN32 1
#elif defined(__linux__)
#define ZULK_LINUX 1
#else
#error "This platform is currently unsupported by ZVK."
#endif

#define ZARRSIZ(x) sizeof(x) / sizeof(*x)
#define ZCLAMP(x, up, low) min(up, max(x, low))
