/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

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
    APTR                sd_MailBox;
    APTR                sd_SDHC;
    ULONG *             sd_Request;
    APTR                sd_RequestBase;
};

#define SDCARD_VERSION  0
#define SDCARD_REVISION 1
#define SDCARD_PRIORITY 20

#define BASE_NEG_SIZE   (6 * 6)
#define BASE_POS_SIZE   (sizeof(struct SDCardBase))

#endif /* _SDCARD_H */
