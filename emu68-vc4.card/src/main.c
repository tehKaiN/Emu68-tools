#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/execbase.h>
#include <clib/debug_protos.h>
#include <devices/inputevent.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/input.h>
#include <proto/devicetree.h>

#include <stdint.h>

#include "boardinfo.h"
#include "emu68-vc4.h"
#include "mbox.h"

int __attribute__((no_reorder)) _start()
{
        return -1;
}

extern const char deviceEnd;
extern const char deviceName[];
extern const char deviceIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&deviceEnd,
    RTF_AUTOINIT,
    VC4CARD_VERSION,
    NT_LIBRARY,
    VC4CARD_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    (APTR)InitTable,
};

const char deviceName[] = "emu68-vc4.card";
const char deviceIdString[] = VERSION_STRING;

/*
    Some properties, like e.g. #size-cells, are not always available in a key, but in that case the properties
    should be searched for in the parent. The process repeats recursively until either root key is found
    or the property is found, whichever occurs first
*/
CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase)
{
    do {
        /* Find the property first */
        APTR property = DT_FindProperty(key, property);

        if (property)
        {
            /* If property is found, get its value and exit */
            return DT_GetPropValue(property);
        }
        
        /* Property was not found, go to the parent and repeat */
        key = DT_GetParent(key);
    } while (key);

    return NULL;
}

static int FindCard(struct BoardInfo* bi asm("a0"), struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    APTR DeviceTreeBase = NULL;
    APTR key;

    // Cancel loading the driver if left or right shift is being held down.
    struct IORequest io;

    if (OpenDevice((STRPTR)"input.device", 0, &io, 0) == 0)
    {
        struct Library *InputBase = (struct Library *)io.io_Device;
        UWORD qual = PeekQualifier();
        CloseDevice(&io);

        if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
            return 0;
    }

    /* Open DOS, Expansion and Intuition, but I don't know yet why... */
    VC4Base->vc4_ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    
    if (VC4Base->vc4_ExpansionBase == NULL) {
        return 0;
    }

    VC4Base->vc4_IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
    
    if (VC4Base->vc4_IntuitionBase == NULL) {
        CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        return 0;
    }

    VC4Base->vc4_DOSBase = (struct DOSBase *)OpenLibrary("dos.library", 0);

    if (VC4Base->vc4_DOSBase == NULL) {
        CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);
        CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        return 0;
    }

    /* Alloc 128-byte aligned memory for mailbox requests */
    VC4Base->vc4_RequestBase = AllocMem(256*4, MEMF_FAST);
    VC4Base->vc4_Request = (ULONG *)(((intptr_t)VC4Base->vc4_RequestBase + 127) & ~127);

    /* Open device tree resource */
    DeviceTreeBase = (struct Library *)OpenResource((STRPTR)"devicetree.resource");
    if (DeviceTreeBase == 0) {
        // If devicetree.resource can't be opened, this probably isn't Emu68.
        return 0;
    }
    VC4Base->vc4_DeviceTreeBase = DeviceTreeBase;

    /* Get VC4 physical address of mailbox interface. Subsequently it will be translated to m68k physical address */
    key = DT_OpenKey("/aliases");
    if (key)
    {
        CONST_STRPTR mbox_alias = DT_GetPropValue(DT_FindProperty(key, "mailbox"));

        DT_CloseKey(key);
        
        if (mbox_alias != NULL)
        {
            key = DT_OpenKey(mbox_alias);

            if (key)
            {
                int size_cells = 1;
                int address_cells = 1;

                const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                if (siz != NULL)
                    size_cells = *siz;
                
                if (addr != NULL)
                    address_cells = *addr;

                const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                VC4Base->vc4_MailBox = (APTR)reg[address_cells - 1];

                DT_CloseKey(key);
            }
        }
    }

    /* Open /soc key and learn about VC4 to CPU mapping. Use it to adjust the addresses obtained above */
    key = DT_OpenKey("/soc");
    if (key)
    {
        int size_cells = 1;
        int address_cells = 1;

        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

        if (siz != NULL)
            size_cells = *siz;
        
        if (addr != NULL)
            address_cells = *addr;

        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "ranges"));

        ULONG phys_vc4 = reg[address_cells - 1];
        ULONG phys_cpu = reg[2 * address_cells - 1];

        VC4Base->vc4_MailBox = (APTR)((ULONG)VC4Base->vc4_MailBox - phys_vc4 + phys_cpu);

        DT_CloseKey(key);
    }

    /* Find out base address of framebuffer and video memory size */
    get_vc_memory(&VC4Base->vc4_MemBase, &VC4Base->vc4_MemSize, VC4Base);

    /* Set basic data in BoardInfo structure */
    bi->MemoryBase = VC4Base->vc4_MemBase;
    bi->MemorySize = VC4Base->vc4_MemSize;
    bi->RegisterBase = NULL;

    return 1;
}

