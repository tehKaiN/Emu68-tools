#ifndef _SDCARD_H
#define _SDCARD_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/libraries.h>
#include <exec/devices.h>

struct SDCardBase {
    struct Device   sd_Device;
    APTR            sd_DeviceTreeBase;
};

#endif /* _SDCARD_H */
