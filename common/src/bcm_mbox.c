/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <common/bcm_mbox.h>
#include <common/endian.h>

#include <exec/io.h>
#include <exec/errors.h>

#include <proto/exec.h>

/* status register flags */

#define MBOX_TX_FULL (1UL << 31)
#define MBOX_RX_EMPTY (1UL << 30)
#define MBOX_CHANMASK 0xF

/* VideoCore tags used. */

#define VCTAG_GET_ARM_MEMORY     0x00010005
#define VCTAG_GET_CLOCK_RATE     0x00030002

#define CMD_GET_CLOCK_RATE 0x00030047
#define CMD_SET_CLOCK_RATE 0x00038002
#define CMD_GET_CLOCK_STATE 0x00030001
#define CMD_SET_CLOCK_STATE 0x00038001
#define CMD_GET_POWER_STATE 0x00020001
#define CMD_SET_POWER_STATE 0x00028001
#define CMD_GET_GPIO_STATE 0x00030041
#define CMD_SET_GPIO_STATE 0x00038041

static uint32_t mbox_recv(uint32_t channel, uint32_t *mailbox_base)
{
	volatile uint32_t *mbox_read = mailbox_base;
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)mailbox_base + 0x18);
	uint32_t response, status;

	do
	{
		do
		{
			status = LE32(*mbox_status);
			asm volatile("nop");
		}
		while (status & MBOX_RX_EMPTY);

		asm volatile("nop");
		response = LE32(*mbox_read);
		asm volatile("nop");
	}
	while ((response & MBOX_CHANMASK) != channel);

	return (response & ~MBOX_CHANMASK);
}

static void mbox_send(uint32_t channel, uint32_t data, uint32_t *mailbox_base)
{
	volatile uint32_t *mbox_write = (uint32_t*)((uintptr_t)mailbox_base + 0x20);
	volatile uint32_t *mbox_status = (uint32_t*)((uintptr_t)mailbox_base + 0x18);
	uint32_t status;

	data &= ~MBOX_CHANMASK;
	data |= channel & MBOX_CHANMASK;

	do
	{
		status = LE32(*mbox_status);
		asm volatile("nop");
	}
	while (status & MBOX_TX_FULL);

	asm volatile("nop");
	*mbox_write = LE32(data);
}

uint32_t get_clock_rate(
    uint32_t clock_id,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_GET_CLOCK_RATE);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t resp = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_clock_rate(
    uint32_t clock_id,
    uint32_t speed,
    uint32_t FBReq[9],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 9*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_SET_CLOCK_RATE);
    FBReq[3] = LE32(12);
    FBReq[4] = 0;
    FBReq[5] = LE32(clock_id);
    FBReq[6] = LE32(speed);
    FBReq[7] = 0;
    FBReq[8] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t get_clock_state(
    uint32_t id,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_GET_CLOCK_STATE);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_clock_state(
    uint32_t id,
    uint32_t state,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_SET_CLOCK_STATE);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = LE32(state);
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t get_power_state(
    uint32_t id,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_GET_POWER_STATE);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_power_state(
    uint32_t id,
    uint32_t state,
    uint32_t FBReq[9],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 9*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_SET_POWER_STATE);// SetClockRate
    FBReq[3] = LE32(12);
    FBReq[4] = 0;
    FBReq[5] = LE32(id);
    FBReq[6] = LE32(state);
    FBReq[7] = 0;
    FBReq[8] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t get_extgpio_state(
    tExtGpio gpio,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_GET_GPIO_STATE);
    FBReq[3] = LE32(8);
    FBReq[4] = 0;
    FBReq[5] = LE32(128 + gpio);
    FBReq[6] = 0;
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}

uint32_t set_extgpio_state(
    tExtGpio gpio,
    uint32_t state,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
)
{
    uint32_t len = 8*4;

    FBReq[0] = LE32(len);       // Length
    FBReq[1] = 0;               // Request
    FBReq[2] = LE32(CMD_SET_GPIO_STATE);
    FBReq[3] = LE32(8);
    FBReq[4] = LE32(8);
    FBReq[5] = LE32(128 + gpio);
    FBReq[6] = LE32(state);
    FBReq[7] = 0;

    CachePreDMA(FBReq, &len, 0);
    mbox_send(8, (uint32_t)FBReq, mailbox_base);
    uint32_t reply = mbox_recv(8, mailbox_base);
    CachePostDMA(FBReq, &len, 0);

    return LE32(FBReq[6]);
}
