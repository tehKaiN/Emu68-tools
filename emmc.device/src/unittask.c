#include <exec/tasks.h>
#include <exec/execbase.h>
#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <resources/filesysres.h>

#include <dos/doshunks.h>
#include <devices/hardblocks.h>

#include <inline/alib.h>

#include <proto/exec.h>
#include <proto/expansion.h>

#include "emmc.h"

struct RelocHunk
{
	ULONG hunkSize;
	ULONG *hunkData;
};

struct SmartBuffer
{
    ULONG pos;
    ULONG size;
    ULONG *buffer;
};

static int lseg_read_long(struct SmartBuffer *buff, ULONG *ptr)
{
    if (buff->pos < (buff->size >> 2)) {
        *ptr = buff->buffer[buff->pos++];
        return 1;
    }
    else 
        return 0;
}

static ULONG LoadSegBlock(struct SmartBuffer *bu)
{
    struct ExecBase *SysBase = *(struct ExecBase **)4UL;
    ULONG data;
    APTR ret;
	LONG firstHunk, lastHunk;
    ULONG *words;
    struct RelocHunk *rh;
    ULONG current_hunk = 0;

    if (bu->size < 4 || bu->buffer[0] != HUNK_HEADER) {
		return 0;
	}

    /* Parse header */
    firstHunk = bu->buffer[3];
    lastHunk = bu->buffer[4];

    words = &bu->buffer[5];

    rh = AllocMem(sizeof(struct RelocHunk) * (lastHunk - firstHunk + 1), MEMF_PUBLIC);

    /* Pre-allocate memory for all loadable hunks */
    for (unsigned i = 0; i < lastHunk - firstHunk + 1; i++)
    {
        ULONG size = *words++;
        ULONG requirements = MEMF_PUBLIC;
        if (size & (HUNKF_CHIP | HUNKF_FAST) == (HUNKF_CHIP | HUNKF_FAST))
        {
            requirements = *words++;
        }
        else if (size & HUNKF_CHIP)
        {
            requirements |= MEMF_CHIP;
        }
        size &= ~(HUNKF_CHIP | HUNKF_FAST);

        rh[i].hunkSize = size + 2;
        rh[i].hunkData = AllocMem(sizeof(ULONG) * (size + 2), requirements | MEMF_CLEAR);
        
        /* First ULONG gives Segment size in ULONG units */
        rh[i].hunkData[0] = rh[i].hunkSize;
    }

    /* Adjust seglist-pointers to next element */
    for (unsigned i = 0; i < lastHunk - firstHunk; i++)
    {
        rh[i].hunkData[1] = MKBADDR(&rh[i+1].hunkData[1]);
    }

    /* Load and relocate hunks one after another */
    do
    {
        ULONG hunk_size;
        ULONG hunk_type;

        switch((hunk_type = *words))
        {
            case HUNK_CODE: // Fallthrough
            case HUNK_DATA: // Fallthrough
            case HUNK_BSS:
                hunk_size = words[1];
                if (current_hunk >= firstHunk) {
                    if (hunk_type != HUNK_BSS) {
                        CopyMem(&words[2], &rh[current_hunk].hunkData[2], hunk_size * 4);
                    }
                }
                words += 2 + hunk_size;
                break;
            
            case HUNK_RELOC32:  // Fallthrough
            case HUNK_RELOC32SHORT:
            {
                ULONG relocCnt, relocHunk;
                ULONG word_cache;
                int c = 0;

                words++;

                for (;;)
                {
                    relocCnt = *words++;
                    if (!relocCnt)
                        break;

                    relocHunk = *words++ - firstHunk;

                    struct RelocHunk *rhr = &rh[relocHunk];
                    while(relocCnt != 0)
                    {
                        ULONG relocOffset;
						if (hunk_type == HUNK_RELOC32SHORT) {
                            if (c == 0) {
                                word_cache = *words++;
                            }
                            relocOffset = word_cache;
                            c++;
                            if (c == 1) {
                                relocOffset >>= 16;
                            }
                            else 
                            {
                                relocOffset &= 0xffff;
                                c = 0;
                            }
						} else {
                            relocOffset = *words++;
						}

                        if (current_hunk >= firstHunk) {
                            ULONG *data = (ULONG*)((ULONG)(rh[current_hunk - firstHunk].hunkData + 2) + relocOffset);
                            *data += (ULONG)(rhr->hunkData + 2);
                        }

						relocCnt--;
                    }
                }

                break;
            }

            case HUNK_SYMBOL:
                words++;
                while(words[0] != 0)
                {
                    words += words[0] + 2;
                }
                words++;
                break;

            case HUNK_END:
                words++;
                current_hunk++;
                break;
        }
    } while(current_hunk <= lastHunk);

    if (rh) {
        ret = &rh[0].hunkData[1];
        FreeMem(rh, sizeof(struct RelocHunk) * (lastHunk - firstHunk + 1));
    }

    return MKBADDR(ret);   
}

