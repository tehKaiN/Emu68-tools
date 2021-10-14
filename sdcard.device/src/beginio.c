/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <dos/dosextens.h>

#include <devices/newstyle.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

#include <proto/exec.h>

#include "sdcard.h"


static void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

static const ULONG quick = 
    (1 << TD_CHANGENUM)     |
    (1 << TD_GETDRIVETYPE)  |
    (1 << TD_GETGEOMETRY)   |
    (1 << TD_PROTSTATUS);

const UWORD NSDSupported[] = {
        CMD_READ,
        CMD_WRITE,
        CMD_UPDATE,
        CMD_CLEAR,
        TD_CHANGENUM,
        TD_FORMAT,
        TD_GETDRIVETYPE,
        TD_GETGEOMETRY,
        TD_PROTSTATUS,
        TD_READ64,
        TD_WRITE64,
        NSCMD_DEVICEQUERY,
        NSCMD_TD_READ64,
        NSCMD_TD_WRITE64,
        HD_SCSICMD,
        0
    };

static CONST_STRPTR const ManuID[] = {
    [0x01] = "Panasoni",
    [0x02] = "Toshiba ",
    [0x03] = "SanDisk ",
    [0x1b] = "Samsung ",
    [0x1d] = "AData   ",
    [0x27] = "Phison  ",
    [0x28] = "Lexar   ",
    [0x31] = "SiliconP",
    [0x74] = "Transcnd",
    [0x76] = "Patriot ",
    [0x82] = "Sony    ",
};

#define MANU_ID_LEN (sizeof(ManuID) / sizeof(ManuID[0]))

