/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>

STRPTR I2CErrText(ULONG errnum asm("d0"), struct I2C_Base *i2cBase asm("a6"))
{
	// Original errors from i2clib40src:
	// "OK"
	// // I/O errors:
	// "data rejected"
	// "no reply"
	// "SDA trashed"
	// "SDA always LO"
	// "SDA always HI"
	// "hardware is busy"
	// // allocation errors
	// "port is busy"
	// "port bits are busy"
	// 'no '
	// "misc.resource"
	// "temporary shutdown"
  // // extras:
	// "error"
	// "???"

	// According to autodocs, errors should be at most 20-character long
	return errnum == I2C_OK ? "OK" : "???";
}
