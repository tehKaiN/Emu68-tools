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
    const ULONG trajectoryCapacity = msg->mm_Body.WorkPackage.maxIterations * 2;
    ULONG trajectoryCurr = 0;
#if 0
    const ULONG maxLength = 1000;
    ULONG currLength = 0;
#endif

    const double x_0 = msg->mm_Body.WorkPackage.x0;
    const double y_0 = msg->mm_Body.WorkPackage.y0;
    const double size = msg->mm_Body.WorkPackage.size;

    double x, y;
    double diff = size / ((double)(msg->mm_Body.WorkPackage.width * msg->mm_Body.WorkPackage.oversample));
    double y_base = size / 2.0 - (diff * 2.0 / size);
    double diff_sr = size / (double)msg->mm_Body.WorkPackage.width;

    complexno_t *workTrajectories = NULL;
    
    if (msg->mm_Body.WorkPackage.type == TYPE_BUDDHA)
    {
        workTrajectories = AllocMem(trajectoryCapacity * sizeof(complexno_t), MEMF_CLEAR|MEMF_ANY);
    }

    for (current = msg->mm_Body.WorkPackage.workStart * workOver2;
         current < (msg->mm_Body.WorkPackage.workEnd + 1) * workOver2;
         current++)
    {
        ULONG val;

        /* Locate the point on the complex plane */
        x = x_0 + ((double)(current % (workWidth * msg->mm_Body.WorkPackage.oversample))) * diff - size / 2.0;
        y = y_0 + ((double)(current / (workWidth * msg->mm_Body.WorkPackage.oversample))) * diff - y_base;

        if (msg->mm_Body.WorkPackage.type == TYPE_BUDDHA)
        {
            /* Calculate the points trajectory ... */
            trajectoryLength = calculateTrajectory(&workTrajectories[trajectoryCurr], msg->mm_Body.WorkPackage.maxIterations, x, y);
            trajectoryCurr += trajectoryLength;
#if 0
            currLength += trajectoryLength;

            if (currLength >= maxLength)
            {
                currLength = 0;
                Signal(msg->mm_Body.WorkPackage.redrawTask, SIGBREAKF_CTRL_D);
            }
#endif
            /* If there is no place for next iteration, flush the trajectory */
            if (trajectoryCapacity - trajectoryCurr < msg->mm_Body.WorkPackage.maxIterations)
            {
                ULONG pos;
                int i;
                ObtainSemaphore(msg->mm_Body.WorkPackage.writeLock);
                Forbid();
                for(i = 0; i < trajectoryCurr; i++)
                {
                    ULONG py = (workTrajectories[i].r - x_0 + size / 2.0) / diff_sr;
                    ULONG px = (workTrajectories[i].i - y_0 + y_base) / diff_sr;

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
                trajectoryCurr = 0;
            }
        }
        else
        {
            trajectoryLength = calculateTrajectory(NULL, msg->mm_Body.WorkPackage.maxIterations, x, y);

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

    /* For buddha type, flush trajectory buffer */
    if (msg->mm_Body.WorkPackage.type == TYPE_BUDDHA)
    {
        if (trajectoryCurr != 0)
        {
            ULONG pos;
            int i;
            ObtainSemaphore(msg->mm_Body.WorkPackage.writeLock);
            for(i = 0; i < trajectoryCurr; i++)
            {
                ULONG py = (workTrajectories[i].r - y_0 + size / 2.0) / diff_sr;
                ULONG px = (workTrajectories[i].i - x_0 + y_base) / diff_sr;

                pos = (ULONG)(workWidth * py + px);

                if (pos > 0 && pos < (workWidth * workHeight))
                {

                    ULONG val = msg->mm_Body.WorkPackage.workBuffer[pos];
                    
                    if (val < 0xfff)
                        val++;

                    msg->mm_Body.WorkPackage.workBuffer[pos] = val;

                }
            }
            ReleaseSemaphore(msg->mm_Body.WorkPackage.writeLock);
            trajectoryCurr = 0;
        }
    }

    Signal(msg->mm_Body.WorkPackage.redrawTask, SIGBREAKF_CTRL_D);

    if (workTrajectories)
    {
        FreeMem(workTrajectories, trajectoryCapacity * sizeof(complexno_t));
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
