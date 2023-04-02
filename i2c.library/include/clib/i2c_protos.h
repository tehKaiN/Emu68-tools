/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef CLIB_I2C_PROTOS_H
#define CLIB_I2C_PROTOS_H

/*
**   $VER: i2c_protos.h 0.1.0 $Id: i2c_lib.sfd 0.1.0 $
**
**   C prototypes. For use with 32 bit integers only.
**
**   Copyright (c) 2001 Amiga, Inc.
**       All Rights Reserved
*/

#include <exec/types.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


/* "i2c.library" */
BYTE AllocI2C(UBYTE Delay_Type, char * Name);
void FreeI2C(void);
ULONG SetI2CDelay(ULONG ticks);
void InitI2C(void);
ULONG SendI2C(UBYTE addr, UWORD number, UBYTE * i2cdata);
ULONG ReceiveI2C(UBYTE addr, UWORD number, UBYTE * i2cdata);
STRPTR GetI2COpponent(void);
STRPTR I2CErrText(ULONG errnum);
void ShutDownI2C(void);
BYTE BringBackI2C(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* CLIB_I2C_PROTOS_H */
