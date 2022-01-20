/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

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
    struct MyMessage msg;
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

    Disable();
    threadCnt++;
    Enable();

    NewList(&workPackages);

    msg.mm_Type = -1;
    msg.mm_Message.mn_Length = sizeof(msg);
    msg.mm_Message.mn_ReplyPort = masterPort;

    PutMsg(mainPort, &msg.mm_Message);

    do {
        struct MyMessage *cmd;

        WaitPort(masterPort);
        
        while((cmd = (struct MyMessage *)GetMsg(masterPort)) != NULL)
        {
            /* If we receive our own message, ignore it */
            if (cmd == &msg)
                continue;

            /* In case of reply message discard it */
            if (cmd->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                FreeMem(cmd, cmd->mm_Message.mn_Length);

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

                        struct MyMessage *m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
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

                        AddTail(&workPackages, &m->mm_Message.mn_Node);
                        tasksIn++;

                        msgStart = msgEnd + 1;
                    }

                    /*
                        Create requested number of threads, every of them will send MSG_HUNGRY message
                    */
                    slaves = AllocMem(slaveCount * sizeof(APTR), MEMF_PUBLIC);
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
                    /* Slave is asking for work, as long as we have some, send it */
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
                        struct MyMessage *m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
                        struct MsgPort *p = cmd->mm_Message.mn_ReplyPort;

                        m->mm_Type = MSG_DIE;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        m->mm_Message.mn_ReplyPort = masterPort;

                        PutMsg(p, &m->mm_Message);
                        if (slaveCount != 0)
                            slaveCount--;

                        if (slaveCount == 0)
                        {
                            m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
                            m->mm_Type = MSG_DONE;
                            m->mm_Message.mn_ReplyPort = masterPort;
                            m->mm_Message.mn_Length = sizeof(struct MyMessage);

                            PutMsg(mainPort, &m->mm_Message);
                        }
                    }
                    {
                        struct MyMessage *m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
                        m->mm_Type = MSG_REDRAW;
                        m->mm_Message.mn_ReplyPort = masterPort;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        PutMsg(mainPort, &m->mm_Message);

                        m = AllocMem(sizeof(struct MyMessage), MEMF_ANY);
                        m->mm_Type = MSG_STATS;
                        m->mm_Message.mn_ReplyPort = masterPort;
                        m->mm_Message.mn_Length = sizeof(struct MyMessage);
                        m->mm_Body.Stats.tasksIn = tasksIn;
                        m->mm_Body.Stats.tasksOut = tasksOut;
                        m->mm_Body.Stats.tasksWork = slaveCount;
                        PutMsg(mainPort, &m->mm_Message);
                    }
                    break;

                case MSG_DIE:
                    /* Don't reply DIE message. Free it instead */
                    FreeMem(cmd, cmd->mm_Message.mn_Length);
                    
                    /* Purge remaining tasks */
                    if (!IsListEmpty(&workPackages))
                    {
                        struct MyMessage *m;
                        while ((m = (struct MyMessage *)RemHead(&workPackages)) != NULL)
                        {
                            FreeMem(m, m->mm_Message.mn_Length);
                        }
                    }
                    timeToDie = TRUE;
                    break;
            }
        }
    } while(!(timeToDie && slaveCount == 0));

    DeleteMsgPort(masterPort);

    if (slaves)
        FreeMem(slaves, slaveCount * sizeof(APTR));
}
