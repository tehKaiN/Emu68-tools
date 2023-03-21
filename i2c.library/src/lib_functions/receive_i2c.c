/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_base.h>

ULONG ReceiveI2C(
	UBYTE addr asm("d0"),
	UWORD number asm("d1"),
	UBYTE i2cdata[] asm("a0"),
	struct I2C_Base *i2cBase asm("a6")
)
{
	// TODO

}
