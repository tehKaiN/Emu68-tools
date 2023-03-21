/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <inline/i2c.h>

struct I2C_Base *I2C_Base = 0;

int main(void)
{
    I2C_Base = (struct I2C_Base *)OpenLibrary("i2c.library", 40);
    if (I2C_Base == NULL) {
        Printf("Failed to open i2c.library\n");
        return EXIT_FAILURE;
    }

    InitI2C();
	static uint8_t pData[5] = {0x80, 0xAF, 0xA5, 0x8D, 0x14};
    SendI2C(0x78, 5, pData);

    CloseLibrary((struct Library*)I2C_Base);
    return EXIT_SUCCESS;
}
