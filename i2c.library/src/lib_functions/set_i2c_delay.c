/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_base.h>

ULONG SetI2CDelay(ULONG ticks asm("d0"), struct I2C_Base *i2cBase asm("a6"))
{
	// There's no way to reliably support i2c freq tuning by specifying idle cia
	// cycles between SCL edges.
	// TODO: Add extra function for setting I2C frequency (up to 400k?).
	// TODO: delay ticks are added after every SCL edge, each takes about 1.5us
	// - calculate frequency out of it.
	return 0;
}
