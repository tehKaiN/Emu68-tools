#ifndef _WIFIPI_H
#define _WIFIPI_H

#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/devices.h>

#include <dos/dos.h>
#include <intuition/intuitionbase.h>
#include <libraries/expansionbase.h>

#define STR(s) #s
#define XSTR(s) STR(s)

#define WIFIPI_VERSION  0
#define WIFIPI_REVISION 1
#define WIFIPI_PRIORITY 0

struct WiFiBase
{
    struct Device       w_Device;
    BPTR                w_SegList;
    struct ExecBase *   w_SysBase;
    APTR                w_DeviceTreeBase;
    APTR                w_SDIO;
    APTR                w_MailBox;
};

static inline __attribute__((always_inline)) void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    (void)ignore;
    *(UBYTE*)0xdeadbeef = data;
}

struct WiFiBase * WiFi_Init(struct WiFiBase *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"));

#define bug(string, ...) \
    do { ULONG args[] = {0, __VA_ARGS__}; RawDoFmt(string, &args[1], (APTR)putch, NULL); } while(0)

#endif
