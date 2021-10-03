#ifndef _SDCARD_H
#define _SDCARD_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/execbase.h>

struct SDCardBase {
    struct Device       sd_Device;
    struct ExecBase *   sd_SysBase;
    APTR                sd_DeviceTreeBase;
};

#define SDCARD_VERSION  0
#define SDCARD_REVISION 1
#define SDCARD_PRIORITY 20

#define BASE_NEG_SIZE   (6 * 6)
#define BASE_POS_SIZE   (sizeof(struct SDCardBase))

#endif /* _SDCARD_H */