void int_handle_scsi(struct IOStdReq *io, struct SDCardBase * SDCardBase)
{
    struct SCSICmd *cmd = io->io_Data;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;
    struct ExecBase *SysBase = unit->su_Base->sd_SysBase;
    ULONG blocks;
    UBYTE *data = (UBYTE *)cmd->scsi_Data;
    UBYTE *tmp1;
    UBYTE tmp2;
#if 0
    {
        ULONG args[] = {
            (ULONG)unit->su_UnitNum,
            (ULONG)cmd->scsi_Command[0],
            (ULONG)cmd->scsi_Command[1],
            (ULONG)cmd->scsi_Command[2],
            (ULONG)cmd->scsi_Command[3],
            (ULONG)cmd->scsi_Command[4],
            (ULONG)cmd->scsi_Command[5],
        };

        RawDoFmt("[brcm-sdhc:%ld] HD_SCSICMD cmd=%02lx %02lx %02lx %02lx %02lx %02lx\n", args, (APTR)putch, NULL);
    }
#endif

    switch (cmd->scsi_Command[0])
    {
        case 0x00:
            io->io_Error = 0;
            break;
        
        case 0x12: // INQUIRY
            for (int i = 0; i < cmd->scsi_Length; i++) {
                UBYTE val;

                switch (i) {
                case 0: // direct-access device
                        val = 0;
                        break;
                case 1: // non-removable medium
                        val = 0;
                        break;
                case 2: // VERSION = 0
                        val = 0;
                        break;
                case 3: // NORMACA=0, HISUP = 0, RESPONSE_DATA_FORMAT = 2
                        val = (0 << 5) | (0 << 4) | 2;
                        break;
                case 4: // ADDITIONAL_LENGTH = 44 - 4
                        val = 44 - 4;
                        break;
                default:
                        if (i == 8)
                        {
                            tmp2 = (SDCardBase->sd_CID[0] >> 16) & 0xff;
                            UBYTE **id = (UBYTE**)((ULONG)ManuID + (ULONG)SDCardBase->sd_ROMBase);
                            if (id[tmp2] != NULL) {
                                tmp1 = (STRPTR)id[tmp2] + (ULONG)SDCardBase->sd_ROMBase;
                            }
                            else {
                                tmp1 = "????    ";
                            }
                        }
                        else if (i == 16) {
                            tmp1 = (UBYTE *)&SDCardBase->sd_CID[0];
                            tmp1 += 2;
                        }
                        else if (i == 36) {
                            val = (SDCardBase->sd_CID[3] >> 16) | (SDCardBase->sd_CID[2] << 16);
                        }
                        if (i >= 8 && i < 16)
                            val = tmp1[i - 8];
                        else if (i >= 16 && i < 23)
                            val = tmp1[i - 16];
                        else if (i >= 23 && i < 32) {
                            if (unit->su_UnitNum == 0)
                                val = ' ';
                            else {
                                if (i == 23)
                                    val = '/';
                                else if (i == 24)
                                    val = '0' + unit->su_UnitNum;
                                else
                                    val = ' ';
                            }
                        }
                        else if (i >= 32 && i < 36) {
                            switch (i) {
                                case 32:
                                    val = 'v';
                                    break;
                                case 33:
                                    val = '0' + ((SDCardBase->sd_CID[2] >> 20) & 0xf);
                                    break;
                                case 34:
                                    val = '.';
                                    break;
                                case 35:
                                    val = '0' + ((SDCardBase->sd_CID[2] >> 16) & 0xf);
                                    break;
                                default:
                                    val = ' ';
                                    break;
                            }
                        }
                        else if (i >= 36 && i < 44) {
                            if ((i & 1) == 0)
                                val >>= 4;
                            val = "0123456789ABCDEF"[val & 0xf];
                        } else
                            val = 0;
                        break;
                }

                data[i] = val;
            }

            cmd->scsi_Actual = cmd->scsi_Length;
            io->io_Error = 0;
            break;

        case 0x25: // READ_CAPACITY (10)
            if (cmd->scsi_CmdLength < 10)
            {
                io->io_Error = HFERR_BadStatus;
                break;
            }

            if (cmd->scsi_Command[2] != 0 || cmd->scsi_Command[3] != 0 || cmd->scsi_Command[4] != 0 || cmd->scsi_Command[5] != 0 || (cmd->scsi_Command[8] & 1))
            {
                io->io_Error = HFERR_BadStatus;
                break;
            }

            if (cmd->scsi_Length < 8)
            {
                io->io_Error = IOERR_BADLENGTH;
                break;
            }

            data[0] = ((unit->su_BlockCount - 1) >> 24) & 0xff;
            data[1] = ((unit->su_BlockCount - 1) >> 16) & 0xff;
            data[2] = ((unit->su_BlockCount - 1) >> 8) & 0xff;
            data[3] = (unit->su_BlockCount - 1) & 0xff;
            data[4] = (unit->su_Base->sd_BlockSize >> 24) & 0xff;
            data[5] = (unit->su_Base->sd_BlockSize >> 16) & 0xff;
            data[6] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
            data[7] = (unit->su_Base->sd_BlockSize) & 0xff;
            
            cmd->scsi_Actual = 8;
            io->io_Error = 0;
            
            break;

        case 0x1a: // MODE SENSE (6)
            for (int i=0; i < cmd->scsi_Length; i++)
                data[i] = 0;
            data[0] = 3 + 8 + 0x16;
            data[1] = 0; // MEDIUM TYPE
            data[2] = 0;
            data[3] = 8;
            if (unit->su_BlockCount > (1 << 24))
                blocks = 0xffffff;
            else
                blocks = unit->su_BlockCount;
            data[4] = (blocks >> 16) & 0xff;
            data[5] = (blocks >>  8) & 0xff;
            data[6] = (blocks >>  0) & 0xff;
            data[7] = 0;
            data[8] = 0;
            data[9] = 0;
            data[10] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
            data[11] = (unit->su_Base->sd_BlockSize) & 0xff;
            switch (((UWORD)cmd->scsi_Command[2] << 8) | cmd->scsi_Command[3]) {
                case 0x0300: // Format Device Mode
                    data[12] = 0x03;  // PAGE CODE
                    data[13] = 0x16;  // PAGE_LENGTH
                    data[14] = 0;     // TRACKS PER ZONE 15..8
                    data[15] = 16;    // TRACKS PER ZONE 7..0
                    
                    data[22] = 0;     // SECTORS PER TRACK 15..8
                    data[23] = 64;    // SECTORS PER TRACK 7..0
                    data[24] = (unit->su_Base->sd_BlockSize >> 8) & 0xff;
                    data[25] = (unit->su_Base->sd_BlockSize) & 0xff;

                    data[32] = (1 << 6) | (1 << 5);

                    cmd->scsi_Actual = data[0] + 1;
                    io->io_Error = 0;
                    break;
                case 0x0400: // Rigid Drive Geometry
                    data[12] = 0x04;  // PAGE CODE
                    data[13] = 0x16;  // PAGE LENGTH
                    data[14] = (unit->su_BlockCount / (64*16)) >> 16;
                    data[15] = (unit->su_BlockCount / (64*16)) >> 8;
                    data[16] = (unit->su_BlockCount / (64*16));
                    data[17] = 16;

                    cmd->scsi_Actual = cmd->scsi_Data[0] + 1;
                    io->io_Error = 0;
                    break;
                default:
                    io->io_Error = HFERR_BadStatus;
                    break;
            }
            break;

        default:
            io->io_Error = IOERR_NOCMD;
            break;
    }
}

