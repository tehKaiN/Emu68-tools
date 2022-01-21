/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdlib.h>

#include "work.h"
#include "support.h"

/*
 * This Task sends work out to the "Workers" to process
 */

void SMPTestMaster(struct MsgPort *mainPort)
{
    BOOL timeToDie = FALSE;
    struct Task *thisTask = FindTask(NULL);
    struct MsgPort *masterPort = CreateMsgPort();
    struct MyMessage *cmd;
    struct Task **slaves;
    struct List workPackages;

    ULONG tasksIn = 0;
    ULONG tasksOut = 0;
    ULONG width, height;
    ULONG slaveCount;
    ULONG subdivide;
    ULONG maxIterations;
    ULONG oversample;
    ULONG type;
    ULONG *workBuffer;
    double x0, y0;
    double size;
    struct SignalSemaphore *writeLock;
    struct Task *redrawTask;

    Disable();
    threadCnt++;
    Enable();

    NewList(&workPackages);

    /* Let main know master is up */
    cmd = AllocPooled(memPool, sizeof(struct MyMessage));
    cmd->mm_Type = -1;
    cmd->mm_Message.mn_Length = sizeof(struct MyMessage);
    cmd->mm_Message.mn_ReplyPort = masterPort;

    PutMsg(mainPort, &cmd->mm_Message);

    do {
        WaitPort(masterPort);
        
        while((cmd = (struct MyMessage *)GetMsg(masterPort)) != NULL)
        {
            /* In case of reply message discard it */
            if (cmd->mm_Message.mn_Node.ln_Type == NT_REPLYMSG) {
                FreePooled(memPool, cmd, cmd->mm_Message.mn_Length);
                continue;
            }

            switch(cmd->mm_Type)
            {
                case MSG_STARTUP:
                    width = cmd->mm_Body.Startup.width;
                    height = cmd->mm_Body.Startup.height;
                    slaveCount = cmd->mm_Body.Startup.threadCount;
                    subdivide = cmd->mm_Body.Startup.subdivide;
                    maxIterations = cmd->mm_Body.Startup.maxIterations;
                    oversample = cmd->mm_Body.Startup.oversample;
                    type = cmd->mm_Body.Startup.type;
                    workBuffer = cmd->mm_Body.Startup.workBuffer;
                    x0 = cmd->mm_Body.Startup.x0;
                    y0 = cmd->mm_Body.Startup.y0;
                    size = cmd->mm_Body.Startup.size;
                    writeLock = cmd->mm_Body.Startup.writeLock;
                    redrawTask = cmd->mm_Body.Startup.redrawTask;

                    /*
                        Now we have necessary information to prepare work packages. Get
                        entire number of pixels of the image and split that across threads.
                        Use subdivide to make the work packages smaller.
                    */
                    ULONG workPortion = (width * height) / subdivide;
                    ULONG msgStart = 0;
                    const ULONG msgStop = width * height;

                    if (workPortion < 20)
                        workPortion = 20;

                    while (msgStart < msgStop)
                    {
                        ULONG msgEnd = msgStart + workPortion - 1;

                        if (msgEnd >= msgStop)
                            msgEnd = msgStop - 1;

                        struct MyMessage *m = AllocPooled(memPool, sizeof(struct MyMessage));
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        m->mm_Message.mn_ReplyPort = masterPort;
                        m->mm_Type = MSG_WORKPACKAGE;
                        m->mm_Body.WorkPackage.width = width;
                        m->mm_Body.WorkPackage.height = height;
                        m->mm_Body.WorkPackage.maxIterations = maxIterations;
                        m->mm_Body.WorkPackage.oversample = oversample;
                        m->mm_Body.WorkPackage.type = type;
                        m->mm_Body.WorkPackage.workBuffer = workBuffer;
                        m->mm_Body.WorkPackage.x0 = x0;
                        m->mm_Body.WorkPackage.y0 = y0;
                        m->mm_Body.WorkPackage.size = size;
                        m->mm_Body.WorkPackage.writeLock = writeLock;
                        m->mm_Body.WorkPackage.workStart = msgStart;
                        m->mm_Body.WorkPackage.workEnd = msgEnd;
                        m->mm_Body.WorkPackage.redrawTask = redrawTask;

                        m->mm_Message.mn_Node.ln_Pri = rand() & 0xff;

                        Enqueue(&workPackages, &m->mm_Message.mn_Node);                       
                        
                        tasksIn++;

                        msgStart = msgEnd + 1;
                    }

                    /*
                        Create requested number of threads, every of them will send MSG_HUNGRY message
                    */
                    slaves = AllocPooled(memPool, slaveCount * sizeof(APTR));
                    for (int i=0; i < slaveCount; i++)
                    {
                        char tmpbuf[32];
                        _sprintf(tmpbuf, "Buddha Worker #%d", i);
                        slaves[i] = NewCreateTask(
                                        TASKTAG_NAME,       (Tag)tmpbuf,
                                        TASKTAG_PRI,        -1,
                                        TASKTAG_PC,         (Tag)SMPTestWorker,
                                        TASKTAG_ARG1,       (Tag)masterPort,
                                        TASKTAG_STACKSIZE,  65536,
                                        TAG_DONE);
                    }

                    ReplyMsg(&cmd->mm_Message);
                    break;
                
                case MSG_HUNGRY:
                    /* Reply immediately, prepare task later */
                    ReplyMsg(&cmd->mm_Message);

                    /*  
                        Slave is asking for work, as long as we have some, send it.
                        If the list of packages is empty, we don't need the slave anymore.
                        Send a kiss of death in that case
                    */
                    if (!IsListEmpty(&workPackages))
                    {
                        struct MyMessage *m = (struct MyMessage *)RemHead(&workPackages);
                        struct MsgPort *p = cmd->mm_Message.mn_ReplyPort;
                        tasksOut++;
                        tasksIn--;
                        
                        PutMsg(p, &m->mm_Message);
                    }
                    else
                    {
                        struct MyMessage *m = AllocPooled(memPool, sizeof(struct MyMessage));
                        struct MsgPort *p = cmd->mm_Message.mn_ReplyPort;

                        m->mm_Type = MSG_DIE;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        m->mm_Message.mn_ReplyPort = masterPort;

                        PutMsg(p, &m->mm_Message);
                    }
                    {
                        struct MyMessage *m = AllocPooled(memPool, sizeof(struct MyMessage));
                        m->mm_Type = MSG_STATS;
                        m->mm_Message.mn_ReplyPort = masterPort;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        m->mm_Body.Stats.tasksIn = tasksIn;
                        m->mm_Body.Stats.tasksOut = tasksOut;
                        m->mm_Body.Stats.tasksWork = slaveCount;
                        PutMsg(mainPort, &m->mm_Message);
                    }
                    break;
                
                case MSG_DONE:
                    /* Slave told that it is goind to die now, accept it, discard message and decrease slave counter */
                    FreePooled(memPool, cmd, cmd->mm_Message.mn_Length);
                    
                    if (slaveCount != 0)
                        slaveCount--;

                    /*
                        All slaves are gone. Let main know
                    */
                    if (slaveCount == 0)
                    {
                        struct MyMessage *m = AllocPooled(memPool, sizeof(struct MyMessage));
                        m->mm_Type = MSG_DONE;
                        m->mm_Message.mn_ReplyPort = masterPort;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);

                        PutMsg(mainPort, &m->mm_Message);
                    }
                    break;

                case MSG_DIE:
                    /* Don't reply DIE message. Free it instead */
                    FreePooled(memPool, cmd, cmd->mm_Message.mn_Length);
                    
                    /* Purge remaining tasks */
                    if (!IsListEmpty(&workPackages))
                    {
                        struct MyMessage *m;
                        while ((m = (struct MyMessage *)RemHead(&workPackages)) != NULL)
                        {
                            FreePooled(memPool, m, m->mm_Message.mn_Length);
                        }
                    }
                    timeToDie = TRUE;
                    break;
            }
        }
    } while(!timeToDie || slaveCount != 0);

    DeleteMsgPort(masterPort);

    if (slaves)
        FreePooled(memPool, slaves, slaveCount * sizeof(APTR));
    
    Disable();
    threadCnt--;
}
