/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/libraries.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include "base.h"

UBYTE rom_end;

ULONG L_Expunge(struct Library * LibraryBase asm("a6"))
{
    /* This is rom based device. no expunge here */
    if (LibraryBase->lib_OpenCnt > 0)
    {
        LibraryBase->lib_Flags |= LIBF_DELEXP;
    }

    return 0;
}

APTR L_ExtFunc(struct Library * LibraryBase asm("a6"))
{
    return LibraryBase;
}

struct Library * L_Open(ULONG version asm("d0"), struct Library * LibraryBase asm("a6"))
{
    LibraryBase->lib_OpenCnt++;
    LibraryBase->lib_Flags &= ~LIBF_DELEXP;

    return LibraryBase;
}

ULONG L_Close(struct Library * LibraryBase asm("a6"))
{
    LibraryBase->lib_OpenCnt--;
    
    return 0;
}

extern const char deviceName[];
extern const char deviceIdString[];

static void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct Library *LibraryBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct CurrentBinding binding;

    APTR base_pointer = NULL;
    
    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    GetCurrentBinding(&binding, sizeof(binding));

    base_pointer = AllocMem(LIB_NEGSIZE + LIB_POSSIZE, MEMF_PUBLIC | MEMF_CLEAR);

    if (base_pointer)
    {
        ULONG relFuncTable[] = {
            (ULONG)L_Open + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
            (ULONG)L_Close + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
            (ULONG)L_Expunge + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
            (ULONG)L_ExtFunc + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
            -1
        };
        LibraryBase = (struct Library *)((UBYTE *)base_pointer + LIB_NEGSIZE);
        MakeFunctions(LibraryBase, relFuncTable, 0);
        
        LibraryBase->lib_Node.ln_Type = NT_LIBRARY;
        LibraryBase->lib_Node.ln_Pri = LIB_PRIORITY;
        LibraryBase->lib_Node.ln_Name = (STRPTR)((ULONG)deviceName + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

        LibraryBase->lib_NegSize = LIB_NEGSIZE;
        LibraryBase->lib_PosSize = LIB_POSSIZE;
        LibraryBase->lib_Version = LIB_VERSION;
        LibraryBase->lib_Revision = LIB_REVISION;
        LibraryBase->lib_IdString = (STRPTR)((ULONG)deviceIdString + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);    

        SumLibrary(LibraryBase);
        AddLibrary(LibraryBase);

        {
            ULONG args[] = {
                SysBase->AttnFlags,
                AFF_68010 | AFF_68020 | AFF_68030 | AFF_68040
            };

            RawDoFmt("[68040] Found AttnFlags = %08lx. Patching to %08lx\n", args, (APTR)putch, NULL);

            SysBase->AttnFlags = AFF_68010 | AFF_68020 | AFF_68030 | AFF_68040;
        }

        APTR vbr_old;
        APTR old_stack = SuperState();

        asm volatile("movec vbr, %0":"=r"(vbr_old));

        if (old_stack != NULL)
            UserState(old_stack);

        if (vbr_old == NULL) {
            RawDoFmt("[68040] VBR was at address 0. Moving it to FastRAM\n", NULL, (APTR)putch, NULL);
            APTR vbr_new = AllocMem(256 * 4, MEMF_PUBLIC | MEMF_CLEAR | MEMF_REVERSE);
            CopyMemQuick((APTR)vbr_old, vbr_new, 256 * 4);

            APTR old_stack = SuperState();

            asm volatile("movec %0, vbr"::"r"(vbr_new));

            if (old_stack != NULL)
                UserState(old_stack);
            
            RawDoFmt("[68040] VBR moved to %08lx\n", &vbr_new, (APTR)putch, NULL);
        }

        binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
        binding.cb_ConfigDev->cd_Driver = LibraryBase;
    }

    CloseLibrary((struct Library*)ExpansionBase);

    return LibraryBase;
}
