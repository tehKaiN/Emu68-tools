#ifndef _EMU68_VC4_H
#define _EMU68_VC4_H

#include <exec/types.h>
#include <exec/libraries.h>

#include <dos/dos.h>
#include <intuition/intuitionbase.h>
#include <libraries/expansionbase.h>

#define STR(s) #s
#define XSTR(s) STR(s)

#define VC4CARD_VERSION  0
#define VC4CARD_REVISION 1
#define VC4CARD_PRIORITY 0

struct VC4Base {
    struct Library          vc4_LibNode;
    BPTR                    vc4_SegList;
    struct ExecBase *       vc4_SysBase;
    struct ExpansionBase *  vc4_ExpansionBase;
    struct DOSBase *        vc4_DOSBase;
    struct IntuitionBase *  vc4_IntuitionBase;
    APTR                    vc4_DeviceTreeBase;
    APTR                    vc4_MailBox;
    APTR                    vc4_RequestBase;
    APTR                    vc4_Request;
    APTR                    vc4_MemBase;
    uint32_t                vc4_MemSize;
};

struct Size {
    UWORD width;
    UWORD height;
};

#endif /* _EMU68_VC4_H */
