#include <exec/resident.h>
#include <exec/nodes.h>
#include <exec/devices.h>

#include <proto/exec.h>

#include <stdint.h>

#include "wifipi.h"

#define D(x) x

extern const char deviceEnd;
extern const char deviceName[];
extern const char deviceIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&deviceEnd,
    RTF_AUTOINIT,
    WIFIPI_VERSION,
    NT_DEVICE,
    WIFIPI_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    (APTR)InitTable,
};

const char deviceName[] = "wifipi.device";
const char deviceIdString[] = VERSION_STRING;

static uint32_t WiFi_ExtFunc()
{
    return 0;
}

static BPTR WiFi_Expunge(struct WiFiBase * WiFiBase asm("a6"))
{
    struct ExecBase *SysBase = WiFiBase->w_SysBase;
    BPTR segList = 0;

    D(bug("[WiFi] WiFi_Expunge()\n"));

    /* If device's open count is 0, remove it from list and free memory */
    if (WiFiBase->w_Device.dd_Library.lib_OpenCnt == 0)
    {
        UWORD negSize = WiFiBase->w_Device.dd_Library.lib_NegSize;
        UWORD posSize = WiFiBase->w_Device.dd_Library.lib_PosSize;

        /* Return SegList so that DOS can unload the binary */
        segList = WiFiBase->w_SegList;

        /* Remove device node */
        Remove(&WiFiBase->w_Device.dd_Library.lib_Node);
        
        /* Free memory */
        FreeMem((APTR)((ULONG)WiFiBase - (negSize + posSize)), sizeof(struct WiFiBase));
    }
    else
    {
        WiFiBase->w_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
    }

    return segList;
}

static uint32_t wifipi_functions[] = {
#if 0
    (uint32_t)WiFi_Open,
    (uint32_t)WiFi_Close,
#endif
    (uint32_t)WiFi_Expunge,
    (uint32_t)WiFi_ExtFunc,
#if 0
    (uint32_t)WiFi_BeginIO,
    (uint32_t)WiFi_AbortIO,
#endif
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct WiFiBase), 
    (uint32_t)wifipi_functions, 
    0, 
    (uint32_t)WiFi_Init
};
