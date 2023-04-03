/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef I2C_PRIVATE_H
#define I2C_PRIVATE_H

#include <common/compiler.h>
#include <libraries/i2c.h>

#define MANUFACTURER_ID     0x6d73
#define PRODUCT_ID          0x01
#define SERIAL_NUMBER       0x4c32
#define LIB_VERSION         40
#define LIB_REVISION        0
#define LIB_PRIORITY        0

#define LIB_POSSIZE         (sizeof(struct I2C_Base))
#define LIB_NEGSIZE         (4*6)

// Deprecated, called internally as of original v40, shouldn't be called by end-user
BYTE AllocI2C(
    REGARG(UBYTE Delay_Type, "d0"),
    REGARG(char *Name, "a1"),
    REGARG(struct I2C_Base *i2cBase, "a6")
);

// Deprecated, called internally as of original v40, shouldn't be called by end-user
void FreeI2C(REGARG(struct I2C_Base *i2cBase, "a6"));

ULONG SetI2CDelay(REGARG(ULONG ticks, "d0"), REGARG(struct I2C_Base *i2cBase, "a6"));

// Deprecated, called internally as of original v40, shouldn't be called by end-user
void InitI2C(REGARG(struct I2C_Base *i2cBase, "a6"));

ULONG SendI2C(
    REGARG(UBYTE addr, "d0"),
    REGARG(UWORD number, "d1"),
    REGARG(UBYTE i2cdata[], "a1"),
    REGARG(struct I2C_Base *i2cBase, "a6")
);

ULONG ReceiveI2C(
    REGARG(UBYTE addr, "d0"),
    REGARG(UWORD number, "d1"),
    REGARG(UBYTE i2cdata[], "a1"),
    REGARG(struct I2C_Base *i2cBase, "a6")
);

STRPTR GetI2COpponent(REGARG(struct I2C_Base *i2cBase, "a6"));

STRPTR I2CErrText(REGARG(ULONG errnum, "d0"), REGARG(struct I2C_Base *i2cBase, "a6"));

void ShutDownI2C(REGARG(struct I2C_Base *i2cBase, "a6"));

BYTE BringBackI2C(REGARG(struct I2C_Base *i2cBase, "a6"));

#endif // I2C_PRIVATE_H
