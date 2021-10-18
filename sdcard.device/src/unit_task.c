#include <exec/tasks.h>
#include <exec/execbase.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>

#include <devices/hardblocks.h>

#include <inline/alib.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include "sdcard.h"

static void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

static void MountPartitions(struct SDCardUnit *unit)
{
    struct SDCardBase *SDCardBase = unit->su_Base;
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    struct ExpansionBase *ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 36);
    BOOL rdsk = FALSE;

    union
    {
        struct RigidDiskBlock   rdsk;
        struct PartitionBlock   part;
        ULONG                   ublock[512/4];
        UBYTE                   bblock[512];
    } buff;

    {
        ULONG args[] = {
            unit->su_UnitNum
        };
        RawDoFmt("[brcm-sdhc:%ld] MountPartitions\n", args, (APTR)putch, NULL);
    }

    /* RigidDiskBlock has to be found within first 16 sectors. Check them now */
    for (int i=0; i < RDB_LOCATION_LIMIT; i++)
    {
        SDCardBase->sd_Read((APTR)buff.bblock, 512, unit->su_StartBlock + i, SDCardBase);

        /* Do we have 'RDSK' header? */
        if (buff.ublock[0] == IDNAME_RIGIDDISK)
        {
            ULONG cnt = buff.rdsk.rdb_SummedLongs;
            ULONG sum = 0;

            /* Header was found. Checksum the block now */
            for (int x = 0; x < cnt; x++) {
                sum += buff.ublock[x];
            }

            /* If sum == 0 then the block can be considered valid. Stop checking now */
            if (sum == 0)
            {
                rdsk = TRUE;
                break;
            }            
        }
    }
    
    /* If we have found RDSK, check for any partitions now */
    if (rdsk)
    {
        ULONG next_part = buff.rdsk.rdb_PartitionList;
        ULONG filesys_header = buff.rdsk.rdb_FileSysHeaderList;

        /* For every partition do the same. Load, parse, eventually mount, eventually add to boot node */
        while(next_part != 0xffffffff)
        {
            SDCardBase->sd_Read((APTR)buff.bblock, 512, unit->su_StartBlock + next_part, SDCardBase);

            if (buff.ublock[0] == IDNAME_PARTITION)
            {
                ULONG cnt = buff.part.pb_SummedLongs;
                ULONG sum = 0;

                /* Header was found. Checksum the block now */
                for (int x = 0; x < cnt; x++) {
                    sum += buff.ublock[x];
                }

                /* If sum == 0 then the block can be considered valid. */
                if (sum == 0)
                {
                    next_part = buff.part.pb_Next;

                    /* If NOMOUNT *is not* set, attempt to mount the partition */
                    if ((buff.part.pb_Flags & PBFF_NOMOUNT) == 0)
                    {
                        ULONG *paramPkt = AllocMem(24 * sizeof(ULONG), MEMF_PUBLIC);
                        UBYTE *name = AllocMem(buff.part.pb_DriveName[0] + 5, MEMF_PUBLIC);

                
                        for (int i=0; i < buff.part.pb_DriveName[0]; i++) {
                            name[i] = buff.part.pb_DriveName[1+i];
                        }
                        name[buff.part.pb_DriveName[0]] = 0;

                        paramPkt[0] = (ULONG)name;
                        paramPkt[1] = (ULONG)SDCardBase->sd_Device.dd_Library.lib_Node.ln_Name;
                        paramPkt[2] = unit->su_UnitNum;
                        paramPkt[3] = 0;
                        for (int i=0; i < 20; i++) {
                            paramPkt[4+i] = buff.part.pb_Environment[i];
                        }

                        struct DeviceNode *devNode = MakeDosNode(paramPkt);
                        AddBootNode(paramPkt[DE_BOOTPRI + 4], 0, devNode, SDCardBase->sd_ConfigDev);
                    }
                }
            }
        }
    }

    CloseLibrary((struct Library *)ExpansionBase);
}

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

    ObtainSemaphore(&SDCardBase->sd_Lock);

    MountPartitions(unit);

    ReleaseSemaphore(&SDCardBase->sd_Lock);

    Signal(unit->su_Caller, SIGBREAKF_CTRL_C);

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