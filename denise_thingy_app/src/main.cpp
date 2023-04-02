/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <cstdio>
#include <string>
#include <experimental/optional>
#include <common/endian.hpp>
#include <common/bcm_mbox.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <proto/exec.h>
#include "bitstream.hpp"
#include "i2c.hpp"
#include "time.hpp"

#define STATUS_BIT_BUSY (1 << 12)
#define STATUS_BIT_ERROR (1 << 13)

extern struct ExecBase *SysBase;

static bool waitForNotBusy(tI2c &I2c, uint8_t ubFpgaAddr)
{
	using namespace std::chrono_literals;
	using namespace deniseThingy;

	uint32_t ulStatus;
	do {
		time::sleepFor(10ms);
		I2c.write(ubFpgaAddr, {0x3C, 0x00, 0x00 , 0x00});
		bool isRead = I2c.read(
			ubFpgaAddr, reinterpret_cast<uint8_t*>(&ulStatus), sizeof(ulStatus)
		);
		if(!isRead) {
			return false;
		}
		ulStatus = common::endian::bigToNative(ulStatus);
		if(ulStatus & STATUS_BIT_ERROR) {
			return false;
		}
	} while(ulStatus & STATUS_BIT_BUSY);
	return true;
}

static uint8_t reverseByte(uint8_t ubData) {
	// https://graphics.stanford.edu/~seander/bithacks.html#ReverseByteWith64BitsDiv
	ubData = (ubData * 0x0202020202ULL & 0x010884422010ULL) % 1023;
	return ubData;
}

int main(int lArgCount, const char *pArgs[])
{
	using namespace std::chrono_literals;
	using namespace deniseThingy;

	if(lArgCount < 3) {
		printf("Usage:\n\t%s i2cSlaveAddrHex /path/to/cfg.bit [-id]\n", pArgs[0]);
		printf("\t-id\tIgnore DeviceId check\n");
		printf("e.g.:\n\t%s 80 file.bit\n", pArgs[0]);
		return EXIT_FAILURE;
	}

	bool isIgnoreDevId = false;
	for(uint8_t i = 3; i < lArgCount; ++i) {
		if(pArgs[i] == std::string("-id")) {
			isIgnoreDevId = true;
		}
	}

	std::experimental::optional<tBitstream> Bitstream;
	try {
		Bitstream = tBitstream(pArgs[2]);
	}
	catch(const std::exception &Exc) {
		printf(
			"ERR: bitstream '%s' read fail: '%s'\n", pArgs[2], Exc.what()
		);
		return EXIT_FAILURE;
	}

	// Display some info about bitstream
	printf("Successfully read bitstream:\n");
	for(const auto &Comment: Bitstream->m_vComments) {
		printf("\t%s\n", Comment.c_str());
	}

	// Read the I2C address of the FPGA
	auto FpgaAddr = std::stoi(pArgs[1], 0, 16);

	// Initialize I2C port
	std::experimental::optional<tI2c> I2c;
	try {
		I2c = tI2c();
	}
	catch(const std::exception &Exc) {
		printf(
			"ERR: i2c port init fail: '%s'\n", Exc.what()
		);
		return EXIT_FAILURE;
	}

	uint8_t *pMboxBufferFull = (uint8_t*)AllocMem(512, MEMF_FAST | MEMF_CLEAR);
	uint8_t *pMboxBufferAligned = (uint8_t*)(((ULONG)pMboxBufferFull + 31) & ~31);
	uint32_t *pMboxBuffer = (uint32_t*)pMboxBufferAligned;
	uint32_t *pMboxBase = (uint32_t *)0xF200B880;

	printf("Resetting FPGA...\n");

	// CRESET:=0
	set_extgpio_state(
		tExtGpio::CAM_GPIO, 0,
		pMboxBuffer, pMboxBase, SysBase
	);
	time::sleepFor(1s);
	I2c->write(FpgaAddr, {0xA4, 0xC6, 0xF4, 0x8A});

	// CRESET:=1
	set_extgpio_state(
		tExtGpio::CAM_GPIO, 1,
		pMboxBuffer, pMboxBase, SysBase
	);
	time::sleepFor(10ms);

	// 1. Read ID (E0)
	printf("Checking DeviceId...\n");
	I2c->write(FpgaAddr, {0xE0, 0x00, 0x00, 0x00});
	std::array<uint8_t, 4> DevId {0};
	bool isRead = I2c->read(FpgaAddr, DevId);
	if(!isRead) {
		printf("ERR: Can't read data from I2C device\n");
		return EXIT_FAILURE;
	}
	if(DevId != Bitstream->m_DeviceId) {
		printf(
			"ERR: DeviceId mismatch! Dev: %02X %02X %02X %02X, "
			".bit: %02X, %02X, %02X, %02X\n",
			DevId[0], DevId[1], DevId[2], DevId[3],
			Bitstream->m_DeviceId[0], Bitstream->m_DeviceId[1],
			Bitstream->m_DeviceId[2], Bitstream->m_DeviceId[3]
		);
		if(isIgnoreDevId) {
			printf("Continuing due to -id flag set...\n");
		}
		else {
			return EXIT_FAILURE;
		}
	}

	// 2. Enable configuration interface (C6)
	printf("Initiating programming...\n");
	I2c->write(FpgaAddr, {0xC6, 0x00, 0x00, 0x00});
	time::sleepFor(1ms);

	// 3. Read status register (3C) and wait for busy bit go to 0
	if(!waitForNotBusy(*I2c, FpgaAddr)) {
		printf("ERR: status register has error bit set!\n");
		return EXIT_FAILURE;
	}

	// 4. LSC_INIT_ADDRESS (46)
	I2c->write(FpgaAddr, {0x46, 0x00, 0x00 , 0x00});
	time::sleepFor(100ms);

	// 5. Program SRAM parts (82)
	uint32_t lRowIdx;
	for(const auto &Row: Bitstream->m_vProgramData) {
		printf(
			"\rProgramming row %5u/%5u...",
			++lRowIdx, Bitstream->m_vProgramData.size()
		);
		fflush(stdout);
		// Compose and send next SRAM packet
		std::vector<uint8_t> Packet = {0x82, 0x21, 0x00, 0x00};
		for(const auto &RowByte: Row) {
			Packet.push_back(RowByte);
		}
		I2c->write(FpgaAddr, Packet);
		time::sleepFor(100us);

		// Some kind of dummy packet
		I2c->write(FpgaAddr, {0x00, 0x00, 0x00, 0x00});
		time::sleepFor(100us);
	}
	printf("\n");

	// 6. Program usercode (C2)
	printf("Programming usercode...\n");
	I2c->write(FpgaAddr, {
		0x46, 0x00, 0x00 , 0x00,
		reverseByte(Bitstream->m_UserCode[3]), reverseByte(Bitstream->m_UserCode[2]),
		reverseByte(Bitstream->m_UserCode[1]), reverseByte(Bitstream->m_UserCode[0])
	});

	// 7. Program done (5E)
	printf("Finalizing programming...\n");
	I2c->write(FpgaAddr, {0x5E, 0x00, 0x00 , 0x00});

	// 8. Read status register (3C) and wait for busy bit go to 0
	if(!waitForNotBusy(*I2c, FpgaAddr)) {
		printf("ERR: status register has error bit set!\n");
		return EXIT_FAILURE;
	}

	// 9. Exit programming mode (26)
	I2c->write(FpgaAddr, {0x26, 0x00, 0x00 , 0x00});

	printf("All done!\n");
	return EXIT_SUCCESS;
}
