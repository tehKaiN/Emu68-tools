/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <exec/types.h>
#include <proto/exec.h>
#include <common/endian.h>
#include <common/bcm_i2c.h>
#include <i2c_base.h>

#define RESULT(isError, ubIoError, ubAllocError) ((ubAllocError << 16) | (ubIoError << 8) | (isError))

ULONG SendI2C(
	UBYTE ubAddress asm("d0"),
	UWORD uwDataSize asm("d1"),
	UBYTE pData[] asm("a0"),
	struct I2C_Base *i2cBase asm("a6")
)
{
	// bcm expects read/write bit to be omitted from address
	ubAddress >>= 1;

	volatile tI2cRegs * const pI2c = (volatile tI2cRegs *)i2cBase->I2cHwRegs;
	UBYTE isError = I2C_OK, ubIoError = I2C_OK, ubAllocError = I2C_OK;

	wr32le(&pI2c->A, ubAddress);
	wr32le(&pI2c->C, I2C_C_CLEAR_FIFO_ONE_SHOT);
	wr32le(&pI2c->S, I2C_S_CLKT | I2C_S_ERR | I2C_S_DONE);
	wr32le(&pI2c->DLEN, uwDataSize);
	wr32le(&pI2c->C, I2C_C_I2CEN | I2C_C_ST);

	UWORD uwBytesCopied = 0;
	while(!(rd32le(&pI2c->S) & I2C_S_DONE)) {
		while(uwDataSize && (rd32le(&pI2c->S) & I2C_S_TXD)) {
			wr32le(&pI2c->FIFO, *(pData++));
			uwBytesCopied++;
			--uwDataSize;
		}
	}

	ULONG ulStatus = rd32le(&pI2c->S);
	wr32le(&pI2c->S, I2C_S_DONE);

	if((ulStatus & (I2C_S_ERR | I2C_S_CLKT)) || uwDataSize) {
		isError = 1;
		ubIoError = I2C_REJECT;
	}

	return RESULT(isError, ubIoError, ubAllocError);
}
