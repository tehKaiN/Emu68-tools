/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COMMON_ENDIAN_H
#define COMMON_ENDIAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <exec/types.h>

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }
static inline uint64_t BE64(uint64_t x) { return x; }
static inline uint32_t BE32(uint32_t x) { return x; }
static inline uint16_t BE16(uint16_t x) { return x; }

static inline ULONG rd32le(volatile void *addr)
{
    return LE32(*(volatile ULONG *)addr);
}

static inline void wr32le(volatile void *addr, ULONG val)
{
    *(volatile ULONG *)addr = LE32(val);
}

static inline ULONG rd32be(volatile void *addr)
{
    return *(volatile ULONG *)addr;
}

static inline void wr32be(volatile void *addr, ULONG val)
{
    *(volatile ULONG *)addr = val;
}

#ifdef __cplusplus
}
#endif

#endif // COMMON_ENDIAN_H
