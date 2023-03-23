/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>

STRPTR GetI2COpponent(REGARG(struct I2C_Base *i2cBase, "a6"))
{
	// Return id string of other process blocking hardware i2c - possibly n/a on pistorm
	return NULL;
}