void int_do_io(struct IORequest *io , struct SDCardBase * SDCardBase)
{
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;
    struct IOStdReq *iostd = (struct IOStdReq *)io;

    unsigned long long off64;
    ULONG offset;
    ULONG actual;
    ULONG startblock = unit->su_StartBlock;
    ULONG endblock = unit->su_StartBlock + unit->su_BlockCount;

    /* If the IO was meant to be aborted, perform no action anymore */
    if (io->io_Error == IOERR_ABORTED) {
        return;
    }

    /* Clear any error */
    io->io_Error = 0;

    ObtainSemaphore(&SDCardBase->sd_Lock);
    SDCardBase->sd_SetLED(1, SDCardBase);

    switch (io->io_Command)
    {
        case NSCMD_DEVICEQUERY:
            if (iostd->io_Length  < sizeof(struct NSDeviceQueryResult)) {
                io->io_Error = IOERR_BADLENGTH;
            }
            else
            {
                struct NSDeviceQueryResult *nsd = (struct NSDeviceQueryResult *)iostd->io_Data;

                nsd->nsdqr_DevQueryFormat = 0;
                nsd->nsdqr_SizeAvailable = sizeof(struct NSDeviceQueryResult);
                nsd->nsdqr_SupportedCommands = SDCardBase->sd_NSDSupported;
                nsd->nsdqr_DeviceType = NSDEVTYPE_TRACKDISK;
                nsd->nsdqr_DeviceSubType = 0;

                iostd->io_Actual = sizeof(struct NSDeviceQueryResult);
            }
            break;

        case TD_PROTSTATUS:
            iostd->io_Actual = 0;
            break;

        case TD_CHANGENUM:
            iostd->io_Actual = 1;
            break;

        case TD_GETDRIVETYPE:
            iostd->io_Actual = DG_DIRECT_ACCESS;
            break;
        
        case TD_MOTOR:
            break;

        case TD_GETGEOMETRY:
            if (iostd->io_Length < sizeof(struct DriveGeometry)) {
                io->io_Error = IOERR_BADLENGTH;
            }
            else {
                struct DriveGeometry *g = (struct DriveGeometry *)iostd->io_Data;

                g->dg_SectorSize = SDCardBase->sd_BlockSize;
                g->dg_TotalSectors = unit->su_BlockCount;
                g->dg_TrackSectors = 64;
                g->dg_Heads = 16;
                g->dg_CylSectors = g->dg_TrackSectors * g->dg_Heads;
                g->dg_Cylinders = g->dg_TotalSectors / g->dg_CylSectors;
                g->dg_BufMemType = MEMF_PUBLIC;
                g->dg_DeviceType = DG_DIRECT_ACCESS;
                g->dg_Flags = 0; /* Non-removable */

                iostd->io_Actual = sizeof(struct DriveGeometry);
            }

        case CMD_CLEAR:
            iostd->io_Actual = 0;
            break;

        case CMD_UPDATE:
            iostd->io_Actual = 0;
            break;

        case CMD_READ:
            offset = iostd->io_Offset / SDCardBase->sd_BlockSize;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) >= unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        case NSCMD_TD_READ64: /* Fallthrough */
        case TD_READ64:
            off64 = iostd->io_Offset;
            off64 |= ((unsigned long long)iostd->io_Actual) << 32;
            offset = off64 >> 9;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) >= unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Read(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        
        case CMD_WRITE:
            offset = iostd->io_Offset / SDCardBase->sd_BlockSize;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) >= unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Write(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        case NSCMD_TD_WRITE64: /* Fallthrough */
        case TD_WRITE64:
            off64 = iostd->io_Offset;
            off64 |= ((unsigned long long)iostd->io_Actual) << 32;
            offset = off64 >> 9;

            if (offset >= unit->su_BlockCount || (offset + iostd->io_Length / SDCardBase->sd_BlockSize) >= unit->su_BlockCount) {
                io->io_Error = IOERR_BADADDRESS;
                iostd->io_Actual = 0;
            }
            else
            {
                actual = SDCardBase->sd_Write(iostd->io_Data, iostd->io_Length, startblock + offset, SDCardBase);
                if (actual < 0)
                {
                    io->io_Error = TDERR_NotSpecified;
                }
                else
                {
                    iostd->io_Actual = actual;
                }
            }         
            break;
        
        case HD_SCSICMD:
            int_handle_scsi(iostd, SDCardBase);
            break;

        default:
            io->io_Error = IOERR_NOCMD;
            break;
    }

    SDCardBase->sd_SetLED(0, SDCardBase);
    ReleaseSemaphore(&SDCardBase->sd_Lock);
}

