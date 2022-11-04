#ifndef __EMMC_H
#define __EMMC_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/semaphores.h>
#include <exec/interrupts.h>
#include <devices/timer.h>
#include <libraries/configvars.h>
#include <stdint.h>

#define EMMC_VERSION  1
#define EMMC_REVISION 0
#define EMMC_PRIORITY 20

#define USE_BUSY_TIMER  0

struct emmc_scr
{
    uint32_t    scr[2];
    uint32_t    emmc_bus_widths;
    int         emmc_version;
    int         emmc_commands;
};

struct EMMCUnit;

struct EMMCBase {
    struct Device       emmc_Device;
    struct ExecBase *   emmc_SysBase;
    APTR                emmc_ROMBase;
    APTR                emmc_DeviceTreeBase;
    APTR                emmc_MailBox;
    APTR                emmc_Regs;
    ULONG *             emmc_Request;
    APTR                emmc_RequestBase;
    ULONG               emmc_SDHCClock;

    struct ConfigDev *  emmc_ConfigDev;

    struct EMMCUnit *   emmc_Units[5];    /* 5 units at most for the case where SDCard has 4 primary partitions type 0x76 */
    UWORD               emmc_UnitCount;

    struct SignalSemaphore emmc_Lock;
    struct timerequest  emmc_TimeReq;
    struct MsgPort      emmc_Port;

    struct emmc_scr     emmc_SCR;

    UWORD               emmc_BlockSize;
    UWORD               emmc_BlocksToTransfer;
    APTR                emmc_Buffer;

    ULONG               emmc_Res0;
    ULONG               emmc_Res1;
    ULONG               emmc_Res2;
    ULONG               emmc_Res3;

    ULONG               emmc_CID[4];
    UBYTE               emmc_StatusReg[64];
    CONST_STRPTR        emmc_ManuID[255];

    ULONG               emmc_Capabilities0;
    ULONG               emmc_Capabilities1;
    ULONG               emmc_LastCMD;
    ULONG               emmc_LastCMDSuccess;
    ULONG               emmc_LastError;
    ULONG               emmc_LastInterrupt;
    ULONG               emmc_CardRCA;
    ULONG               emmc_CardRemoval;
    ULONG               emmc_FailedVoltageSwitch;
    ULONG               emmc_CardOCR;
    ULONG               emmc_CardSupportsSDHC;
    ULONG               emmc_Overclock;

    UBYTE               emmc_DisableHighSpeed;
    UBYTE               emmc_InCommand;
    UBYTE               emmc_AppCommand;
    UBYTE               emmc_HideUnit0;
    UBYTE               emmc_ReadOnlyUnit0;
    UBYTE               emmc_Verbose;

    struct Interrupt    emmc_Interrupt;
};

struct EMMCUnit {
    struct Unit         su_Unit;
    struct EMMCBase *   su_Base;
    uint32_t            su_StartBlock;
    uint32_t            su_BlockCount;
    uint8_t             su_UnitNum;
    uint8_t             su_ReadOnly;
    struct Task *       su_Caller;
};

void UnitTask();

#define UNIT_TASK_PRIORITY  10
#define UNIT_TASK_STACKSIZE 16384

#define BASE_NEG_SIZE   (6 * 6)
#define BASE_POS_SIZE   (sizeof(struct EMMCBase))

/* Endian support */

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

static inline ULONG rd32(APTR addr, ULONG offset)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    ULONG val = LE32(*(volatile ULONG *)addr_off);
    asm volatile("nop");
    return val;
}

static inline void wr32(APTR addr, ULONG offset, ULONG val)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    *(volatile ULONG *)addr_off = LE32(val);
    asm volatile("nop");
}

static inline ULONG rd32be(APTR addr, ULONG offset)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    ULONG val = *(volatile ULONG *)addr_off;
    asm volatile("nop");
    return val;
}

static inline void wr32be(APTR addr, ULONG offset, ULONG val)
{
    APTR addr_off = (APTR)((ULONG)addr + offset);
    *(volatile ULONG *)addr_off = val;
    asm volatile("nop");
}

/* Misc */

static inline void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

void kprintf(const char * msg asm("a0"), void * args asm("a1"));
void delay(ULONG us, struct EMMCBase *EMMCBase);
ULONG EMMC_Expunge(struct EMMCBase * EMMCBase asm("a6"));
APTR EMMC_ExtFunc(struct EMMCBase * EMMCBase asm("a6"));
void EMMC_Open(struct IORequest * io asm("a1"), LONG unitNumber asm("d0"), ULONG flags asm("d1"));
ULONG EMMC_Close(struct IORequest * io asm("a1"));
void EMMC_BeginIO(struct IORequest *io asm("a1"));
LONG EMMC_AbortIO(struct IORequest *io asm("a1"));

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; kprintf(string, &args[1]); } while(0)

#endif /* __EMMC_H */
