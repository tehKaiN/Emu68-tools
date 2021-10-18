/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

/* THIS CODE IS PARTIALLY BASED ON WORK OF JOHN CRONIN */
/* Copyright (C) 2013 by John Cronin <jncronin@tysos.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


#include <exec/types.h>
#include <exec/execbase.h>
#include <exec/io.h>
#include <exec/errors.h>
#include <exec/lists.h>
#include <exec/nodes.h>
#include <exec/memory.h>
#include <inline/alib.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include "sdcard.h"
#include "emmc.h"
#include "mbox.h"

ULONG SD_Expunge(struct SDCardBase * SDCardBase asm("a6"))
{
    /* This is rom based device. no expunge here */
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

static void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

void SD_Open(struct IORequest * io asm("a1"), LONG unitNumber asm("d0"),
    ULONG flags asm("d1"))
{
    struct SDCardBase *SDCardBase = (struct SDCardBase *)io->io_Device;
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    (void)flags;

#if 0
    {
        ULONG args[] = {
            (ULONG)io, unitNumber, flags
        };
        RawDoFmt("[brcm-sdhc] Open(%08lx, %lx, %ld)\n", args, (APTR)putch, NULL);
    }
#endif

    io->io_Error = 0;

    /* Do not continue if unit number does not fit */
    if (unitNumber >= SDCardBase->sd_UnitCount) {
        io->io_Error = IOERR_OPENFAIL;
    }

    /* 
        Do whatever necessary to open given unit number with flags, set NT_REPLYMSG if 
        opening device shall complete with success, set io_Error otherwise
    */

    if (io->io_Error == 0)
    {
        /* Get unit based on unit number */
        struct SDCardUnit *u = SDCardBase->sd_Units[unitNumber];

        /* Increase open counter of the unit */
        u->su_Unit.unit_OpenCnt++;
      
        /* Increase global open coutner of the device */
        SDCardBase->sd_Device.dd_Library.lib_OpenCnt++;
        SDCardBase->sd_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

        io->io_Unit = &u->su_Unit;
        io->io_Unit->unit_flags |= UNITF_ACTIVE;
        io->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    }
    else
    {
        io->io_Error = IOERR_OPENFAIL;
    }
    
    /* In contrast to normal library there is no need to return anything */
    return;
}

