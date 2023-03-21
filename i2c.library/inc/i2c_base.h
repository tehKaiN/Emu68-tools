/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _BASE_H
#define _BASE_H

#include <exec/types.h>
#include <exec/libraries.h>

// I2C library interface taken from i2clib40 package on Aminet

// If you call SetI2CDelay only to read the delay, not change it:
#define I2CDELAY_READONLY 0xffffffff // V39+ !

// Type of delay to pass to AllocI2C (obsolete in V39+, see docs):
#define DELAY_TIMER 1 // Use timer.device for SCL-delay
#define DELAY_LOOP  2 // Use for/next-loop for SCL-delay

// Allocation Errors
// (as returned by AllocI2C, BringBackI2C, or found in the middle high
// byte of the error codes from V39's SendI2C/ReceiveI2C)
enum {
    I2C_OK=0, // Hardware allocated successfully
    I2C_PORT_BUSY, // \_Allocation is actually done in two steps:
    I2C_BITS_BUSY, // / port & bits, and each step may fail
    I2C_NO_MISC_RESOURCE, // Shouldn't occur, something's very wrong
    I2C_ERROR_PORT, // Failed to create a message port
    I2C_ACTIVE, // Some other I2C client has pushed us out
    I2C_NO_TIMER // Failed to open the timer.device
};

// I/O Errors
// (as found in the middle low byte of the error codes from V39's
// SendI2C/ReceiveI2C)
enum {
    // I2C_OK=0, // Last send/receive was OK
    I2C_REJECT=1, // Data not acknowledged (i.e. unwanted) */
    I2C_NO_REPLY, // Chip address apparently invalid */
    SDA_TRASHED, // SDA line randomly trashed. Timing problem? */
    SDA_LO, // SDA always LO \_wrong interface attached, */
    SDA_HI, // SDA always HI / or none at all? */
    SCL_TIMEOUT, // \_Might make sense for interfaces that can */
    SCL_HI,      // / read the clock line, but currently none can. */
    I2C_HARDW_BUSY // Hardware allocation failed
};

/*
 * Starting with V40, i2c.library exposes some statistics counters, and a
 * hint what kind of hardware implementation you are dealing with, in its
 * base structure. These data weren't present in any of the previous
 * releases, so check the lib version before you try to read them.
 */

// This structure is READ ONLY, and only present in V40 or later!
struct I2C_Base
{
    struct Library LibNode;
    ULONG SendCalls; // calls to SendI2C
    ULONG SendBytes; // bytes actually sent
    ULONG RecvCalls; // calls to ReceiveI2C
    ULONG RecvBytes; // bytes actually received
    ULONG Lost; // calls rejected due to resource conflicts
    ULONG Unheard; // calls to addresses that didn't reply
    ULONG Overflows; // times a chip rejected some or all of our data
    ULONG Errors; // errors caused by hardware/timing problems
    UBYTE HwType; // implementation: 0=par, 1=ser, 2=disk, 3=card 4=smart card

    // The data beyond this point is private and is different between most
    // of the various i2c.library implementations anyway.
    struct ExecBase *SysBase;
    BPTR SegList;
    APTR I2cHwRegs;
};

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

#endif /* _BASE_H */
