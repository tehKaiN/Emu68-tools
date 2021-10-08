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
#include <inline/alib.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/devicetree.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include "sdcard.h"
#include "mbox.c"

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

void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

static inline ULONG rd32(APTR addr, ULONG offset)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    return LE32(*(volatile ULONG *)addr_off);
    asm volatile("nop");
}

static inline void wr32(APTR addr, ULONG offset, ULONG val)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    *(volatile ULONG *)addr_off = LE32(val);
    asm volatile("nop");
}

int powerCycle(struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;

    RawDoFmt("[brcm-sdhc] powerCycle\n", NULL, (APTR)putch, NULL);

    RawDoFmt("[brcm-sdhc]   power OFF\n", NULL, (APTR)putch, NULL);
    set_power_state(0, 2, SDCardBase);

    SDCardBase->sd_Delay(500000, SDCardBase);

    RawDoFmt("[brcm-sdhc]   power ON\n", NULL, (APTR)putch, NULL);
    return set_power_state(0, 3, SDCardBase);
}

void led(int on, struct SDCardBase *SDCardBase)
{
    if (on) {
        set_led_state(130, 0, SDCardBase);
    }
    else {
        set_led_state(130, 1, SDCardBase);
    }
}

void delay(ULONG us, struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    SDCardBase->sd_Port.mp_SigTask = FindTask(NULL);
    SDCardBase->sd_TimeReq.tr_time.tv_micro = us % 1000000;
    SDCardBase->sd_TimeReq.tr_time.tv_secs = us / 1000000;
    SDCardBase->sd_TimeReq.tr_node.io_Command = TR_ADDREQUEST;

    DoIO((struct IORequest *)&SDCardBase->sd_TimeReq);
}

