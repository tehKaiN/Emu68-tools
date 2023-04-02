/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GPIO_H
#define GPIO_H

#include <common/bcm_gpio.h>

void gpioSetPull(volatile tGpioRegs *pGpio, UBYTE ubIndex, tGpioPull ePull);

void gpioSetAlternate(
	volatile tGpioRegs *pGpio,
	UBYTE ubIndex,
	tGpioAlternativeFunction eAlternativeFunction
);

void gpioSetLevel(volatile tGpioRegs *pGpio, UBYTE ubIndex, UBYTE ubState);

#endif // GPIO_H
