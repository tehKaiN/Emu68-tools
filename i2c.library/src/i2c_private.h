/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef I2C_PRIVATE_H
#define I2C_PRIVATE_H

#include <libraries/i2c.h>

#define MANUFACTURER_ID     0x6d73
#define PRODUCT_ID          0x01
#define SERIAL_NUMBER       0x4c32
#define LIB_VERSION         40
#define LIB_REVISION        0
#define LIB_PRIORITY        0

#define LIB_POSSIZE         (sizeof(struct I2C_Base))
#define LIB_NEGSIZE         (4*6)

BYTE AllocI2C(
    UBYTE Delay_Type asm("d0"),
    char *Name asm("a0"),
    struct I2C_Base *i2cBase asm("a6")
);

void FreeI2C(struct I2C_Base *i2cBase asm("a6"));

ULONG SetI2CDelay(ULONG ticks asm("d0"), struct I2C_Base *i2cBase asm("a6"));

void InitI2C(struct I2C_Base *i2cBase asm("a6"));

ULONG SendI2C(
    UBYTE addr asm("d0"),
    UWORD number asm("d1"),
    UBYTE i2cdata[] asm("a0"),
    struct I2C_Base *i2cBase asm("a6")
);

ULONG ReceiveI2C(
    UBYTE addr asm("d0"),
    UWORD number asm("d1"),
    UBYTE i2cdata[] asm("a0"),
    struct I2C_Base *i2cBase asm("a6")
);

STRPTR GetI2COpponent(struct I2C_Base *i2cBase asm("a6"));

STRPTR I2CErrText(ULONG errnum asm("d0"), struct I2C_Base *i2cBase asm("a6"));

void ShutDownI2C(struct I2C_Base *i2cBase asm("a6"));

BYTE BringBackI2C(struct I2C_Base *i2cBase asm("a6"));

#endif // I2C_PRIVATE_H