static void LoadFilesystem(struct EMMCUnit *unit, ULONG dosType)
{
    struct EMMCBase *EMMCBase = unit->su_Base;
    struct ExecBase *SysBase = EMMCBase->emmc_SysBase;
    BOOL rdsk = FALSE;

    union
    {
        struct RigidDiskBlock       rdsk;
        struct PartitionBlock       part;
        struct FileSysHeaderBlock   fshd;
        struct LoadSegBlock         lseg;
        ULONG                       ublock[512/4];
        UBYTE                       bblock[512];
    } *buff = AllocMem(512, MEMF_PUBLIC);

    /* RigidDiskBlock has to be found within first 16 sectors. Check them now */
    for (int i=0; i < RDB_LOCATION_LIMIT; i++)
    {
        emmc_read((APTR)buff->bblock, 512, unit->su_StartBlock + i, EMMCBase);

        /* Do we have 'RDSK' header? */
        if (buff->ublock[0] == IDNAME_RIGIDDISK)
        {
            ULONG cnt = buff->rdsk.rdb_SummedLongs;
            ULONG sum = 0;

            /* Header was found. Checksum the block now */
            for (int x = 0; x < cnt; x++) {
                sum += buff->ublock[x];
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
        ULONG filesys_header = buff->rdsk.rdb_FileSysHeaderList;

        /* For every FileSysHeader block, load, verify, check if this is the one we need */
        while(filesys_header != 0xffffffff)
        {
            emmc_read((APTR)buff->bblock, 512, unit->su_StartBlock + filesys_header, EMMCBase);

            if (buff->ublock[0] == IDNAME_FILESYSHEADER)
            {
                ULONG cnt = buff->fshd.fhb_SummedLongs;
                ULONG sum = 0;

                /* Header was found. Checksum the block now */
                for (int x = 0; x < cnt; x++) {
                    sum += buff->ublock[x];
                }

                /* If sum == 0 then the block can be considered valid. */
                if (sum == 0)
                {
                    filesys_header = buff->fshd.fhb_Next;

                    if (EMMCBase->emmc_Verbose)
                    {
                        bug("[brcm-sdhc:%ld] Checking FSHD for DosType %08lx\n", unit->su_UnitNum, buff->fshd.fhb_DosType);
                    }

                    if (buff->fshd.fhb_DosType == dosType)
                    {
                        APTR buffer = AllocMem(65536, MEMF_PUBLIC);
                        ULONG buffer_size = 65536;
                        ULONG loaded = 0;

                        if (EMMCBase->emmc_Verbose)
                        {
                            bug("[brcm-sdhc:%ld] DOSType match!\n", unit->su_UnitNum);
                        }   

                        // Check LSEG location
                        ULONG lseg = buff->fshd.fhb_SegListBlocks;

                        union
                        {
                            struct RigidDiskBlock       rdsk;
                            struct PartitionBlock       part;
                            struct FileSysHeaderBlock   fshd;
                            struct LoadSegBlock         lseg;
                            ULONG                       ublock[512/4];
                            UBYTE                       bblock[512];
                        } *ls_buff = AllocMem(512, MEMF_PUBLIC);

                        while(lseg != 0xffffffff)
                        {
                            emmc_read((APTR)ls_buff->bblock, 512, unit->su_StartBlock + lseg, EMMCBase);

                            if (ls_buff->ublock[0] == IDNAME_LOADSEG)
                            {
                                ULONG cnt = ls_buff->lseg.lsb_SummedLongs;
                                ULONG sum = 0;

                                /* Header was found. Checksum the block now */
                                for (int x = 0; x < cnt; x++) {
                                    sum += ls_buff->ublock[x];
                                }

                                /* If sum == 0 then the block can be considered valid. */
                                if (sum == 0)
                                {
                                    lseg = ls_buff->lseg.lsb_Next;

                                    if (loaded + 4*123 > buffer_size) {
                                        APTR new_buff = AllocMem(buffer_size + 65536, MEMF_PUBLIC);
                                        CopyMemQuick(buffer, new_buff, buffer_size);
                                        FreeMem(buffer, buffer_size);
                                        buffer = new_buff;
                                        buffer_size += 65536;
                                    }

                                    CopyMemQuick(ls_buff->lseg.lsb_LoadData, buffer + loaded, 4 * 123);

                                    loaded += 4*123;
                                }
                            }
                        }

                        if (EMMCBase->emmc_Verbose)
                        {
                            bug("[brcm-sdhc:%ld] Loaded %ld bytes into buffer at %08lx\n", unit->su_UnitNum, loaded, (ULONG)buffer);
                        }

                        struct SmartBuffer bu;
                        
                        bu.buffer = buffer;
                        bu.pos = 0;
                        bu.size = buffer_size;
                        
                        ULONG segList = LoadSegBlock(&bu);

                        struct FileSysEntry *fse = AllocMem(sizeof(struct FileSysEntry), MEMF_CLEAR);

                        if (fse) {
                            struct FileSysResource *fsr = OpenResource(FSRNAME);
                            ULONG *dstPatch = &fse->fse_Type;
                            ULONG *srcPatch = &buff->fshd.fhb_Type;
                            ULONG patchFlags = buff->fshd.fhb_PatchFlags;
                            while (patchFlags) {
                                *dstPatch++ = *srcPatch++;
                                patchFlags >>= 1;
                            }
                            fse->fse_DosType = buff->fshd.fhb_DosType;
                            fse->fse_Version = buff->fshd.fhb_Version;
                            fse->fse_PatchFlags = buff->fshd.fhb_PatchFlags;
                            fse->fse_Node.ln_Name = NULL;
                            fse->fse_SegList = segList;

                            Forbid();
                            AddHead(&fsr->fsr_FileSysEntries, &fse->fse_Node);
                            Permit();
                        }

                        FreeMem(buffer, buffer_size);
                        FreeMem(ls_buff, 512);
                    }
                }
            }
        }
    }

    FreeMem(buff, 512);
}

struct FileSysEntry * findFSE(struct EMMCUnit *unit, ULONG dosType)
{
    struct EMMCBase *EMMCBase = unit->su_Base;
    struct ExecBase *SysBase = EMMCBase->emmc_SysBase;
    struct FileSysResource *fsr = OpenResource(FSRNAME);
    struct FileSysEntry *ret = NULL;

    if (fsr)
    {
        struct FileSysEntry *n;
        
        Forbid();
        for (   n = (struct FileSysEntry *)fsr->fsr_FileSysEntries.lh_Head; 
                n != NULL; 
                n = (struct FileSysEntry *)n->fse_Node.ln_Succ)
        {
            if (n->fse_DosType == dosType)
            {
                ret = n;
                break;
            }
        }
        Permit();
    }

    return ret;
}

static void MountPartitions(struct EMMCUnit *unit)
{
    struct EMMCBase *EMMCBase = unit->su_Base;
    struct ExecBase *SysBase = EMMCBase->emmc_SysBase;
    struct ExpansionBase *ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 36);
    BOOL rdsk = FALSE;

    union
    {
        struct RigidDiskBlock   rdsk;
        struct PartitionBlock   part;
        ULONG                   ublock[512/4];
        UBYTE                   bblock[512];
    } buff;

    if (EMMCBase->emmc_Verbose)
    {
        bug("[brcm-sdhc:%ld] MountPartitions\n", unit->su_UnitNum);
    }

    /* RigidDiskBlock has to be found within first 16 sectors. Check them now */
    for (int i=0; i < RDB_LOCATION_LIMIT; i++)
    {
        emmc_read((APTR)buff.bblock, 512, unit->su_StartBlock + i, EMMCBase);

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
            emmc_read((APTR)buff.bblock, 512, unit->su_StartBlock + next_part, EMMCBase);

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
                        struct ConfigDev *cdev = EMMCBase->emmc_ConfigDev;
                        struct FileSysEntry *fse = findFSE(unit, buff.part.pb_Environment[DE_DOSTYPE]);

                        if (fse == NULL) {
                            LoadFilesystem(unit, buff.part.pb_Environment[DE_DOSTYPE]);
                            fse = findFSE(unit, buff.part.pb_Environment[DE_DOSTYPE]);
                        }
                        
                        if (EMMCBase->emmc_Verbose)
                        {
                            bug("[brcm-sdhc:%ld] FileSysEntry seems to be %08lx\n", unit->su_UnitNum, (ULONG)fse);
                        }

                        for (int i=0; i < buff.part.pb_DriveName[0]; i++) {
                            name[i] = buff.part.pb_DriveName[1+i];
                        }
                        name[buff.part.pb_DriveName[0]] = 0;

                        paramPkt[0] = (ULONG)name;
                        paramPkt[1] = (ULONG)EMMCBase->emmc_Device.dd_Library.lib_Node.ln_Name;
                        paramPkt[2] = unit->su_UnitNum;
                        paramPkt[3] = 0;
                        for (int i=0; i < 20; i++) {
                            paramPkt[4+i] = buff.part.pb_Environment[i];
                        }

                        if ((buff.part.pb_Flags & PBFF_BOOTABLE) == 0) {
                            paramPkt[DE_BOOTPRI + 4] = -128;
                            cdev = NULL;
                        }

                        struct DeviceNode *devNode = MakeDosNode(paramPkt);

                        if (fse) {
                            // Process PatchFlags
                            ULONG *dstPatch = &devNode->dn_Type;
                            ULONG *srcPatch = &fse->fse_Type;
                            ULONG patchFlags = fse->fse_PatchFlags;
                            while (patchFlags) {
                                if (patchFlags & 1) {
                                    *dstPatch = *srcPatch;
                                }
                                patchFlags >>= 1;
                                srcPatch++;
                                dstPatch++;
                            }
                        }

                        AddBootNode(paramPkt[DE_BOOTPRI + 4], 0, devNode, cdev);
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

    struct EMMCUnit *unit;
    struct EMMCBase *EMMCBase;
    struct Task *task;
      
    task = FindTask(NULL);
    unit = task->tc_UserData;
    EMMCBase = unit->su_Base;

    NewList(&unit->su_Unit.unit_MsgPort.mp_MsgList);
    unit->su_Unit.unit_MsgPort.mp_SigTask = task;
    unit->su_Unit.unit_MsgPort.mp_SigBit = AllocSignal(-1);
    unit->su_Unit.unit_MsgPort.mp_Flags = PA_SIGNAL;
    unit->su_Unit.unit_MsgPort.mp_Node.ln_Type = NT_MSGPORT;

    ObtainSemaphore(&EMMCBase->emmc_Lock);

    MountPartitions(unit);

    ReleaseSemaphore(&EMMCBase->emmc_Lock);

    Signal(unit->su_Caller, SIGBREAKF_CTRL_C);

    while(1) {
        struct IORequest *io;
        WaitPort(&unit->su_Unit.unit_MsgPort);
        while(io = (struct IORequest *)GetMsg(&unit->su_Unit.unit_MsgPort))
        {
            int_do_io(io, EMMCBase);
            io->io_Message.mn_Node.ln_Type = NT_MESSAGE;
            ReplyMsg(&io->io_Message);
        }
    }       
}