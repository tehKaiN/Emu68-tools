/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/


#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/errors.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include "sdcard.h"

extern UWORD relFuncTable[];
asm(
"       .globl _relFuncTable    \n"
"_relFuncTable:                 \n"
"       .short _SD_Open         \n"
"       .short _SD_Close        \n"
"       .short _SD_Expunge      \n"
"       .short _SD_ExtFunc      \n"
"       .short _SD_BeginIO      \n"
"       .short _SD_AbortIO      \n"
"       .short -1               \n"
);

ULONG SD_Expunge(struct SDCardBase * SDCardBase asm("a6"))
{
    if (SDCardBase->sd_Device.dd_Library.lib_OpenCnt > 0)
    {
        SDCardBase->sd_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
    }

    return 0;
}

APTR SD_ExtFunc(struct SDCardBase * SDCardBase asm("a6"))
{
    return SDCardBase;
}

void SD_Open(struct IORequest * io asm("a1"), LONG unitNumber asm("d0"),
    ULONG flags asm("d1"), struct SDCardBase * SDCardBase asm("a6"))
{
    (void)flags;
    (void)unitNumber;

    /* 
        Do whatever necessary to open given unit number with flags, set NT_REPLYMSG if 
        opening device shall complete with success, set io_Error otherwise
    */

    if (io->io_Error == 0)
    {
        SDCardBase->sd_Device.dd_Library.lib_OpenCnt++;
        SDCardBase->sd_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

        io->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    }
    else
    {
        io->io_Error = IOERR_OPENFAIL;
    }
    
    /* In contrast to normal library there is no need to return anything */
    return;
}

ULONG SD_Close(struct IORequest * io asm("a1"), struct SDCardBase * SDCardBase asm("a6"))
{
    (void)io;

    SDCardBase->sd_Device.dd_Library.lib_OpenCnt--;

    if (SDCardBase->sd_Device.dd_Library.lib_OpenCnt == 0)
    {
        if (SDCardBase->sd_Device.dd_Library.lib_Flags & LIBF_DELEXP)
        {
            return SD_Expunge(SDCardBase);
        }
    }
    
    return 0;
}

extern const char deviceName[];
extern const char deviceIdString[];

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct SDCardBase *SDCardBase = NULL;
    struct CurrentBinding binding;

    DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase != NULL)
    {
        APTR base_pointer = NULL;
    
        ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
        GetCurrentBinding(&binding, sizeof(binding));

        base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

        if (base_pointer != NULL)
        {
            SDCardBase = (struct SDCardBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            MakeFunctions(SDCardBase, relFuncTable, (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Type = NT_DEVICE;
            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Pri = SDCARD_PRIORITY;
            SDCardBase->sd_Device.dd_Library.lib_Node.ln_Name = (STRPTR)((ULONG)deviceName + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

            SDCardBase->sd_Device.dd_Library.lib_NegSize = BASE_NEG_SIZE;
            SDCardBase->sd_Device.dd_Library.lib_PosSize = BASE_POS_SIZE;
            SDCardBase->sd_Device.dd_Library.lib_Version = SDCARD_VERSION;
            SDCardBase->sd_Device.dd_Library.lib_Revision = SDCARD_REVISION;
            SDCardBase->sd_Device.dd_Library.lib_IdString = (STRPTR)((ULONG)deviceIdString + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);    

            SDCardBase->sd_SysBase = SysBase;
            SDCardBase->sd_DeviceTreeBase = DeviceTreeBase;

            SumLibrary((struct Library*)SDCardBase);
            AddDevice((struct Device *)SDCardBase);

            binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
            binding.cb_ConfigDev->cd_Driver = SDCardBase;

            /* Get VC4 physical address of mailbox interface. Subsequently it will be translated to m68k physical address */

            APTR mbox = DT_OpenKey("/soc/mailbox");
            if (mbox)
            {
                APTR key = mbox;
                int size_cells = 0;
                int address_cells = 0;

                do {
                    const ULONG *siz = DT_GetPropValue(DT_FindProperty(key, "#size-cells"));
                    const ULONG *addr = DT_GetPropValue(DT_FindProperty(key, "#address-cells"));

                    if (siz != NULL)
                        size_cells = *siz;

                    if (addr != NULL)
                        address_cells = *addr;

                    key = DT_GetParent(key);
                } while (key && (size_cells == 0 || address_cells == 0));

                if (size_cells == 0)
                    size_cells = 1;
                if (address_cells == 0)
                    address_cells = 1;

                const ULONG *reg = DT_GetPropValue(DT_FindProperty(mbox, "reg"));

                SDCardBase->sd_MailBox = (APTR)reg[address_cells - 1];

                DT_CloseKey(mbox);
            }

            /* Open /aliases and find out the "link" to the emmc alias */
            APTR aliases = DT_OpenKey("/aliases");
            if (aliases)
            {
                CONST_STRPTR mmc_alias = DT_GetPropValue(DT_FindProperty(aliases, "mmc"));
                
                /* Open the alias and find out the MMIO VC4 physical base address */
                APTR mmc = DT_OpenKey(mmc_alias);
                if (mmc) {
                    int size_cells = 0;
                    int address_cells = 0;
                    APTR key = mmc;

                    do {
                        const ULONG *siz = DT_GetPropValue(DT_FindProperty(key, "#size-cells"));
                        const ULONG *addr = DT_GetPropValue(DT_FindProperty(key, "#address-cells"));

                        if (siz != NULL)
                            size_cells = *siz;

                        if (addr != NULL)
                            address_cells = *addr;

                        key = DT_GetParent(key);
                    } while (key && (size_cells == 0 || address_cells == 0));

                    if (size_cells == 0)
                        size_cells = 1;
                    if (address_cells == 0)
                        address_cells = 1;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(mmc, "reg"));
                    SDCardBase->sd_SDHC = (APTR)reg[address_cells - 1];
                    DT_CloseKey(mmc);
                }
                DT_CloseKey(aliases);
            }

            mbox = DT_OpenKey("/soc");
            if (mbox)
            {
                APTR key = mbox;
                int size_cells = 0;
                int address_cells = 0;

                do {
                    const ULONG *siz = DT_GetPropValue(DT_FindProperty(key, "#size-cells"));
                    const ULONG *addr = DT_GetPropValue(DT_FindProperty(key, "#address-cells"));

                    if (siz != NULL)
                        size_cells = *siz;

                    if (addr != NULL)
                        address_cells = *addr;

                    key = DT_GetParent(key);
                } while (key && (size_cells == 0 || address_cells == 0));

                if (size_cells == 0)
                    size_cells = 1;
                if (address_cells == 0)
                    address_cells = 1;

                const ULONG *reg = DT_GetPropValue(DT_FindProperty(mbox, "ranges"));

                ULONG phys_vc4 = reg[address_cells - 1];
                ULONG phys_cpu = reg[2 * address_cells - 1];

                SDCardBase->sd_MailBox = (APTR)((ULONG)SDCardBase->sd_MailBox - phys_vc4 + phys_cpu);
                SDCardBase->sd_SDHC = (APTR)((ULONG)SDCardBase->sd_SDHC - phys_vc4 + phys_cpu);

                DT_CloseKey(mbox);
            }

            asm volatile("move.l %0, d0; move.l %1, d1; illegal"::"r"(SDCardBase->sd_SDHC),"r"(SDCardBase->sd_MailBox):"d0","d1","memory");
        }

        CloseLibrary((struct Library*)ExpansionBase);
    }

    return SDCardBase;
}
