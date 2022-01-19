/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>

#include "work.h"
#include "support.h"

/*
 * This Task sends work out to the "Workers" to process
 */
void SMPTestMaster()
{
    struct Task *thisTask = FindTask(NULL);
    struct SMPMaster *workMaster = thisTask->tc_UserData;
    struct SMPWorkMessage *workMsg;
    struct SMPWorker *coreWorker;

    if (workMaster)
    {
        ULONG msgWork = (workMaster->smpm_Width * workMaster->smpm_Height) / (256 / workMaster->smpm_WorkerCount);
        ULONG msgNo;

        for (msgNo = 0; msgNo < ((workMaster->smpm_Width * workMaster->smpm_Height)/ msgWork); msgNo++)
        {
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
                workMsg->smpwm_Start = msgNo * msgWork;
                workMsg->smpwm_End = workMsg->smpwm_Start + msgWork - 1;
                if  ((((workMaster->smpm_Width * workMaster->smpm_Height)/ msgWork) <= (msgNo + 1)) &&
                      (workMsg->smpwm_End < (workMaster->smpm_Width * workMaster->smpm_Height)))
                    workMsg->smpwm_End = workMaster->smpm_Width * workMaster->smpm_Height;

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
        }
    }

    workMaster->smpm_Master = NULL;
    Signal(workMaster->smpm_MasterPort->mp_SigTask, SIGBREAKF_CTRL_D);
}
