#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/execbase.h>
#include <clib/debug_protos.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/input.h>
#include <proto/devicetree.h>

#include <stdint.h>
#include <i2C_base.h>

int __attribute__((no_reorder)) _start()
{
        return -1;
}

extern const char libraryEnd;
extern const char libraryName[];
extern const char libraryIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&libraryEnd,
    RTF_AUTOINIT,
    LIBRARY_VERSION,
    NT_LIBRARY,
    LIBRARY_PRIORITY,
    (char *)((intptr_t)&libraryName),
    (char *)((intptr_t)&libraryIdString),
    (APTR)InitTable,
};

const char libraryName[] = LIBRARY_NAME;
const char libraryIdString[] = VERSION_STRING;

// /*
//     Some properties, like e.g. #size-cells, are not always available in a key, but in that case the properties
//     should be searched for in the parent. The process repeats recursively until either root key is found
//     or the property is found, whichever occurs first
// */
// CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR devicetree)
// {
//     do {
//         /* Find the property first */
//         APTR prop = DT_FindProperty(key, property);

//         if (prop)
//         {
//             /* If property is found, get its value and exit */
//             return DT_GetPropValue(prop);
//         }

//         /* Property was not found, go to the parent and repeat */
//         key = DT_GetParent(key);
//     } while (key);

//     return NULL;
// }

// int _strcmp(const char *s1, const char *s2)
// {
//     while (*s1 == *s2++)
//         if (*s1++ == '\0')
//             return (0);
//     return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
// }

static struct I2C_Base * OpenLib(ULONG version asm("d0"), struct I2C_Base *I2C_Base asm("a6"))
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    I2C_Base->LibNode.lib_OpenCnt++;
    I2C_Base->LibNode.lib_Flags &= ~LIBF_DELEXP;

    return I2C_Base;
}

static ULONG ExpungeLib(struct I2C_Base *I2C_Base asm("a6"))
{
    struct ExecBase *SysBase = I2C_Base->SysBase;
    BPTR segList = 0;

    if (I2C_Base->LibNode.lib_OpenCnt == 0)
    {
        /* Remove library from Exec's list */
        Remove(&I2C_Base->LibNode.lib_Node);

        /* Close all eventually opened libraries */
        // if (I2C_Base->vc4_DOSBase != NULL)
        //     CloseLibrary((struct Library *)I2C_Base->vc4_DOSBase);

        /* Save seglist */
        segList = I2C_Base->SegList;

        /* Remove I2C_Base itself - free the memory */
        ULONG size = I2C_Base->LibNode.lib_NegSize + I2C_Base->LibNode.lib_PosSize;
        FreeMem((APTR)((ULONG)I2C_Base - I2C_Base->LibNode.lib_NegSize), size);
    }
    else
    {
        /* Library is still in use, set delayed expunge flag */
        I2C_Base->LibNode.lib_Flags |= LIBF_DELEXP;
    }

    /* Return 0 or segList */
    return segList;
}

static ULONG CloseLib(struct I2C_Base *I2C_Base asm("a6"))
{
    if (I2C_Base->LibNode.lib_OpenCnt != 0)
        I2C_Base->LibNode.lib_OpenCnt--;

    if (I2C_Base->LibNode.lib_OpenCnt == 0)
    {
        if (I2C_Base->LibNode.lib_Flags & LIBF_DELEXP)
            return ExpungeLib(I2C_Base);
    }

    return 0;
}


static uint32_t ExtFunc()
{
    return 0;
}

struct I2C_Base * LibInit(
    struct I2C_Base *base asm("d0"),
    BPTR seglist asm("a0"),
    struct ExecBase *SysBase asm("a6")
)
{
    struct I2C_Base *I2cBase = base;
    I2cBase->SegList = seglist;
    I2cBase->LibNode.lib_Revision = LIBRARY_REVISION;
    I2cBase->SysBase = SysBase;
    // I2cBase->Enabled = -1;

    return I2cBase;
}

