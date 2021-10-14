#include <exec/tasks.h>
#include <exec/execbase.h>

#include <inline/alib.h>

#include <proto/exec.h>

#include "sdcard.h"

void UnitTask()
{
    struct ExecBase *SysBase = *(struct ExecBase**)4UL;

    struct SDCardUnit *unit;
    struct SDCardBase *SDCardBase;
    struct Task *task;
      
    task = FindTask(NULL);
    unit = task->tc_UserData;
    SDCardBase = unit->su_Base;

    NewList(&unit->su_Unit.unit_MsgPort.mp_MsgList);
    unit->su_Unit.unit_MsgPort.mp_SigTask = task;
    unit->su_Unit.unit_MsgPort.mp_SigBit = AllocSignal(-1);
    unit->su_Unit.unit_MsgPort.mp_Flags = PA_SIGNAL;
    unit->su_Unit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;

    while(1) {
        struct IORequest *io;
        WaitPort(&unit->su_Unit.unit_MsgPort);
        while(io = (struct IORequest *)GetMsg(&unit->su_Unit.unit_MsgPort))
        {
            SDCardBase->sd_DoIO(io, SDCardBase);
            io->io_Message.mn_Node.ln_Type = NT_MESSAGE;
            ReplyMsg(&io->io_Message);
        }
    }
        
}