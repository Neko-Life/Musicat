#ifndef MUSICAT_FUNCTION_MACROS_H
#define MUSICAT_FUNCTION_MACROS_H

#define ONE_HOUR_SECOND 3600

#define STR_SIZE(x) (sizeof (x) / sizeof (x[0])) - 1

#if __cplusplus >= 201703L && __cplusplus < 202002L

#define MUSICAT_U8(x) u8x

#elif __cplusplus >= 202002L

#define MUSICAT_U8(x) x

#else

#error Compile target only support C++17 and C++20

#endif

#endif // MUSICAT_FUNCTION_MACROS_H
