#define __NOLIBBASE__

#include <exec/execbase.h>
#include <exec/ports.h>
#include <devices/timer.h>
#include <utility/date.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/i2c.h>
#include <proto/utility.h>
#include <common/debug.h>

#include <stdlib.h>

int main(int);

/* Startup code including workbench message support */
int _start()
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    struct Process *p = NULL;
    struct WBStartup *wbmsg = NULL;
    int ret = 0;

    p = (struct Process *)SysBase->ThisTask;

    if (p->pr_CLI == 0)
    {
        WaitPort(&p->pr_MsgPort);
        wbmsg = (struct WBStartup *)GetMsg(&p->pr_MsgPort);
    }

    ret = main(wbmsg ? 1 : 0);

    if (wbmsg)
    {
        Forbid();
        ReplyMsg((struct Message *)wbmsg);
    }

    return ret;
}

static const char version[] __attribute__((used)) = "$VER: " VERSION_STRING;

struct ExecBase * SysBase = NULL;
struct DosLibrary * DOSBase = NULL;
struct Library * I2C_Base = NULL;
struct Library * UtilityBase = NULL;

#define RDA_TEMPLATE "LOAD/S,SAVE/S"

enum {
    OPT_LOAD,
    OPT_SAVE,

    OPT_COUNT
};

LONG result[OPT_COUNT];

int OpenLibs()
{
    DOSBase = (struct DosLibrary *)OpenLibrary("dos.library", 37);
    if (DOSBase == NULL)
        return 0;
    
    I2C_Base = OpenLibrary("i2c.library", 39);
    if (I2C_Base == NULL)
        return 0;

    UtilityBase = OpenLibrary("utility.library", 0);
    if (UtilityBase == NULL)
        return 0;

    return 1;
}

void CloseLibs()
{
    if (UtilityBase != NULL) CloseLibrary(UtilityBase);
    if (I2C_Base != NULL) CloseLibrary(I2C_Base);
    if (DOSBase != NULL) CloseLibrary((struct Library *)DOSBase);
}

UBYTE bcd2dec(UBYTE bcd)
{
    UBYTE hi = (bcd & 0xf0) >> 4;
    UBYTE lo = bcd & 0x0f;

    if (hi > 9 || lo > 9)
        return 0xff;
    
    else return hi * 10 + lo;
}

UBYTE dec2bcd(UBYTE dec)
{
    if (dec > 99)
        return 0;
    
    UBYTE lo = dec % 10;
    UBYTE hi = dec / 10;

    return lo | (hi << 4);
}

int main(int wb)
{
    struct RDArgs *args;
    SysBase = *(struct ExecBase **)4;
    struct timerequest *tr;

    if (!OpenLibs())
    {
        CloseLibs();
        return -1;
    }
    
    tr = (struct timerequest *)CreateIORequest(CreateMsgPort(), sizeof(*tr));
    if (tr == NULL)
    {
        CloseLibrary(I2C_Base);
        CloseLibrary((struct Library *)DOSBase);
        return -1;
    }

    OpenDevice("timer.device", UNIT_VBLANK, (struct IORequest *)tr, 0);

    args = ReadArgs(RDA_TEMPLATE, result, NULL);

    if (args)
    {
        BOOL load = result[OPT_LOAD];
        BOOL save = result[OPT_SAVE];

        if (save)
        {
            struct ClockData datetime;
            UBYTE buf[8];
            ULONG status = 0;

            // Get system time
            tr->tr_node.io_Command = TR_GETSYSTIME;
            tr->tr_node.io_Flags = IOF_QUICK;

            DoIO((struct IORequest *)tr);

            // Convert system time to normal
            Amiga2Date(tr->tr_time.tv_sec, &datetime);
            
            buf[0] = 0; // start register 0
            buf[1] = dec2bcd(datetime.sec);
            buf[2] = dec2bcd(datetime.min);
            buf[3] = dec2bcd(datetime.hour); // always 24h mode!
            buf[4] = datetime.wday;
            buf[5] = dec2bcd(datetime.mday);
            buf[6] = dec2bcd(datetime.month);
            // Adjust year
            datetime.year -= 1900;
            // 20th century? Set bit in month, adjust year again
            if (datetime.year > 99)
            {
                datetime.year -= 100;
                buf[6] |= 0x80;
            }
            buf[7] = dec2bcd(datetime.year);

            status = SendI2C(0xd0, 8, buf);
            bug("SendI2C returned status %06x\n", status);
        }
        else
        {
            struct ClockData datetime;
            UBYTE buf[7];
            ULONG status = 0;

            // Reset address register of RTC module
            buf[0] = 0;
            status = SendI2C(0xd0, 1, buf);
            bug("SendI2C returned status %06x\n", status);

            // Get time and date
            status = ReceiveI2C(0xd0, 7, buf);
            bug("ReceiveI2C returned status %06x\n", status);

            // Decode minutes
            datetime.sec = bcd2dec(buf[0]);
            // Decode seconds
            datetime.min = bcd2dec(buf[1]);
            if (buf[2] & 0x40)
            {
                // Decode AM/PM hours
                datetime.hour = bcd2dec(buf[2] & 0x1f);
                
                if (buf[2] & 0x20)
                {
                    datetime.hour += 12;
                }
            }
            else
            {
                // Decode hours
                datetime.hour = bcd2dec(buf[2]);
            }

            // Decode weekday
            datetime.wday = buf[3];
            // Decode month, year and century
            datetime.mday = bcd2dec(buf[4]);
            datetime.month = bcd2dec(buf[5] & 0x1f);
            datetime.year = bcd2dec(buf[6]) + 1900;
            if (buf[5] & 0x80)
                datetime.year += 100;

            Printf("RTC Date: %ld-%ld-%ld %02ld:%02ld:%02ld\n", (int)datetime.year, (int)datetime.month, (int)datetime.mday, (int)datetime.hour, (int)datetime.min, (int)datetime.sec);
            bug("RTC Date: %d-%d-%d %02d:%02d:%02d\n", (int)datetime.year, (int)datetime.month, (int)datetime.mday, (int)datetime.hour, (int)datetime.min, (int)datetime.sec);

            if (load && CheckDate(&datetime))
            {
                tr->tr_time.tv_sec = Date2Amiga(&datetime);
                tr->tr_time.tv_micro = 0;
                tr->tr_node.io_Command = TR_SETSYSTIME;
                tr->tr_node.io_Flags = IOF_QUICK;

                DoIO((struct IORequest *)tr);
            }
        }

        FreeArgs(args);
    }

    CloseDevice((struct IORequest *)tr);
    DeleteMsgPort(tr->tr_node.io_Message.mn_ReplyPort);
    DeleteIORequest((struct IORequest *)tr);
    
    CloseLibs();

    return 0;
}
