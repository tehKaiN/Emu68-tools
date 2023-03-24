/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BCM_PERIPHERAL_H
#define BCM_PERIPHERAL_H

#include <common/bcm_gpio.h>
#include <common/bcm_i2c.h>

#define BCM2708_PERI_BASE 0xF2000000

#define BCM_GPIO ((volatile tGpioRegs*)(BCM2708_PERI_BASE + 0x200000));
#define BCM_I2C0 (volatile tI2cRegs*)(BCM2708_PERI_BASE + 0x205000);
#define BCM_I2C1 (volatile tI2cRegs*)(BCM2708_PERI_BASE + 0x804000);
#define BCM_I2C2 (volatile tI2cRegs*)(BCM2708_PERI_BASE + 0x805000); // I2C iniside HDMI

#endif // BCM_PERIPHERAL_H
