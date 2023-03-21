/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BCM_PERIPHERAL_H
#define BCM_PERIPHERAL_H

#include <common/bcm_gpio.h>
#include <common/bcm_i2c.h>

volatile tGpioRegs * const g_pGpio;
volatile tI2cRegs * const g_pI2c0;
volatile tI2cRegs * const g_pI2c1;
volatile tI2cRegs * const g_pI2c2;

#endif // BCM_PERIPHERAL_H