ULONG SD_Close(struct IORequest * io asm("a1"))
{
    struct SDCardBase *SDCardBase = (struct SDCardBase *)io->io_Device;
    struct SDCardUnit *u = (struct SDCardUnit *)io->io_Unit;

    u->su_Unit.unit_OpenCnt--;
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

void SD_BeginIO(struct IORequest *io asm("a1"));
LONG SD_AbortIO(struct IORequest *io asm("a1"));

void delay(ULONG us, struct SDCardBase *SDCardBase)
{
    ULONG timer = LE32(*(volatile ULONG*)0xf2003004);
    ULONG end = timer + us;

    if (end < timer) {
        while (end < LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
    }
    while (end > LE32(*(volatile ULONG*)0xf2003004)) asm volatile("nop");
#if 0
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    SDCardBase->sd_Port.mp_SigTask = FindTask(NULL);
    SDCardBase->sd_TimeReq.tr_time.tv_micro = us % 1000000;
    SDCardBase->sd_TimeReq.tr_time.tv_secs = us / 1000000;
    SDCardBase->sd_TimeReq.tr_node.io_Command = TR_ADDREQUEST;

    DoIO((struct IORequest *)&SDCardBase->sd_TimeReq);
#endif
}

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct SDCardBase *SDCardBase = NULL;
    struct CurrentBinding binding;

    RawDoFmt("[brcm-sdhc] Init\n", NULL, (APTR)putch, NULL);

    DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase != NULL)
    {
        APTR base_pointer = NULL;
    
        ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
        GetCurrentBinding(&binding, sizeof(binding));

        base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

        if (base_pointer != NULL)
        {
            APTR key;
            ULONG relFuncTable[] = {
                (ULONG)SD_Open + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                (ULONG)SD_Close + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                (ULONG)SD_Expunge + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                (ULONG)SD_ExtFunc + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                (ULONG)SD_BeginIO + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                (ULONG)SD_AbortIO + (ULONG)binding.cb_ConfigDev->cd_BoardAddr,
                -1
            };
            
            SDCardBase = (struct SDCardBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            MakeFunctions(SDCardBase, relFuncTable, 0);

            SDCardBase->sd_ConfigDev = binding.cb_ConfigDev;
            SDCardBase->sd_ROMBase = binding.cb_ConfigDev->cd_BoardAddr;

            SDCardBase->sd_RequestBase = AllocMem(256*4, MEMF_FAST);
            SDCardBase->sd_Request = (ULONG *)(((intptr_t)SDCardBase->sd_RequestBase + 127) & ~127);

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

            InitSemaphore(&SDCardBase->sd_Lock);
            SDCardBase->sd_Port.mp_Flags = PA_SIGNAL;
            SDCardBase->sd_Port.mp_SigBit = SIGBREAKB_CTRL_C;
            NewList(&SDCardBase->sd_Port.mp_MsgList);

            SDCardBase->sd_TimeReq.tr_node.io_Message.mn_ReplyPort = &SDCardBase->sd_Port;
            OpenDevice("timer.device", UNIT_MICROHZ, (struct IORequest *)&SDCardBase->sd_TimeReq, 0);

            SDCardBase->sd_DoIO = (APTR)((ULONG)int_do_io + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            extern const UWORD NSDSupported[];
            SDCardBase->sd_NSDSupported = (APTR)((ULONG)NSDSupported + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

            /* Set MBOX functions in device base */
            SDCardBase->get_clock_rate = (APTR)((ULONG)get_clock_rate + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->set_clock_rate = (APTR)((ULONG)set_clock_rate + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->get_clock_state = (APTR)((ULONG)get_clock_state + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->set_clock_state = (APTR)((ULONG)set_clock_state + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->get_power_state = (APTR)((ULONG)get_power_state + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->set_power_state = (APTR)((ULONG)set_power_state + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->set_led_state = (APTR)((ULONG)set_led_state + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

            /* Set SD functions in device base */
            SDCardBase->sd_PowerCycle = (APTR)((ULONG)powerCycle + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_SetLED = (APTR)((ULONG)led + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_Delay = (APTR)((ULONG)delay + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_GetBaseClock = (APTR)((ULONG)getclock + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_CMD = (APTR)((ULONG)cmd + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_CMD_int = (APTR)((ULONG)cmd_int + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_CardInit = (APTR)((ULONG)sd_card_init + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_Read = (APTR)((ULONG)sd_read + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_Write = (APTR)((ULONG)sd_write + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

            SumLibrary((struct Library*)SDCardBase);

            RawDoFmt("[brcm-sdhc] DeviceBase at %08lx\n", &SDCardBase, (APTR)putch, NULL);

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

                        SDCardBase->sd_MailBox = (APTR)reg[address_cells - 1];

                        DT_CloseKey(key);
                    }
                }
            }

            /* Open /aliases and find out the "link" to the emmc */
            key = DT_OpenKey("/aliases");
            if (key)
            {
                CONST_STRPTR mmc_alias = DT_GetPropValue(DT_FindProperty(key, "mmc"));

                DT_CloseKey(key);
               
                if (mmc_alias != NULL)
                {
                    /* Open the alias and find out the MMIO VC4 physical base address */
                    key = DT_OpenKey(mmc_alias);
                    if (key) {
                        int size_cells = 1;
                        int address_cells = 1;

                        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                        if (siz != NULL)
                            size_cells = *siz;
                        
                        if (addr != NULL)
                            address_cells = *addr;

                        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                        SDCardBase->sd_SDHC = (APTR)reg[address_cells - 1];
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

                SDCardBase->sd_MailBox = (APTR)((ULONG)SDCardBase->sd_MailBox - phys_vc4 + phys_cpu);
                SDCardBase->sd_SDHC = (APTR)((ULONG)SDCardBase->sd_SDHC - phys_vc4 + phys_cpu);

                RawDoFmt("[brcm-sdhc] Mailbox at %08lx\n", &SDCardBase->sd_MailBox, (APTR)putch, NULL);
                RawDoFmt("[brcm-sdhc] SDHC regs at %08lx\n", &SDCardBase->sd_SDHC, (APTR)putch, NULL);

                DT_CloseKey(key);
            }

            /* If both sd_MailBox and sd_SDHC are set, everything went OK and now we can add the device */
            if (SDCardBase->sd_MailBox != NULL && SDCardBase->sd_SDHC != NULL)
            {
                AddDevice((struct Device *)SDCardBase);

                /* Turn the power/act led off */
                SDCardBase->sd_SetLED(0, SDCardBase);

                /* Select AF3 on GPIOs 48..53 */
                ULONG tmp = rd32((APTR)0xf2200000, 0x10);
                tmp &= 0xffffff;
                tmp |= 0x3f000000;
                
                wr32((APTR)0xf2200000, 0x10, tmp);
                wr32((APTR)0xf2200000, 0x14, 0xfff);
              
                /* Enable EMMC clock */
                SDCardBase->set_clock_state(1, 1, SDCardBase);
                                
                /* Initialize the card */
                if (0 == SDCardBase->sd_CardInit(SDCardBase))
                {
                    uint8_t buff[512];
                    /* Initializataion was successful. Read parition table and create the units */

                    RawDoFmt("[brcm-sdhc] Attempting to read card capacity\n", NULL, (APTR)putch, NULL);
                    SDCardBase->sd_CMD(DESELECT_CARD, 0, 1000000, SDCardBase);
                    SDCardBase->sd_CMD(SEND_CSD, SDCardBase->sd_CardRCA << 16, 1000000, SDCardBase);

                    if (!FAIL(SDCardBase)) {
                        SDCardBase->sd_UnitCount = 1;
                        SDCardBase->sd_Units[0] = AllocMem(sizeof(struct SDCardUnit), MEMF_PUBLIC | MEMF_CLEAR);
                        SDCardBase->sd_Units[0]->su_StartBlock = 0;
                        SDCardBase->sd_Units[0]->su_BlockCount = 1024 * ((SDCardBase->sd_Res1 >> 8) & 0x3fffff) + 1024;
                        SDCardBase->sd_Units[0]->su_Base = SDCardBase;
                        SDCardBase->sd_Units[0]->su_UnitNum = 0;

                        ULONG size_gb = SDCardBase->sd_Units[0]->su_BlockCount / 2048;
                        RawDoFmt("[brcm-sdhc] Reported card capacity: %ld MB", &size_gb, (APTR)putch, NULL);
                        RawDoFmt(" (%ld sectors)\n", &SDCardBase->sd_Units[0]->su_BlockCount, (APTR)putch, NULL);

                        RawDoFmt("[brcm-sdhc] Attempting to read block at 0\n", NULL, (APTR)putch, NULL);
                        ULONG ret = SDCardBase->sd_Read(buff, 512, 0, SDCardBase);
                        RawDoFmt("[brcm-sdhc] Result %ld\n", &ret, (APTR)putch, NULL);

                        if (ret > 0)
                        {
                            for (int i=0; i < 4; i++) {
                                uint8_t *b = &buff[0x1be + 16 * i];
                                ULONG p0_Type = b[4];
                                ULONG p0_Start = b[8] | (b[9] << 8) | (b[10] << 16) | (b[11] << 24);
                                ULONG p0_Len = b[12] | (b[13] << 8) | (b[14] << 16) | (b[15] << 24);

                                // Partition does exist. List it.
                                if (p0_Type != 0) {
                                    ULONG args[] = {
                                        i, p0_Type, p0_Start, p0_Len 
                                    };
                                    RawDoFmt("[brcm-sdhc] Partition%ld: type 0x%02lx, start 0x%08lx, length 0x%08lx\n", args, (APTR)putch, NULL);

                                    // Partition type 0x76 (Amithlon-like) creates new virtual unit with given capacity
                                    if (p0_Type == 0x76) {
                                        struct SDCardUnit *unit = AllocMem(sizeof(struct SDCardUnit), MEMF_PUBLIC | MEMF_CLEAR);
                                        unit->su_StartBlock = p0_Start;
                                        unit->su_BlockCount = p0_Len;
                                        unit->su_Base = SDCardBase;
                                        unit->su_UnitNum = SDCardBase->sd_UnitCount;
                                        
                                        SDCardBase->sd_Units[SDCardBase->sd_UnitCount++] = unit;
                                    }
                                }
                            }
                        }
                    }
                  
                    RawDoFmt("[brcm-sdhc] Init complete.\n", NULL, (APTR)putch, NULL);

                    /* Initialization is complete. Create all tasks for the units now */
                    for (int unit = 0; unit < SDCardBase->sd_UnitCount; unit++)
                    {
                        APTR entry = (APTR)((ULONG)UnitTask + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);;
                        struct Task *task = AllocMem(sizeof(struct Task), MEMF_PUBLIC | MEMF_CLEAR);
                        struct MemList *ml = AllocMem(sizeof(struct MemList) + 2*sizeof(struct MemEntry), MEMF_PUBLIC | MEMF_CLEAR);
                        ULONG *stack = AllocMem(UNIT_TASK_STACKSIZE * sizeof(ULONG), MEMF_PUBLIC | MEMF_CLEAR);
                        const char unit_name[] = "brcm-sdhc unit 0";
                        STRPTR unit_name_copy = AllocMem(sizeof(unit_name), MEMF_PUBLIC | MEMF_CLEAR);
                        CopyMem(unit_name, unit_name_copy, sizeof(unit_name));

                        ml->ml_NumEntries = 3;
                        ml->ml_ME[0].me_Un.meu_Addr = task;
                        ml->ml_ME[0].me_Length = sizeof(struct Task);

                        ml->ml_ME[1].me_Un.meu_Addr = &stack[0];
                        ml->ml_ME[1].me_Length = UNIT_TASK_STACKSIZE * sizeof(ULONG);

                        ml->ml_ME[2].me_Un.meu_Addr = unit_name_copy;
                        ml->ml_ME[2].me_Length = sizeof(unit_name);

                        unit_name_copy[sizeof(unit_name) - 2] += unit;

                        task->tc_UserData = SDCardBase->sd_Units[unit];
                        task->tc_SPLower = &stack[0];
                        task->tc_SPUpper = &stack[UNIT_TASK_STACKSIZE];
                        task->tc_SPReg = task->tc_SPUpper;

                        task->tc_Node.ln_Name = unit_name_copy;
                        task->tc_Node.ln_Type = NT_TASK;
                        task->tc_Node.ln_Pri = UNIT_TASK_PRIORITY;

                        NewList(&task->tc_MemEntry);
                        AddHead(&task->tc_MemEntry, &ml->ml_Node);

                        SDCardBase->sd_Units[unit]->su_Caller = FindTask(NULL);

                        AddTask(task, entry, NULL);
                        Wait(SIGBREAKF_CTRL_C);
                    }

                }
                
                binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
                binding.cb_ConfigDev->cd_Driver = SDCardBase;
            }
            else
            {
                /*  
                    Something failed, device will not be added to the system, free allocated memory and 
                    return NULL instead
                */
                FreeMem(base_pointer, BASE_NEG_SIZE + BASE_POS_SIZE);
                SDCardBase = NULL;
            }
        }

        CloseLibrary((struct Library*)ExpansionBase);
    }

    return SDCardBase;
}
