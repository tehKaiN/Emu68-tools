/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BCM_GPIO_H
#define BCM_GPIO_H

#include <exec/types.h>

// GPIO alternative functions ordered by pin function
#define GPIO0_AF_I2C0_SDA GPIO_AF_0
#define GPIO1_AF_I2C0_SCL GPIO_AF_0

#define GPIO2_AF_I2C1_SDA GPIO_AF_0
#define GPIO3_AF_I2C1_SCL GPIO_AF_0

#define GPIO28_AF_I2C0_SDA GPIO_AF_0
#define GPIO29_AF_I2C0_SCL GPIO_AF_0

#define GPIO44_AF_I2C0_SDA GPIO_AF_1
#define GPIO45_AF_I2C0_SCL GPIO_AF_1
#define GPIO44_AF_I2C1_SDA GPIO_AF_2
#define GPIO45_AF_I2C1_SCL GPIO_AF_2

typedef enum tGpioAlternativeFunction {
	GPIO_AF_INPUT = 0b000,
	GPIO_AF_OUTPUT = 0b001,
	GPIO_AF_0 = 0b100,
	GPIO_AF_1 = 0b101,
	GPIO_AF_2 = 0b110,
	GPIO_AF_3 = 0b110,
	GPIO_AF_4 = 0b011,
	GPIO_AF_5 = 0b010,
} tGpioAlternativeFunction;

typedef enum tGpioPull {
	GPIO_PULL_OFF = 0b00, // no pull
	GPIO_PULL_PD = 0b01, // pull down
	GPIO_PULL_PU = 0b10, // pull up
} tGpioPull;

typedef struct tGpioRegs {
	ULONG GPFSEL[6];
	ULONG RESERVED0; // reserved
	ULONG GPSET[2];
	ULONG RESERVED1; // reserved
	ULONG GPCLR[2];
	ULONG RESERVED2;
	ULONG GPLEV[2];
	ULONG RESERVED3;
	ULONG GPEDS[2];
	ULONG RESERVED4;
	ULONG GPREN[2];
	ULONG RESERVED5;
	ULONG GPFEN[2];
	ULONG RESERVED6;
	ULONG GPHEN[2];
	ULONG RESERVED7;
	ULONG GPLEN[2];
	ULONG RESERVED8;
	ULONG GPAREN[2];
	ULONG RESERVED9;
	ULONG GPAFEN[2];
	ULONG RESERVED10;
	ULONG GPPUD;
	ULONG GPPUDCLK[2];
	ULONG RESERVED11;
	ULONG TEST;
} tGpioRegs;

#endif // BCM_GPIO_H
