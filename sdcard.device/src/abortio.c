/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <dos/dosextens.h>

#include <proto/exec.h>

#include "sdcard.h"

LONG SD_AbortIO(struct IORequest *io asm("a1"), struct SDCardBase * SDCardBase asm("a6"))
{
    /* AbortIO is a *wish* call. Someone would like to abort current IORequest */

    /* We cannot abort, sorry... */
    return 0;
}