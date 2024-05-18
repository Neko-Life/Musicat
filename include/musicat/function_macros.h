#ifndef MUSICAT_FUNCTION_MACROS_H
#define MUSICAT_FUNCTION_MACROS_H

#define ONE_HOUR_SECOND 3600

#define STR_SIZE(x) (sizeof (x) / sizeof (x[0])) - 1

#if __cplusplus >= 201703L && __cplusplus < 202002L

#define MUSICAT_U8(x) u8##x

#elif __cplusplus >= 202002L

#define MUSICAT_U8(x) x

#else

#error Compile target only support C++17 and C++20

#endif

#define DELETE_COPY_MOVE_CTOR(x)                                              \
    x (const x &_o) = delete;                                                 \
    x (x &&_o) noexcept = delete;                                             \
    x &operator= (const x &_o) = delete;                                      \
    x &operator= (x &&_o) noexcept = delete

#endif // MUSICAT_FUNCTION_MACROS_H
