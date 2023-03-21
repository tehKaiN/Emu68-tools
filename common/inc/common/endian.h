#ifndef COMMON_ENDIAN_H
#define COMMON_ENDIAN_H

#include <stdint.h>
#include <exec/types.h>

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

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

#endif // COMMON_ENDIAN_H
