/* Automatically generated header (sfdc 1.11e)! Do not edit! */

#ifndef PROTO_I2C_H
#define PROTO_I2C_H

#include <clib/i2c_protos.h>

#ifndef _NO_INLINE
# if defined(__GNUC__)
#  ifdef __AROS__
#   include <defines/i2c.h>
#  else
#   include <inline/i2c.h>
#  endif
# else
#  include <pragmas/i2c_pragmas.h>
# endif
#endif /* _NO_INLINE */

#ifdef __amigaos4__
# include <interfaces/i2c.h>
# ifndef __NOGLOBALIFACE__
   extern struct I2C_IFace *II2C_;
# endif /* __NOGLOBALIFACE__*/
#endif /* !__amigaos4__ */
#ifndef __NOLIBBASE__
  extern APTR
# ifdef __CONSTLIBBASEDECL__
   __CONSTLIBBASEDECL__
# endif /* __CONSTLIBBASEDECL__ */
  I2C_Base;
#endif /* !__NOLIBBASE__ */

#endif /* !PROTO_I2C_H */