static void InitCard()
{

}

static struct VC4Base * OpenLib(ULONG version asm("d0"), struct VC4Base *VC4Base asm("a6"))
{
    VC4Base->vc4_LibNode.lib_OpenCnt++;
    VC4Base->vc4_LibNode.lib_Flags &= ~LIBF_DELEXP;

    return VC4Base;
}

static ULONG ExpungeLib(struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    BPTR segList = 0;

    if (VC4Base->vc4_LibNode.lib_OpenCnt == 0)
    {
        /* Free memory of mailbox request buffer */
        FreeMem(VC4Base->vc4_RequestBase, 4*256);

        /* Remove library from Exec's list */
        Remove(&VC4Base->vc4_LibNode.lib_Node);

        /* Close all eventually opened libraries */
        if (VC4Base->vc4_ExpansionBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        if (VC4Base->vc4_DOSBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_DOSBase);
        if (VC4Base->vc4_IntuitionBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);

        /* Save seglist */
        segList = VC4Base->vc4_SegList;

        /* Remove VC4Base itself - free the memory */
        ULONG size = VC4Base->vc4_LibNode.lib_NegSize + VC4Base->vc4_LibNode.lib_PosSize;
        FreeMem((APTR)((ULONG)VC4Base - VC4Base->vc4_LibNode.lib_NegSize), size);
    }
    else
    {
        /* Library is still in use, set delayed expunge flag */
        VC4Base->vc4_LibNode.lib_Flags |= LIBF_DELEXP;
    }

    /* Return 0 or segList */
    return segList;
}

static ULONG CloseLib(struct VC4Base *VC4Base asm("a6"))
{
    if (VC4Base->vc4_LibNode.lib_OpenCnt != 0)
        VC4Base->vc4_LibNode.lib_OpenCnt--;
    
    if (VC4Base->vc4_LibNode.lib_OpenCnt == 0)
    {
        if (VC4Base->vc4_LibNode.lib_Flags & LIBF_DELEXP)
            return ExpungeLib(VC4Base);
    }

    return 0;
}


static uint32_t ExtFunc()
{
    return 0;
}

struct VC4Base * vc4_Init(struct VC4Base *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"))
{
    struct VC4Base *VC4Base = base;
    VC4Base->vc4_SegList = seglist;
    VC4Base->vc4_SysBase = SysBase;
    VC4Base->vc4_LibNode.lib_Revision = VC4CARD_REVISION;

    return VC4Base;
}

static uint32_t vc4_functions[] = {
    (uint32_t)OpenLib,
    (uint32_t)CloseLib,
    (uint32_t)ExpungeLib,
    (uint32_t)ExtFunc,
    (uint32_t)FindCard,
    (uint32_t)InitCard,
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct VC4Base), 
    (uint32_t)vc4_functions, 
    0, 
    (uint32_t)vc4_Init
};