void cmd_int(ULONG cmd, ULONG arg, ULONG timeout, struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;

    ULONG tout = 0;
    SDCardBase->sd_LastCMDSuccess = 0;

    // Check Command Inhibit
    while(rd32(SDCardBase->sd_SDHC, EMMC_STATUS) & 0x1)
        SDCardBase->sd_Delay(1000, SDCardBase);

    // Is the command with busy?
    if((cmd & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B)
    {
        // With busy

        // Is is an abort command?
        if((cmd & SD_CMD_TYPE_MASK) != SD_CMD_TYPE_ABORT)
        {
            // Not an abort command

            // Wait for the data line to be free
            while(rd32(SDCardBase->sd_SDHC, EMMC_STATUS) & 0x2)
                SDCardBase->sd_Delay(1000, SDCardBase);
        }
    }

    uint32_t blksizecnt = 512 | (SDCardBase->sd_BlocksToTransfer << 16);

    wr32(SDCardBase->sd_SDHC, EMMC_BLKSIZECNT, blksizecnt);

    // Set argument 1 reg
    wr32(SDCardBase->sd_SDHC, EMMC_ARG1, arg);

    {
        ULONG args[] = {cmd, arg};
        RawDoFmt("[brcm-sdhc] sending command %08lx, arg %08lx\n", args, (APTR)putch, NULL);
    }

    // Set command reg
    wr32(SDCardBase->sd_SDHC, EMMC_CMDTM, cmd);

    SDCardBase->sd_Delay(2000, SDCardBase);

    // Wait for command complete interrupt
    tout = timeout / 100;
    while(!(rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT) & 0x8001) && tout) {
        tout--;
        SDCardBase->sd_Delay(100, SDCardBase);
    }
    uint32_t irpts = rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT);

    // Clear command complete status
    wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffff0001);

        // Test for errors
    if((irpts & 0xffff0001) != 0x1)
    {
        RawDoFmt("[brcm-sdhc] error occured whilst waiting for command complete interrupt\n", NULL, (APTR)putch, NULL);

        SDCardBase->sd_LastError = irpts & 0xffff0000;
        SDCardBase->sd_LastInterrupt = irpts;
        return;
    }

    SDCardBase->sd_Delay(2000, SDCardBase);

    // Get response data
    switch(cmd & SD_CMD_RSPNS_TYPE_MASK)
    {
        case SD_CMD_RSPNS_TYPE_48:
        case SD_CMD_RSPNS_TYPE_48B:
            SDCardBase->sd_Res0 = rd32(SDCardBase, EMMC_RESP0);
            break;

        case SD_CMD_RSPNS_TYPE_136:
            SDCardBase->sd_Res0 = rd32(SDCardBase, EMMC_RESP0);
            SDCardBase->sd_Res1 = rd32(SDCardBase, EMMC_RESP1);
            SDCardBase->sd_Res2 = rd32(SDCardBase, EMMC_RESP2);
            SDCardBase->sd_Res3 = rd32(SDCardBase, EMMC_RESP3);
            break;
    }

    // If with data, wait for the appropriate interrupt
    if(cmd & SD_CMD_ISDATA)
    {
        uint32_t wr_irpt;
        int is_write = 0;
        if(cmd & SD_CMD_DAT_DIR_CH)
            wr_irpt = (1 << 5);     // read
        else
        {
            is_write = 1;
            wr_irpt = (1 << 4);     // write
        }

        int cur_block = 0;
        uint32_t *cur_buf_addr = (uint32_t *)SDCardBase->sd_Buffer;
        while(cur_block < SDCardBase->sd_BlocksToTransfer)
        {
            tout = timeout / 100;
            while(tout && (rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT) & (wr_irpt | 0x8000))) {
                tout--;
                SDCardBase->sd_Delay(100, SDCardBase);
            }
            irpts = rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT);
            wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffff0000 | wr_irpt);

            if((irpts & (0xffff0000 | wr_irpt)) != wr_irpt)
            {
                RawDoFmt("[brcm-sdhc] error occured whilst waiting for data ready interrupt\n", NULL, (APTR)putch, NULL);

                SDCardBase->sd_LastError = irpts & 0xffff0000;
                SDCardBase->sd_LastInterrupt = irpts;
                return;
            }

            // Transfer the block
            UWORD cur_byte_no = 0;
            while(cur_byte_no < 512)
            {
                if(is_write)
				{
					uint32_t data = *(ULONG*)cur_buf_addr;
                    wr32(SDCardBase->sd_SDHC, EMMC_DATA, data);
				}
                else
				{
					uint32_t data = rd32(SDCardBase->sd_SDHC, EMMC_DATA);
					*(ULONG*)cur_buf_addr = data;
				}
                cur_byte_no += 4;
                cur_buf_addr++;
            }

            cur_block++;
        }
    }

    // Wait for transfer complete (set if read/write transfer or with busy)
    if((((cmd & SD_CMD_RSPNS_TYPE_MASK) == SD_CMD_RSPNS_TYPE_48B) ||
       (cmd & SD_CMD_ISDATA)))
    {
        // First check command inhibit (DAT) is not already 0
        if((rd32(SDCardBase->sd_SDHC, EMMC_STATUS) & 0x2) == 0)
            wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffff0002);
        else
        {
            tout = timeout / 100;
            while(tout && !(rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT) & 0x8002)) {
                tout--;
                SDCardBase->sd_Delay(100, SDCardBase);
            }
            irpts = rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT);
            wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffff0002);

            // Handle the case where both data timeout and transfer complete
            //  are set - transfer complete overrides data timeout: HCSS 2.2.17
            if(((irpts & 0xffff0002) != 0x2) && ((irpts & 0xffff0002) != 0x100002))
            {
                RawDoFmt("[brcm-sdhc] error occured whilst waiting for transfer complete interrupt\n", NULL, (APTR)putch, NULL);
                SDCardBase->sd_LastError = irpts & 0xffff0000;
                SDCardBase->sd_LastInterrupt = irpts;
                return;
            }
            wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffff0002);
        }
    }


    SDCardBase->sd_LastCMDSuccess = 1;
}


// Reset the CMD line
static int sd_reset_cmd(struct SDCardBase *SDCardBase)
{
    int tout = 10000;
    uint32_t control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
	control1 |= SD_RESET_CMD;
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
	while (tout && (rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & SD_RESET_CMD) != 0) {
        SDCardBase->sd_Delay(100, SDCardBase);
        tout--;
    }
	if((rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & SD_RESET_CMD) != 0)
	{
		return -1;
	}
	return 0;
}

