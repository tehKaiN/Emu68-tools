/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef _INLINE_I2C_H
#define _INLINE_I2C_H

#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
#ifdef __HAVE_IPTR_ATTR__
typedef APTR _sfdc_vararg __attribute__((iptr));
#else
typedef ULONG _sfdc_vararg;
#endif /* __HAVE_IPTR_ATTR__ */
#endif /* _SFDC_VARARG_DEFINED */

#ifndef __INLINE_MACROS_H
#include <inline/macros.h>
#endif /* !__INLINE_MACROS_H */

#ifndef I2C_BASE_NAME
#define I2C_BASE_NAME I2C_Base
#endif /* !I2C_BASE_NAME */

#define AllocI2C(___Delay_Type, ___Name) \
      LP2(0x1e, BYTE, AllocI2C , UBYTE, ___Delay_Type, d0, char *, ___Name, a0,\
      , I2C_BASE_NAME)

#define FreeI2C() \
      LP0NR(0x24, FreeI2C ,\
      , I2C_BASE_NAME)

#define SetI2CDelay(___ticks) \
      LP1(0x2a, ULONG, SetI2CDelay , ULONG, ___ticks, d0,\
      , I2C_BASE_NAME)

#define InitI2C() \
      LP0NR(0x30, InitI2C ,\
      , I2C_BASE_NAME)

#define SendI2C(___addr, ___number, ___i2cdata) \
      LP3(0x36, ULONG, SendI2C , UBYTE, ___addr, d0, UWORD, ___number, d1, UBYTE, ___i2cdata, a0,\
      , I2C_BASE_NAME)

#define ReceiveI2C(___addr, ___number, ___i2cdata) \
      LP3(0x3c, ULONG, ReceiveI2C , UBYTE, ___addr, d0, UWORD, ___number, d1, UBYTE, ___i2cdata, a0,\
      , I2C_BASE_NAME)

#define GetI2COpponent() \
      LP0(0x42, STRPTR, GetI2COpponent ,\
      , I2C_BASE_NAME)

#define I2CErrText(___errnum) \
      LP1(0x48, STRPTR, I2CErrText , ULONG, ___errnum, d0,\
      , I2C_BASE_NAME)

#define ShutDownI2C() \
      LP0NR(0x4e, ShutDownI2C ,\
      , I2C_BASE_NAME)

#define BringBackI2C() \
      LP0(0x54, BYTE, BringBackI2C ,\
      , I2C_BASE_NAME)

#endif /* !_INLINE_I2C_H */
