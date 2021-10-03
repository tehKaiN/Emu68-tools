#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <exec/io.h>
#include <exec/errors.h>

#include "sdcard.h"

extern UWORD relFuncTable[];
asm(
"       .globl _relFuncTable    \n"
"_relFuncTable:                 \n"
"       .short _SD_Open         \n"
"       .short _SD_Close        \n"
"       .short _SD_Expunge      \n"
"       .short _SD_ExtFunc      \n"
"       .short _SD_BeginIO      \n"
"       .short _SD_AbortIO      \n"
"       .short -1               \n"
);

ULONG SD_Expunge(struct SDCardBase * SDCardBase asm("a6"))
{
    if (SDCardBase->sd_Device.dd_Library.lib_OpenCnt > 0)
    {
        SDCardBase->sd_Device.dd_Library.lib_Flags |= LIBF_DELEXP;
    }

    return 0;
}

APTR SD_ExtFunc(struct SDCardBase * SDCardBase asm("a6"))
{
    return SDCardBase;
}

void SD_Open(struct IORequest * io asm("a1"), LONG unitNumber asm("d0"),
    ULONG flags asm("d1"), struct SDCardBase * SDCardBase asm("a6"))
{
    (void)flags;
    (void)unitNumber;

    /* 
        Do whatever necessary to open given unit number with flags, set NT_REPLYMSG if 
        opening device shall complete with success, set io_Error otherwise
    */

    if (io->io_Error == 0)
    {
        SDCardBase->sd_Device.dd_Library.lib_OpenCnt++;
        SDCardBase->sd_Device.dd_Library.lib_Flags &= ~LIBF_DELEXP;

        io->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    }
    else
    {
        io->io_Error = IOERR_OPENFAIL;
    }
    
    /* In contrast to normal library there is no need to return anything */
    return;
}

ULONG SD_Close(struct IORequest * io asm("a1"), struct SDCardBase * SDCardBase asm("a6"))
{
    (void)io;

    SDCardBase->sd_Device.dd_Library.lib_OpenCnt--;

    if (SDCardBase->sd_Device.dd_Library.lib_OpenCnt == 0)
    {
        if (SDCardBase->sd_Device.dd_Library.lib_Flags & LIBF_DELEXP)
        {
            return SD_Expunge(SDCardBase);
        }
    }
    
    return 0;
}

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    return NULL;
}
