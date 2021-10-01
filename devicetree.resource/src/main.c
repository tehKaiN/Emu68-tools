#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <exec/libraries.h>
#include <dos/dosextens.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <proto/exec.h>
#include <stdint.h>

#define VERSION             0
#define REVISION            1

#define MANUFACTURER_ID     0x6d73
#define PRODUCT_ID          0x21
#define SERIAL_NUMBER       0x04f6403d 

extern UBYTE diag_start;
extern UBYTE rom_end;
extern UBYTE rom_start;
extern UBYTE ramcopy_end;
extern ULONG diag_offset;
extern const char deviceName[];
extern const char deviceIdString[];
void Init();

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&ramcopy_end,
    RTW_COLDSTART,
    VERSION,
    NT_RESOURCE,
    120,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    Init,
};

const char deviceName[] = "devicetree.resource";
const char deviceIdString[] = VERSION_STRING;

const APTR patchListRAM[] = {
    (APTR)((intptr_t)&RomTag.rt_MatchTag - 32),
    (APTR)((intptr_t)&RomTag.rt_EndSkip - 32),
    (APTR)-1
};

const APTR patchListROM[] = {
    (APTR)&RomTag.rt_Init,
    (APTR)&RomTag.rt_Name,
    (APTR)&RomTag.rt_IdString,
    (APTR)-1
};

int DiagPoint(APTR boardBase asm("a0"), struct DiagArea *diagCopy asm("a2"), struct ConfigDev *configDev asm("a3"))
{
    const APTR *patch = &patchListRAM[0];
    ULONG offset = (ULONG)&diag_offset;

    /* Patch parts which reside in RAM only */
    while(*patch != (APTR)-1)
    {
        *(ULONG*)*patch += (intptr_t)diagCopy - offset;
        patch++;
    }

    /* Patch parts which are in the ROM image */
    patch = &patchListROM[0];
    while(*patch != (APTR)-1)
    {
        *(ULONG*)*patch += (intptr_t)boardBase;
        patch++;
    }

    return 1;
}

void BootPoint()
{

}
