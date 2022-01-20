/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/semaphores.h>
#include <dos/dos.h>
#include <clib/exec_protos.h>

#include "work.h"
#include "support.h"

#define DWORK(x)

/*
 * define a complex number with
 * real and imaginary parts
 */
typedef struct {
	double r;
	double i;
} complexno_t;

ULONG calculateTrajectory(complexno_t *workTrajectories, ULONG workMax, double r, double i)
{
    double realNo, imaginaryNo, realNo2, imaginaryNo2, tmp;
    ULONG trajectory;

    /* Calculate trajectory */
    realNo = 0;
    imaginaryNo = 0;

    for(trajectory = 0; trajectory < workMax; trajectory++)
    {
        /* Check if it's out of circle with radius 2 */
        realNo2 = realNo * realNo;
        imaginaryNo2 = imaginaryNo * imaginaryNo;

        if (realNo2 + imaginaryNo2 > 4.0)
            return trajectory;

        /* Next */
        tmp = realNo2 - imaginaryNo2 + r;
        imaginaryNo = 2.0 * realNo * imaginaryNo + i;
        realNo = tmp;

        /* Store */
        if (workTrajectories)
        {
            workTrajectories[trajectory].r = realNo;
            workTrajectories[trajectory].i = imaginaryNo;
        }
    }

    return 0;
}

void processWork(struct MyMessage *msg)
{
    ULONG trajectoryLength;
    ULONG current;
    const ULONG workOver2 = msg->mm_Body.WorkPackage.oversample * msg->mm_Body.WorkPackage.oversample;
    const ULONG workWidth = msg->mm_Body.WorkPackage.width;
    const ULONG workHeight = msg->mm_Body.WorkPackage.height;
    
    double x_0 = msg->mm_Body.WorkPackage.x0;
    double y_0 = msg->mm_Body.WorkPackage.y0;
    double size = msg->mm_Body.WorkPackage.size;

    double x, y;
    double diff = size / ((double)(msg->mm_Body.WorkPackage.width * msg->mm_Body.WorkPackage.oversample));
    double y_base = size / 2.0 - (diff * 2.0 / size);
    double diff_sr = size / (double)msg->mm_Body.WorkPackage.width;

    complexno_t *workTrajectories = NULL;
    
    if (msg->mm_Body.WorkPackage.type == TYPE_BUDDHA)
    {
        workTrajectories = AllocMem(sizeof(complexno_t) * msg->mm_Body.WorkPackage.maxIterations, MEMF_CLEAR|MEMF_ANY);
    }

    for (current = msg->mm_Body.WorkPackage.workStart * workOver2;
         current < (msg->mm_Body.WorkPackage.workEnd + 1) * workOver2;
         current++)
    {
        ULONG val;

        /* Locate the point on the complex plane */
        x = x_0 + ((double)(current % (workWidth * msg->mm_Body.WorkPackage.oversample))) * diff - size / 2.0;
        y = y_0 + ((double)(current / (workWidth * msg->mm_Body.WorkPackage.oversample))) * diff - y_base;

        /* Calculate the points trajectory ... */
        trajectoryLength = calculateTrajectory(workTrajectories, msg->mm_Body.WorkPackage.maxIterations, x, y);

        if (msg->mm_Body.WorkPackage.type == TYPE_BUDDHA)
        {
            /* Update the display if it escapes */
            if (trajectoryLength > 0)
            {
                ULONG pos;
                int i;
                ObtainSemaphore(msg->mm_Body.WorkPackage.writeLock);
                for(i = 0; i < trajectoryLength; i++)
                {
                    ULONG py = (workTrajectories[i].r - y_0 + size / 2.0) / diff_sr;
                    ULONG px = (workTrajectories[i].i - x_0 + y_base) / diff_sr;

                    pos = (ULONG)(workWidth * py + px);

                    if (pos > 0 && pos < (workWidth * workHeight))
                    {

                        val = msg->mm_Body.WorkPackage.workBuffer[pos];
                        
                        if (val < 0xfff)
                            val++;

                        msg->mm_Body.WorkPackage.workBuffer[pos] = val;

                    }
                }
                ReleaseSemaphore(msg->mm_Body.WorkPackage.writeLock);
            }
        }
        else
        {
            (void)diff_sr;

            ULONG px = (current % (workWidth * msg->mm_Body.WorkPackage.oversample)) / msg->mm_Body.WorkPackage.oversample;
            ULONG py = current / (workWidth * workOver2);

            ULONG pos = px + py * workWidth;

            val = msg->mm_Body.WorkPackage.workBuffer[pos];

            val+= 10*trajectoryLength / msg->mm_Body.WorkPackage.oversample;
            
            if (val > 0xfff)
                val = 0xfff;

            msg->mm_Body.WorkPackage.workBuffer[pos] = val;
        }
    }

    if (workTrajectories)
    {
        FreeMem(workTrajectories, sizeof(complexno_t) * msg->mm_Body.WorkPackage.maxIterations);
    }
}

/*
 * This Task processes work received
 */
void SMPTestWorker(struct MsgPort *masterPort)
{
    struct Task *thisTask = FindTask(NULL);
    struct MsgPort *port = CreateMsgPort();
    struct MyMessage cmd;

    Disable();
    threadCnt++;
    Enable();

    cmd.mm_Message.mn_Length = sizeof(cmd);
    cmd.mm_Message.mn_ReplyPort = port;

    if (port)
    {
        BOOL done = FALSE;

        cmd.mm_Type = MSG_HUNGRY;
        PutMsg(masterPort, &cmd.mm_Message);

        while(!done)
        {
            struct MyMessage *msg;

            WaitPort(port);
            while((msg = (struct MyMessage *)GetMsg(port)) != NULL)
            {
                /* If we receive our own message, ignore it */
                if (&cmd == msg)
                    continue;
                
                /* Ignore reply messages */
                if (msg->mm_Message.mn_Node.ln_Type == NT_REPLYMSG)
                    continue;

                if (msg->mm_Type == MSG_WORKPACKAGE)
                {
                    processWork(msg);
                    ReplyMsg(&msg->mm_Message);
                    cmd.mm_Type = MSG_HUNGRY;
                    PutMsg(masterPort, &cmd.mm_Message);
                }
                else if (msg->mm_Type == MSG_DIE)
                {
                    Forbid();
                    done = TRUE;
                    FreeMem(msg, msg->mm_Message.mn_Length);
                }
            }
        }
    }

    /* Drain messages from the port */
    if (port)
    {
        struct MyMessage *msg;
        while ((msg = (struct MyMessage *)GetMsg(port)) != NULL);
    }

    DeleteMsgPort(port);
}
