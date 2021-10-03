#ifndef _SDCARD_H
#define _SDCARD_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/devices.h>
#include <exec/execbase.h>

struct SDCardBase {
    struct Device   sd_Device;
    struct ExecBase *sd_SysBase;
    APTR            sd_DeviceTreeBase;
};

#endif /* _SDCARD_H */
