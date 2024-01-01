/**
 * Copyright (c) Riven Zheng (zhengheiot@gmail.com).
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#ifndef _TYPE_H_
#define _TYPE_H_

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef char                                    char_t;
typedef unsigned char                           uchar_t;
typedef unsigned char                           u8_t;
typedef unsigned short                          u16_t;
typedef unsigned int                            u32_t;
typedef unsigned long long                      u64_t;
typedef signed char                             i8_t;
typedef signed short                            i16_t;
typedef signed int                              i32_t;
typedef signed long long                        i64_t;
typedef volatile char                           vchar_t;
typedef volatile unsigned char                  vuchar_t;
typedef volatile unsigned char                  vu8_t;
typedef volatile unsigned short                 vu16_t;
typedef volatile unsigned int                   vu32_t;
typedef volatile unsigned long long             vu64_t;
typedef volatile signed char                    vi8_t;
typedef volatile signed short                   vi16_t;
typedef volatile signed int                     vi32_t;
typedef volatile signed long long               vi64_t;
typedef bool                                    b_t;
typedef u32_t                                   u32p_t;

#ifndef FALSE
    #define FALSE                               false
#endif

#ifndef TRUE
    #define TRUE                                true
#endif

#define UNUSED_MSG(x)                           (void)(x)
#define UNUSED_ARG                              (0u)

#define FLAG(x)                                 (!!(x))
#define UNFLAG(x)                               (!!!(x))

#define MAGIC(pre, post)                        pre ## post

#if defined __COUNTER__
    #define COMPLIER_UNIQUE_NUMBER              __COUNTER__
#else
    #define COMPLIER_UNIQUE_NUMBER              __LINE__
#endif

#define HW_REG32(addr)                          (*(volatile u32_t *)(u32_t)(addr))
#define HW_REG16(addr)                          (*(volatile u16_t *)(u32_t)(addr))
#define HW_REG8(addr)                           (*(volatile u8_t *)(u32_t)(addr))

#define SET_BIT(x)                              ((u32_t)((u32_t)0x01u<<(x)))
#define SET_BITS(start, end)                    ((0xFFFFFFFFul << (start)) & (0xFFFFFFFFul >> (31u - (u32_t)(end))))
#define GET_BITS(regval, start, end)            (((regval) & SET_BITS((start),(end))) >> (start))

#define DEQUALIFY(s, v)                         ((s)(u32_t)(const volatile void *)(v))
#define OFFSETOF(s, m)                          ((u32_t)(&((s *)0)->m))
#define CONTAINEROF(p, s, m)                    (DEQUALIFY(s*, ((const vu8_t *)(p) - OFFSETOF(s, m))))
#define SIZEOF(arr)                             (sizeof(arr))
#define DIMOF(arr)                              (SIZEOF(arr) / SIZEOF(arr[0]))

#define ROUND_UP(size, align)                   (((u32_t)(size) + (align - 1u)) & (~(align - 1)))
#define ROUND_DOWN(size, align)                 (((u32_t)(size)) & (~(align - 1)))
#define RANGE_ADDRESS_CONDITION(address, pool)  (((u32_t)(address) >= (u32_t)(pool)) && ((u32_t)(address) < ((u32_t)(pool) + (u16_t)SIZEOF(pool))))

void  _memcpy(char_t *dst, const char_t *src, u32_t cnt);
void  _memset(char_t *dst, u8_t val, u32_t cnt);
i32_t _memcmp(const char_t *dst, const char_t *src, u32_t cnt);
i32_t _memstr(const char_t *str, u32_t chr);
u32_t _strlen(const char_t *str);

#ifdef __cplusplus
}
#endif

#endif /* _TYPE_H_ */

