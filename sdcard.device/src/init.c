#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include "sdcard.h"

const CONST_APTR funcTable[] = {
    (CONST_APTR) -1
};

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    return NULL;
}
