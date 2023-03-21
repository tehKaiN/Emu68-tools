/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <common/bcm_gpio.h>
#include <common/bcm_i2c.h>

#define BCM2708_PERI_BASE 0xF2000000

volatile tGpioRegs * const g_pGpio = (tGpioRegs*)(BCM2708_PERI_BASE + 0x200000);
volatile tI2cRegs * const g_pI2c0 = (tI2cRegs*)(BCM2708_PERI_BASE + 0x205000);
volatile tI2cRegs * const g_pI2c1 = (tI2cRegs*)(BCM2708_PERI_BASE + 0x804000);
volatile tI2cRegs * const g_pI2c2 = (tI2cRegs*)(BCM2708_PERI_BASE + 0x805000); // I2C iniside HDMI
