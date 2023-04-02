/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
#include <i2c_private.h>

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
    AllocI2C(0, NULL, I2C_Base);
    InitI2C(I2C_Base);

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
    FreeI2C(I2C_Base);
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
