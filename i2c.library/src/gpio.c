/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gpio.h"
#include <common/endian.h>
#include "bcm_peripheral.h"

static void busyWait(uint32_t ulCycles)
{
	volatile uint32_t i = ulCycles;
	while(i--) continue;
}

void gpioSetPull(volatile tGpioRegs *pGpio, UBYTE ubIndex, tGpioPull ePull)
{
	UBYTE ubRegIndex = ubIndex / 32;
	UBYTE ubRegShift = ubIndex % 32;

	// This is kinda like bitbanging of the pulse on the 2-lane bus
	wr32le(&pGpio->GPPUD, ePull);
	busyWait(150);
	wr32le(&pGpio->GPPUDCLK[ubRegIndex], 1 << ubRegShift);
	busyWait(150);
	wr32le(&pGpio->GPPUD, 0);
	wr32le(&pGpio->GPPUDCLK[ubRegIndex], 0);
}

void gpioSetAlternate(
	volatile tGpioRegs *pGpio,
	UBYTE ubIndex,
	tGpioAlternativeFunction eAlternativeFunction
)
{
	static const UBYTE ubBitsPerGpio = 3;
	UBYTE ubRegIndex = ubIndex / 10;
	UBYTE ubRegShift = (ubIndex % 10) * ubBitsPerGpio;
	uint32_t ulClearMask = ~(0b111 << ubRegShift);
	uint32_t ulWriteMask = eAlternativeFunction << ubRegShift;
	wr32le(
		&pGpio->GPFSEL[ubRegIndex],
		(rd32le(&pGpio->GPFSEL[ubRegIndex]) & ulClearMask) | ulWriteMask
	);
}

void gpioSetLevel(volatile tGpioRegs *pGpio, UBYTE ubIndex, UBYTE ubState)
{
	UBYTE ubRegIndex = ubIndex / 32;
	UBYTE ubRegShift = ubIndex % 32;
	uint32_t ulRegState = (1 << ubRegShift);
	if(ubState) {
		wr32le(&pGpio->GPSET[ubRegIndex], ulRegState);
	}
	else {
		wr32le(&pGpio->GPCLR[ubRegIndex], ulRegState);
	}
}
