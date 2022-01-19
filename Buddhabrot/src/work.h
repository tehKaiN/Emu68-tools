/*
    Copyright � 2017, The AROS Development Team. All rights reserved.
    $Id$
*/

#include <exec/tasks.h>
#include <exec/ports.h>
#include <exec/semaphores.h>

struct SMPMaster
{
    ULONG                       smpm_WorkerCount;
    struct List                 smpm_Workers;
    struct Task                 *smpm_Master;
    struct MsgPort              *smpm_MasterPort;
    ULONG                       *smpm_WorkBuffer;
    UWORD                       smpm_Width;
    UWORD                       smpm_Height;    
    struct SignalSemaphore      smpm_Lock;
    ULONG                       smpm_MaxWork;
    ULONG                       smpm_Oversample;
    BOOL                        smpm_Buddha;
};

struct SMPWorker
{
    struct Node                 smpw_Node;
    struct Task                 *smpw_Task;
    struct MsgPort              *smpw_MasterPort;
    struct MsgPort              *smpw_MsgPort;
    struct Task                 *smpw_SyncTask;
    struct SignalSemaphore      *smpw_Lock;
    ULONG                       smpw_MaxWork;
    ULONG                       smpw_Oversample;
};

struct SMPWorkMessage
{
    struct Message              smpwm_Msg;
    ULONG                       smpwm_Type;
    ULONG                       *smpwm_Buffer;
    ULONG                       smpwm_Width;
    ULONG                       smpwm_Height;
    ULONG                       smpwm_Start;
    ULONG                       smpwm_End;
    struct SignalSemaphore      *smpwm_Lock;
};

#define SPMWORKTYPE_FINISHED    (1 << 0)
#define SPMWORKTYPE_MANDLEBROT  (1 << 1)
#define SPMWORKTYPE_BUDDHA      (1 << 2)

void SMPTestMaster();
void SMPTestWorker();
