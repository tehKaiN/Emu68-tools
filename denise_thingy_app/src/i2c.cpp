/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "i2c.hpp"

#include <stdexcept>

extern "C" {
#include <clib/exec_protos.h>
#include <inline/i2c.h>
#include <libraries/i2c.h>
}

static struct I2C_Base *I2C_Base = 0;

tI2c::tI2c([[gnu::unused]] const std::string &Port)
{
    I2C_Base = (struct I2C_Base *)OpenLibrary("i2c.library", 40);
    if (I2C_Base == NULL) {
        throw std::runtime_error("Failed to open i2c.library\n");
    }
}

tI2c::~tI2c()
{
    CloseLibrary((struct Library*)I2C_Base);
}

bool tI2c::write(uint8_t ubAddr, const std::vector<uint8_t> &vData)
{
    auto Result = SendI2C(ubAddr, vData.size(), const_cast<UBYTE*>(vData.data()));
    return Result == I2C_OK;
}

bool tI2c::read(uint8_t ubAddr, uint8_t *pDest, uint32_t ulReadSize)
{
    auto Result = ReceiveI2C(ubAddr, ulReadSize, pDest);
    return Result == I2C_OK;
}
