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

ULONG WorkSubdivide = 256;

void SMPTestMaster(struct MsgPort *mainPort)
{
    struct Task *thisTask = FindTask(NULL);
    struct SMPMaster *workMaster = thisTask->tc_UserData;
    struct SMPWorkMessage *workMsg;
    struct SMPWorker *coreWorker;
    struct MsgPort *masterPort;
    
    NewList(&workMaster->smpm_Tasks);

    WaitPort(mainPort);


    if (workMaster)
    {
        ULONG workPortion = ((workMaster->smpm_Width * workMaster->smpm_Height) / workMaster->smpm_WorkerCount) / WorkSubdivide; 
        ULONG msgStart = 0;
        const ULONG msgStop = workMaster->smpm_Width * workMaster->smpm_Height;

        if (workPortion == 0)
            workPortion = 1;

        while (msgStart < msgStop)
        {
            ULONG msgEnd = msgStart + workPortion - 1;

            if ((workMsg = (struct SMPWorkMessage *)AllocMem(sizeof(struct SMPWorkMessage), MEMF_CLEAR)) != NULL)
            {
                /* prepare the work to be done ... */
                if (workMaster->smpm_Buddha)
                    workMsg->smpwm_Type = SPMWORKTYPE_BUDDHA;
                else
                    workMsg->smpwm_Type = SPMWORKTYPE_MANDLEBROT;

                workMsg->smpwm_Buffer = workMaster->smpm_WorkBuffer;
                workMsg->smpwm_Width = workMaster->smpm_Width;
                workMsg->smpwm_Height = workMaster->smpm_Height;
                workMsg->smpwm_Start = msgStart;
                workMsg->smpwm_End = msgEnd;

                /* send out the work to an available worker... */
                do {
                    ForeachNode(&workMaster->smpm_Workers, coreWorker)
                    {
                        if ((workMsg) && (coreWorker->smpw_Node.ln_Type == 1) && (coreWorker->smpw_MsgPort))
                        {
                            coreWorker->smpw_Node.ln_Type = 0;
                            PutMsg(coreWorker->smpw_MsgPort, (struct Message *)workMsg);
                            workMsg = NULL;
                            break;
                        }
                    }
                } while (workMsg);
            }

            msgStart = msgEnd + 1;
        }
    }

    workMaster->smpm_Master = NULL;
    Signal(workMaster->smpm_MasterPort->mp_SigTask, SIGBREAKF_CTRL_D);
}