// Reset the CMD line
static int sd_reset_dat(struct SDCardBase *SDCardBase)
{
    int tout = 10000;
    uint32_t control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
	control1 |= SD_RESET_DAT;
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
	while (tout && (rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & SD_RESET_DAT) != 0) {
        SDCardBase->sd_Delay(100, SDCardBase);
        tout--;
    }
	if((rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & SD_RESET_DAT) != 0)
	{
		return -1;
	}
	return 0;
}

static void sd_handle_card_interrupt(struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;

    // Handle a card interrupt

    // Get the card status
    if(SDCardBase->sd_CardRCA)
    {
        cmd_int(SEND_STATUS, SDCardBase->sd_CardRCA << 16, 500000, SDCardBase);
        if(FAIL(SDCardBase))
        {
        }
        else
        {
        }
    }
    else
    {
    }
}

static void sd_handle_interrupts(struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    uint32_t irpts = rd32(SDCardBase->sd_SDHC, EMMC_INTERRUPT);
    uint32_t reset_mask = 0;

    if(irpts & SD_COMMAND_COMPLETE)
    {
        RawDoFmt("[brcm-sdhc] spurious command complete interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_COMMAND_COMPLETE;
    }

    if(irpts & SD_TRANSFER_COMPLETE)
    {
        RawDoFmt("[brcm-sdhc] spurious transfer complete interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_TRANSFER_COMPLETE;
    }

    if(irpts & SD_BLOCK_GAP_EVENT)
    {
        RawDoFmt("[brcm-sdhc] spurious block gap event interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_BLOCK_GAP_EVENT;
    }

    if(irpts & SD_DMA_INTERRUPT)
    {
        RawDoFmt("[brcm-sdhc] spurious DMA interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_DMA_INTERRUPT;
    }

    if(irpts & SD_BUFFER_WRITE_READY)
    {
        RawDoFmt("[brcm-sdhc] spurious buffer write ready interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_BUFFER_WRITE_READY;
        sd_reset_dat(SDCardBase);
    }

    if(irpts & SD_BUFFER_READ_READY)
    {
        RawDoFmt("[brcm-sdhc] spurious buffer read ready interrupt\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_BUFFER_READ_READY;
        sd_reset_dat(SDCardBase);
    }

    if(irpts & SD_CARD_INSERTION)
    {
        RawDoFmt("[brcm-sdhc] card insertion detected\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_CARD_INSERTION;
    }

    if(irpts & SD_CARD_REMOVAL)
    {
        RawDoFmt("[brcm-sdhc] card removal detected\n", NULL, (APTR)putch, NULL);
        reset_mask |= SD_CARD_REMOVAL;
        SDCardBase->sd_CardRemoval = 1;
    }

    if(irpts & SD_CARD_INTERRUPT)
    {
        sd_handle_card_interrupt(SDCardBase);
        reset_mask |= SD_CARD_INTERRUPT;
    }

    if(irpts & 0x8000)
    {
        reset_mask |= 0xffff0000;
    }

    wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, reset_mask);
}

void cmd(ULONG command, ULONG arg, ULONG timeout, struct SDCardBase *SDCardBase)
{
    // First, handle any pending interrupts
    sd_handle_interrupts(SDCardBase);

    // Stop the command issue if it was the card remove interrupt that was
    //  handled
    if(SDCardBase->sd_CardRemoval)
    {
        SDCardBase->sd_LastCMDSuccess = 0;
        return;
    }

    // Now run the appropriate commands by calling sd_issue_command_int()
    if(command & IS_APP_CMD)
    {
        command &= 0x7fffffff;

        SDCardBase->sd_LastCMD = APP_CMD;

        uint32_t rca = 0;
        if(SDCardBase->sd_CardRCA)
            rca = SDCardBase->sd_CardRCA << 16;
        cmd_int(APP_CMD, rca, timeout, SDCardBase);
        if(SDCardBase->sd_LastCMDSuccess)
        {
            SDCardBase->sd_LastCMD = command | IS_APP_CMD;
            cmd_int(command, arg, timeout, SDCardBase);
        }
    }
    else
    {
        SDCardBase->sd_LastCMD = command;
        cmd_int(command, arg, timeout, SDCardBase);
    }
}

// Set the clock dividers to generate a target value
static uint32_t sd_get_clock_divider(uint32_t base_clock, uint32_t target_rate)
{
    // TODO: implement use of preset value registers

    uint32_t targetted_divisor = 0;
    if(target_rate > base_clock)
        targetted_divisor = 1;
    else
    {
        targetted_divisor = base_clock / target_rate;
        uint32_t mod = base_clock % target_rate;
        if(mod)
            targetted_divisor--;
    }

    // Decide on the clock mode to use

    // Currently only 10-bit divided clock mode is supported

    // HCI version 3 or greater supports 10-bit divided clock mode
    // This requires a power-of-two divider

    // Find the first bit set
    int divisor = -1;
    for(int first_bit = 31; first_bit >= 0; first_bit--)
    {
        uint32_t bit_test = (1 << first_bit);
        if(targetted_divisor & bit_test)
        {
            divisor = first_bit;
            targetted_divisor &= ~bit_test;
            if(targetted_divisor)
            {
                // The divisor is not a power-of-two, increase it
                divisor++;
            }
            break;
        }
    }

    if(divisor == -1)
        divisor = 31;
    if(divisor >= 32)
        divisor = 31;

    if(divisor != 0)
        divisor = (1 << (divisor - 1));

    if(divisor >= 0x400)
        divisor = 0x3ff;

    uint32_t freq_select = divisor & 0xff;
    uint32_t upper_bits = (divisor >> 8) & 0x3;
    uint32_t ret = (freq_select << 8) | (upper_bits << 6) | (0 << 5);

    return ret;
}

// Switch the clock rate whilst running
static int sd_switch_clock_rate(uint32_t base_clock, uint32_t target_rate, struct SDCardBase *SDCardBase)
{
    // Decide on an appropriate divider
    uint32_t divider = sd_get_clock_divider(base_clock, target_rate);

    // Wait for the command inhibit (CMD and DAT) bits to clear
    while(rd32(SDCardBase->sd_SDHC, EMMC_STATUS) & 0x3)
        SDCardBase->sd_Delay(1000, SDCardBase);

    // Set the SD clock off
    uint32_t control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
    control1 &= ~(1 << 2);
    wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
    SDCardBase->sd_Delay(2000, SDCardBase);

    // Write the new divider
	control1 &= ~0xffe0;		// Clear old setting + clock generator select
    control1 |= divider;
    wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
    SDCardBase->sd_Delay(2000, SDCardBase);

    // Enable the SD clock
    control1 |= (1 << 2);
    wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
    SDCardBase->sd_Delay(2000, SDCardBase);

    return 0;
}



static int sd_card_init(struct SDCardBase *SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;

    RawDoFmt("[brcm-sdhc] SD Card init\n", NULL, (APTR)putch, NULL);



    uint32_t ver = rd32(SDCardBase->sd_SDHC, EMMC_SLOTISR_VER);
	uint32_t vendor = ver >> 24;
	uint32_t sdversion = (ver >> 16) & 0xff;
	uint32_t slot_status = ver & 0xff;
	
    UWORD args[] = { vendor, sdversion, slot_status };
    RawDoFmt("[brcm-sdhc] EMMC: vendor %x, sdversion %x, slot_status %x\n", args, (APTR)putch, NULL);

    uint32_t control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
	control1 |= (1 << 24);
	// Disable clock
	control1 &= ~(1 << 2);
	control1 &= ~(1 << 0);
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
    int tout = 10000;
    while(tout && (rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & (7 << 24)) != 0) {
        tout--;
        SDCardBase->sd_Delay(100, SDCardBase);
    }
	if((rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & (7 << 24)) != 0)
	{
		RawDoFmt("[brcm-sdhc] EMMC: controller did not reset properly\n", NULL, (APTR)putch, NULL);
		return -1;
	}

    {
        ULONG args[] = {
            rd32(SDCardBase->sd_SDHC, EMMC_CONTROL0),
            rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1),
            rd32(SDCardBase->sd_SDHC, EMMC_CONTROL2),
        };
        RawDoFmt("[brcm-sdhc] EMMC: control0: %08lx, control1: %08lx, control2: %08lx\n", args, (APTR)putch, NULL);
    }

// Read the capabilities registers
	SDCardBase->sd_Capabilities0 = rd32(SDCardBase->sd_SDHC, EMMC_CAPABILITIES_0);
	SDCardBase->sd_Capabilities1 = rd32(SDCardBase->sd_SDHC, EMMC_CAPABILITIES_1);

    tout = 5000;
    while(tout && !(rd32(SDCardBase->sd_SDHC, EMMC_STATUS) & (1 << 16))) {
        tout--;
        SDCardBase->sd_Delay(100, SDCardBase);
    }

	uint32_t status_reg = rd32(SDCardBase->sd_SDHC, EMMC_STATUS);
	if((status_reg & (1 << 16)) == 0)
	{
		RawDoFmt("[brcm-sdhc] EMMC: no card inserted\n", NULL, (APTR)putch, NULL);
		return -1;
	}

	RawDoFmt("[brcm-sdhc] EMMC: status: %08lx\n", &status_reg, (APTR)putch, NULL);

	// Clear control2
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL2, 0);

	// Get the base clock rate
	uint32_t base_clock = SDCardBase->sd_GetBaseClock(SDCardBase);

    RawDoFmt("[brcm-sdhc] Base clock: %ld Hz\n", &base_clock, (APTR)putch, NULL);

	control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
	control1 |= 1;			// enable clock

	// Set to identification frequency (400 kHz)
	uint32_t f_id = sd_get_clock_divider(base_clock, SD_CLOCK_ID);

	control1 |= f_id;

	control1 |= (7 << 16);		// data timeout = TMCLK * 2^10
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
    tout = 10000;
    while(tout && !(rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & 0x2)) {
        tout--;
        SDCardBase->sd_Delay(100, SDCardBase);
    }
	if((rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1) & 0x2) == 0)
	{
		RawDoFmt("[brcm-sdhc] EMMC: controller's clock did not stabilise within 1 second\n", NULL, (APTR)putch, NULL);
		return -1;
	}

    {
        ULONG args[] = {
            rd32(SDCardBase->sd_SDHC, EMMC_CONTROL0),
            rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1)
        };
        RawDoFmt("[brcm-sdhc] EMMC: control0: %08lx, control1: %08lx\n", args, (APTR)putch, NULL);
    }

	// Enable the SD clock
    SDCardBase->sd_Delay(2000, SDCardBase);
	control1 = rd32(SDCardBase->sd_SDHC, EMMC_CONTROL1);
	control1 |= 4;
	wr32(SDCardBase->sd_SDHC, EMMC_CONTROL1, control1);
	SDCardBase->sd_Delay(2000, SDCardBase);

	// Mask off sending interrupts to the ARM
	wr32(SDCardBase->sd_SDHC, EMMC_IRPT_EN, 0);
	// Reset interrupts
	wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, 0xffffffff);
	// Have all interrupts sent to the INTERRUPT register
	uint32_t irpt_mask = 0xffffffff & (~SD_CARD_INTERRUPT);
#ifdef SD_CARD_INTERRUPTS
    irpt_mask |= SD_CARD_INTERRUPT;
#endif
	wr32(SDCardBase->sd_SDHC, EMMC_IRPT_MASK, irpt_mask);

	SDCardBase->sd_Delay(2000, SDCardBase);

	// Send CMD0 to the card (reset to idle state)
	SDCardBase->sd_CMD(GO_IDLE_STATE, 0, 500000, SDCardBase);
	if(FAIL(SDCardBase))
	{
        RawDoFmt("[brcm-sdhc] SD: no CMD0 response\n", NULL, (APTR)putch, NULL);
        return -1;
	}

    // Send CMD8 to the card
	// Voltage supplied = 0x1 = 2.7-3.6V (standard)
	// Check pattern = 10101010b (as per PLSS 4.3.13) = 0xAA

    RawDoFmt("[brcm-sdhc] note a timeout error on the following command (CMD8) is normal "
           "and expected if the SD card version is less than 2.0\n", NULL, (APTR)putch, NULL);

    SDCardBase->sd_CMD(SEND_IF_COND, 0x1aa, 500000, SDCardBase);

	int v2_later = 0;
	if(TIMEOUT(SDCardBase))
        v2_later = 0;
    else if(CMD_TIMEOUT(SDCardBase))
    {
        if(sd_reset_cmd(SDCardBase) == -1)
            return -1;
        wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        v2_later = 0;
    }
    else if(FAIL(SDCardBase))
    {
        RawDoFmt("[brcm-sdhc] failure sending CMD8 (%08lx)\n", &SDCardBase->sd_LastInterrupt, (APTR)putch, NULL);
        return -1;
    }
    else
    {
        if(((SDCardBase->sd_Res0) & 0xfff00) != 0x1aa00)
        {
            RawDoFmt("[brcm-sdhc] unusable card\n", NULL, (APTR)putch, NULL);
            RawDoFmt("[brcm-sdhc] CMD8 response %08lx\n", &SDCardBase->sd_Res0, (APTR)putch, NULL);
            //return -1;
        }
        else
            v2_later = 1;
    }

    // Here we are supposed to check the response to CMD5 (HCSS 3.6)
    // It only returns if the card is a SDIO card
    RawDoFmt("[brcm-sdhc] note that a timeout error on the following command (CMD5) is "
           "normal and expected if the card is not a SDIO card.\n", NULL, (APTR)putch, NULL);
    SDCardBase->sd_CMD(IO_SET_OP_COND, 0, 10000, SDCardBase);
    if(!TIMEOUT(SDCardBase))
    {
        if(CMD_TIMEOUT(SDCardBase))
        {
            if(sd_reset_cmd(SDCardBase) == -1)
                return -1;
            wr32(SDCardBase->sd_SDHC, EMMC_INTERRUPT, SD_ERR_MASK_CMD_TIMEOUT);
        }
        else
        {
            RawDoFmt("[brcm-sdhc] SDIO card detected - not currently supported\n", NULL, (APTR)putch, NULL);
            return -1;
        }
    }

    // Call an inquiry ACMD41 (voltage window = 0) to get the OCR
    SDCardBase->sd_CMD(ACMD_41, 0, 500000, SDCardBase);
    if(FAIL(SDCardBase))
    {
        RawDoFmt("[brcm-sdhc] inquiry ACMD41 failed\n", NULL, (APTR)putch, NULL);
        return -1;
    }

	// Call initialization ACMD41
	int card_is_busy = 1;
	while(card_is_busy)
	{
	    uint32_t v2_flags = 0;
	    if(v2_later)
	    {
	        // Set SDHC support
	        v2_flags |= (1 << 30);
	    }

	    SDCardBase->sd_CMD(ACMD_41, 0x00ff8000 | v2_flags, 500000, SDCardBase);
	    if(FAIL(SDCardBase))
	    {
	        RawDoFmt("[brcm-sdhc] error issuing ACMD41\n", NULL, (APTR)putch, NULL);
	        return -1;
	    }

	    if((SDCardBase->sd_Res0 >> 31) & 0x1)
	    {
	        // Initialization is complete
	        SDCardBase->sd_CardOCR = (SDCardBase->sd_Res0 >> 8) & 0xffff;
	        SDCardBase->sd_CardSupportsSDHC = (SDCardBase->sd_Res0 >> 30) & 0x1;

	        card_is_busy = 0;
	    }
	    else
	    {
            RawDoFmt("[brcm-sdhc] card is busy, retrying\n", NULL, (APTR)putch, NULL);
            SDCardBase->sd_Delay(500000, SDCardBase);
	    }
	}

    {
        UWORD args[] = {
            SDCardBase->sd_CardOCR, SDCardBase->sd_CardSupportsSDHC
        };
        RawDoFmt("[brcm-sdhc] card identified: OCR: %04x, SDHC support: %d", args, (APTR)putch, NULL);
    }

    // At this point, we know the card is definitely an SD card, so will definitely
	//  support SDR12 mode which runs at 25 MHz
    sd_switch_clock_rate(base_clock, SD_CLOCK_NORMAL, SDCardBase);

	// A small wait before the voltage switch
	SDCardBase->sd_Delay(5000, SDCardBase);

	// Send CMD2 to get the cards CID
	SDCardBase->sd_CMD(ALL_SEND_CID, 0, 500000, SDCardBase);
	if(FAIL(SDCardBase))
	{
	    RawDoFmt("SD: error sending ALL_SEND_CID\n", NULL, (APTR)putch, NULL);
	    return -1;
	}
	uint32_t card_cid_0 = SDCardBase->sd_Res0;
	uint32_t card_cid_1 = SDCardBase->sd_Res1;
	uint32_t card_cid_2 = SDCardBase->sd_Res2;
	uint32_t card_cid_3 = SDCardBase->sd_Res3;

    {
        ULONG args[] = {
            card_cid_0, card_cid_1, card_cid_2, card_cid_3
        };

        RawDoFmt("[brcm-sdhc] card CID: %08lx%08lx%08lx%08lx\n", args, (APTR)putch, NULL);
    }

#if 0
	uint32_t *dev_id = (uint32_t *)malloc(4 * sizeof(uint32_t));
	dev_id[0] = card_cid_0;
	dev_id[1] = card_cid_1;
	dev_id[2] = card_cid_2;
	dev_id[3] = card_cid_3;
	ret->bd.device_id = (uint8_t *)dev_id;
	ret->bd.dev_id_len = 4 * sizeof(uint32_t);
#endif

	// Send CMD3 to enter the data state
	SDCardBase->sd_CMD(SEND_RELATIVE_ADDR, 0, 500000, SDCardBase);
	if(FAIL(SDCardBase))
    {
        RawDoFmt("SD: error sending SEND_RELATIVE_ADDR\n", NULL, (APTR)putch, NULL);
        return -1;
    }

	uint32_t cmd3_resp = SDCardBase->sd_Res0;

	SDCardBase->sd_CardRCA = (cmd3_resp >> 16) & 0xffff;
	uint32_t crc_error = (cmd3_resp >> 15) & 0x1;
	uint32_t illegal_cmd = (cmd3_resp >> 14) & 0x1;
	uint32_t error = (cmd3_resp >> 13) & 0x1;
	uint32_t status = (cmd3_resp >> 9) & 0xf;
	uint32_t ready = (cmd3_resp >> 8) & 0x1;

	if(crc_error)
	{
		RawDoFmt("SD: CRC error\n", NULL, (APTR)putch, NULL);
		return -1;
	}

	if(illegal_cmd)
	{
		RawDoFmt("SD: illegal command\n", NULL, (APTR)putch, NULL);
		return -1;
	}

	if(error)
	{
		RawDoFmt("SD: generic error\n", NULL, (APTR)putch, NULL);
		return -1;
	}

	if(!ready)
	{
		RawDoFmt("SD: not ready for data\n", NULL, (APTR)putch, NULL);
		return -1;
	}

    RawDoFmt("[brcm-sdhc] RCA: %04x\n", &SDCardBase->sd_CardRCA, (APTR)putch, NULL);

	// Now select the card (toggles it to transfer state)
	SDCardBase->sd_CMD(SELECT_CARD, SDCardBase->sd_CardRCA << 16, 500000, SDCardBase);
	if(FAIL(SDCardBase))
	{
	    RawDoFmt("SD: error sending CMD7\n", NULL, (APTR)putch, NULL);
	    return -1;
	}


    return 0;
}

ULONG getclock(struct SDCardBase *SDCardBase)
{
    ULONG clock = get_clock_rate(1, SDCardBase);
    if (clock == 0)
        clock = 40000000;
    return clock;
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
            
            SDCardBase = (struct SDCardBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
            MakeFunctions(SDCardBase, relFuncTable, (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

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

            /* Sed SD functions in device base */
            SDCardBase->sd_PowerCycle = (APTR)((ULONG)powerCycle + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_SetLED = (APTR)((ULONG)led + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_Delay = (APTR)((ULONG)delay + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_GetBaseClock = (APTR)((ULONG)getclock + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
            SDCardBase->sd_CMD = (APTR)((ULONG)cmd + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

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
                set_clock_state(1, 1, SDCardBase);
                set_clock_rate(1, 50000000, SDCardBase);

                {
                    ULONG args[] = {
                        get_max_clock_rate(1, SDCardBase) / 1000,
                        get_min_clock_rate(1, SDCardBase) / 1000,
                        get_clock_rate(1, SDCardBase) / 1000
                    };
                    RawDoFmt("[brcm-sdhc] Clock max: %ld kHz, min: %ld kHz, selected: %ld kHz\n", args, (APTR)putch, NULL);
                }

                /* Cycle the power of card for well-known state */
                SDCardBase->sd_PowerCycle(SDCardBase);
                sd_card_init(SDCardBase);

                RawDoFmt("[brcm-sdhc] Init complete.\n", NULL, (APTR)putch, NULL);

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
