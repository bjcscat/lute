#pragma once

#include <cstdio>

#ifdef _MSC_VER
#define LUTE_NORETURN __declspec(noreturn)
#define LUTE_NOINLINE __declspec(noinline)
#define LUTE_FORCEINLINE __forceinline
#define LUTE_LIKELY(x) x
#define LUTE_UNLIKELY(x) x
#define LUTE_UNREACHABLE() __assume(false)
#define LUTE_DEBUGBREAK() __debugbreak()
#else
#define LUTE_NORETURN __attribute__((__noreturn__))
#define LUTE_NOINLINE __attribute__((noinline))
#define LUTE_FORCEINLINE inline __attribute__((always_inline))
#define LUTE_LIKELY(x) __builtin_expect(x, 1)
#define LUTE_UNLIKELY(x) __builtin_expect(x, 0)
#define LUTE_UNREACHABLE() __builtin_unreachable()
#define LUTE_DEBUGBREAK() __builtin_trap()
#endif

constexpr bool is_uv_error(ssize_t err) noexcept
{
    return err < 0;
}