void SD_BeginIO(struct IORequest *io asm("a1"))
{
    struct SDCardBase *SDCardBase = (struct SDCardBase *)io->io_Device;
    struct ExecBase *SysBase = SDCardBase->sd_SysBase;
    struct SDCardUnit *unit = (struct SDCardUnit *)io->io_Unit;

    io->io_Error = 0;
    io->io_Message.mn_Node.ln_Type = NT_MESSAGE;

#if 0
    {
        ULONG args[] = {
            (ULONG)unit->su_UnitNum,
            (ULONG)io->io_Unit,
            (ULONG)io->io_Command,
        };

        RawDoFmt("[brcm-sdhc:%ld] BeginIO Unit=%08lx, cmd=%ld\n", args, (APTR)putch, NULL);
    }
#endif

    Disable();

    /* Check if command is quick. If this is the case, process immediately */
    if (io->io_Command == NSCMD_DEVICEQUERY || 
        ((io->io_Command < 32) && ((1 << io->io_Command) & quick)))
    {
        Enable();
        switch(io->io_Command)
        {
            case NSCMD_DEVICEQUERY: /* Fallthrough */
            case TD_CHANGENUM:      /* Fallthrough */
            case TD_GETDRIVETYPE:   /* Fallthrough */
            case TD_GETNUMTRACKS:   /* Fallthrough */
            case TD_GETGEOMETRY:    /* Fallthrough */
            case TD_PROTSTATUS:
                SDCardBase->sd_DoIO(io, SDCardBase);
                break;
            default:
                io->io_Error = IOERR_NOCMD;
                break;
        }

        /* 
            The IOF_QUICK flag was cleared. It means the caller was going to wait for command 
            completion. Therefore, reply the command now.
        */
        if (!(io->io_Flags & IOF_QUICK))
            ReplyMsg(&io->io_Message);
    }
    else
    {
        /* 
            If command is slow, clear IOF_QUICK flag and put it to some internal queue. When
            done with slow command, use ReplyMsg to notify exec that the command completed.
            In such case *do not* reply the command now.
            When modifying IoRequest, do it in disabled state.
            In case of a quick command, handle it now.
        */
        io->io_Flags &= ~IOF_QUICK;
        Enable();

        /* Push the command to a queue, process it maybe in another task/process, reply when
        completed */
        PutMsg(&unit->su_Unit.unit_MsgPort, &io->io_Message);
    }
}
