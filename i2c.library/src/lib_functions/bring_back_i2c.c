/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_private.h>

BYTE BringBackI2C(REGARG(struct I2C_Base *i2cBase, "a6"))
{
	// TODO: free the usage lock which was set up by ShutDownI2C
}
