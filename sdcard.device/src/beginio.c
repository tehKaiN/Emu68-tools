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

void SD_BeginIO(struct IORequest *io asm("a1"), struct SDCardBase * SDCardBase asm("a6"))
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    io->io_Error = 0;
    io->io_Message.mn_Node.ln_Type = NT_MESSAGE;

    Disable();
    /* 
        If command is slow, clear IOF_QUICK flag and put it to some internal queue. When
        done with slow command, use ReplyMsg to notify exec that the command completed.
        In such case *do not* reply the command now.
        When modifying IoRequest, do it in disabled state.
        In case of a quick command, handle it now.
    */
    if (0 /* If command is slow */)
    {
        io->io_Flags &= ~IOF_QUICK;
        Enable();
        /* Push the command to a queue, process it maybe in another task/process, reply when
        completed */
    }
    else
    {
        Enable();
        switch(io->io_Command)
        {
            default:
                io->io_Error = IOERR_NOCMD;
                break;
        }

        /* 
            The IOF_QUICK flag was cleared. It means the caller was going to wait for command 
            completion. Therefore, reply the command now.
        */
        if (!(io->io_Flags & IOF_QUICK))
            ReplyMsg(&io->io_Message);
    }
}
