/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BCM_I2C_H
#define BCM_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <exec/types.h>

// Based on BCM2835 ARM Peripherals, chapter 3: I2C

#define I2C_C_I2CEN (1 << 15) // I2C Enable
#define I2C_C_INTR (1 <<  10) // Interrupt on RX
#define I2C_C_INTT (1 << 9) // Interrupt on TX
#define I2C_C_INTD (1 << 8) // Interrupt on Done
#define I2C_C_ST (1 << 7) // Start transfer
#define I2C_C_CLEAR_FIFO_NONE (0b00 << 4)
#define I2C_C_CLEAR_FIFO_ONE_SHOT (0b01 << 4)
#define I2C_C_CLEAR_FIFO_ONE_SHOT2 (0b10 << 4)
#define I2C_C_WRITE_PACKET (0 << 0)
#define I2C_C_READ_PACKET (1 << 0)

#define I2C_S_CLKT (1 << 9) // Clock stretch timeout
#define I2C_S_ERR (1 << 8) // ERR Ack error
#define I2C_S_RXF (1 << 7) // RX FIFO full
#define I2C_S_TXE (1 << 6) // TX FIFO empty
#define I2C_S_RXD (1 << 5) // RX contains data
#define I2C_S_TXD (1 << 4) // TX contains data
#define I2C_S_RXR (1 << 3) // RX needs reading
#define I2C_S_TXW (1 << 2) // TX needs writing
#define I2C_S_DONE (1 << 1) // Transfer done
#define I2C_S_TA (1 << 0) // Transfer active

typedef struct tI2cRegs {
	ULONG C;
	ULONG S;
	ULONG DLEN;
	ULONG A;
	ULONG FIFO;
	ULONG DIV;
	ULONG DEL;
	ULONG CLKT;
} tI2cRegs;

#ifdef __cplusplus
}
#endif

#endif // BCM_I2C_H