static uint32_t vc4_functions[] = {
    (uint32_t)OpenLib,
    (uint32_t)CloseLib,
    (uint32_t)ExpungeLib,
    (uint32_t)ExtFunc,
    (uint32_t)AllocI2C,
    (uint32_t)FreeI2C,
    (uint32_t)SetI2CDelay,
    (uint32_t)InitI2C,
    (uint32_t)SendI2C,
    (uint32_t)ReceiveI2C,
    (uint32_t)GetI2COpponent,
    (uint32_t)I2CErrText,
    (uint32_t)ShutDownI2C,
    (uint32_t)BringBackI2C,
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct I2C_Base),
    (uint32_t)vc4_functions,
    0,
    (uint32_t)LibInit
};

#if 0

unsigned char a[] = {
  0x10, 0xf0, 0x00, 0xc0, 0x80, 0x03, 0x10, 0xf8, 0x40, 0xc0, 0x40, 0x00,
  0xc0, 0xf3, 0x00, 0x00, 0x10, 0xf8, 0x80, 0xc0, 0x00, 0x00, 0xc0, 0xf3,
  0x01, 0x00, 0x10, 0xf8, 0xc0, 0xc0, 0x40, 0x00, 0xc0, 0xf3, 0x01, 0x00,
  0x90, 0xf0, 0x30, 0x00, 0x81, 0x03, 0x90, 0xf8, 0x30, 0x00, 0x40, 0x10,
  0xc0, 0xf3, 0x04, 0x00, 0x90, 0xf8, 0x30, 0x00, 0x00, 0x20, 0xc0, 0xf3,
  0x05, 0x00, 0x90, 0xf8, 0x30, 0x00, 0x40, 0x30, 0xc0, 0xf3, 0x05, 0x00,
  0x40, 0xe8, 0x00, 0x01, 0x00, 0x00, 0x41, 0xe8, 0x00, 0x01, 0x00, 0x00,
  0x12, 0x66, 0x02, 0x6a, 0xd4, 0x18, 0x5a, 0x00
};

void test()
{
    int c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x3000c);
    FBReq[c++] = LE32(12);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(sizeof(a));  // 32 bytes
    FBReq[c++] = LE32(4);   // 4 byte align
    FBReq[c++] = LE32((3 << 2) | (1 << 6));   // COHERENT | DIRECT | HINT_PERMALOCK
    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);

    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    int handle = LE32(FBReq[5]);
    kprintf("Alloc returned %d\n", handle);

    c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x0003000d);
    FBReq[c++] = LE32(4);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(handle);  // 32 bytes
    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);

    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    uint64_t phys = LE32(FBReq[5]);
    uint64_t cpu = phys & ~0xc0000000;
    kprintf("Locl memory returned %08x, CPU addr %08x\n", phys, cpu);

    phys &= ~0xc0000000;
    for (unsigned i=0; i < sizeof(a); i++)
        ((uint8_t *)cpu)[i] = a[i];
    arm_flush_cache(cpu, sizeof(a));

    kprintf("test code uploaded\n");

    kprintf("running code with r0=%08x, r1=%08x, r2=%08x\n", 0xc0000000, fb_phys_base, 30000);

    uint32_t t0 = LE32(*(volatile uint32_t*)0xf2003004);

for (int i=0; i < 20000; i++) {
    if (i == 1)
        phys++;
    c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x00030010);
    FBReq[c++] = LE32(28);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(phys);  // code address
    FBReq[c++] = LE32(0xc0000000 + (0x3fffffff & ((uint64_t)i * 15000*256))); // r0 source address
    FBReq[c++] = LE32(fb_phys_base); // r1 dest address
    FBReq[c++] = LE32(7500); // r2 Number of 256-byte packets
    FBReq[c++] = 0; // r3
    FBReq[c++] = 0; // r4
    FBReq[c++] = 0; // r5

    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);

    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);
}

    uint32_t t1 = LE32(*(volatile uint32_t*)0xf2003004);

    kprintf("Returned from test code. Retval = %08x\n", LE32(FBReq[5]));
    kprintf("Time wasted: %d milliseconds\n", (t1 - t0) / 1000);

}

#endif
