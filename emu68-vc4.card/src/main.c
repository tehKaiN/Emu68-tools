#include <exec/types.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/execbase.h>
#include <exec/resident.h>
#include <exec/initializers.h>
#include <exec/execbase.h>
#include <clib/debug_protos.h>
#include <devices/inputevent.h>

#include <proto/exec.h>
#include <proto/expansion.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/input.h>
#include <proto/devicetree.h>

#include <stdint.h>

#include "boardinfo.h"
#include "emu68-vc4.h"
#include "mbox.h"

int __attribute__((no_reorder)) _start()
{
        return -1;
}

extern const char deviceEnd;
extern const char deviceName[];
extern const char deviceIdString[];
extern const uint32_t InitTable[];

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
    (APTR)&deviceEnd,
    RTF_AUTOINIT,
    VC4CARD_VERSION,
    NT_LIBRARY,
    VC4CARD_PRIORITY,
    (char *)((intptr_t)&deviceName),
    (char *)((intptr_t)&deviceIdString),
    (APTR)InitTable,
};

const char deviceName[] = "emu68-vc4.card";
const char deviceIdString[] = VERSION_STRING;

/*
    Some properties, like e.g. #size-cells, are not always available in a key, but in that case the properties
    should be searched for in the parent. The process repeats recursively until either root key is found
    or the property is found, whichever occurs first
*/
CONST_APTR GetPropValueRecursive(APTR key, CONST_STRPTR property, APTR DeviceTreeBase)
{
    do {
        /* Find the property first */
        APTR property = DT_FindProperty(key, property);

        if (property)
        {
            /* If property is found, get its value and exit */
            return DT_GetPropValue(property);
        }
        
        /* Property was not found, go to the parent and repeat */
        key = DT_GetParent(key);
    } while (key);

    return NULL;
}

static int FindCard(struct BoardInfo* bi asm("a0"), struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    APTR DeviceTreeBase = NULL;
    APTR key;

    // Cancel loading the driver if left or right shift is being held down.
    struct IORequest io;

    if (OpenDevice((STRPTR)"input.device", 0, &io, 0) == 0)
    {
        struct Library *InputBase = (struct Library *)io.io_Device;
        UWORD qual = PeekQualifier();
        CloseDevice(&io);

        if (qual & (IEQUALIFIER_LSHIFT | IEQUALIFIER_RSHIFT))
            return 0;
    }

    /* Open device tree resource */
    DeviceTreeBase = (struct Library *)OpenResource((STRPTR)"devicetree.resource");
    if (DeviceTreeBase == 0) {
        // If devicetree.resource can't be opened, this probably isn't Emu68.
        return 0;
    }
    VC4Base->vc4_DeviceTreeBase = DeviceTreeBase;

    /* Open DOS, Expansion and Intuition, but I don't know yet why... */
    VC4Base->vc4_ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    
    if (VC4Base->vc4_ExpansionBase == NULL) {
        return 0;
    }

    VC4Base->vc4_IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library", 0);
    
    if (VC4Base->vc4_IntuitionBase == NULL) {
        CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        return 0;
    }

    VC4Base->vc4_DOSBase = (struct DOSBase *)OpenLibrary("dos.library", 0);

    if (VC4Base->vc4_DOSBase == NULL) {
        CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);
        CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        return 0;
    }

    /* Alloc 128-byte aligned memory for mailbox requests */
    VC4Base->vc4_RequestBase = AllocMem(MBOX_SIZE, MEMF_FAST);

    if (VC4Base->vc4_RequestBase == NULL) {
        CloseLibrary((struct Library *)VC4Base->vc4_DOSBase);
        CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);
        CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        return 0;
    }

    VC4Base->vc4_Request = (ULONG *)(((intptr_t)VC4Base->vc4_RequestBase + 127) & ~127);

    /* Get VC4 physical address of mailbox interface. Subsequently it will be translated to m68k physical address */
    key = DT_OpenKey("/aliases");
    if (key)
    {
        CONST_STRPTR mbox_alias = DT_GetPropValue(DT_FindProperty(key, "mailbox"));

        DT_CloseKey(key);
        
        if (mbox_alias != NULL)
        {
            key = DT_OpenKey(mbox_alias);

            if (key)
            {
                int size_cells = 1;
                int address_cells = 1;

                const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                if (siz != NULL)
                    size_cells = *siz;
                
                if (addr != NULL)
                    address_cells = *addr;

                const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));

                VC4Base->vc4_MailBox = (APTR)reg[address_cells - 1];

                DT_CloseKey(key);
            }
        }
    }

    /* Open /soc key and learn about VC4 to CPU mapping. Use it to adjust the addresses obtained above */
    key = DT_OpenKey("/soc");
    if (key)
    {
        int size_cells = 1;
        int address_cells = 1;

        const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
        const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

        if (siz != NULL)
            size_cells = *siz;
        
        if (addr != NULL)
            address_cells = *addr;

        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "ranges"));

        ULONG phys_vc4 = reg[address_cells - 1];
        ULONG phys_cpu = reg[2 * address_cells - 1];

        VC4Base->vc4_MailBox = (APTR)((ULONG)VC4Base->vc4_MailBox - phys_vc4 + phys_cpu);

        DT_CloseKey(key);
    }

    /* Find out base address of framebuffer and video memory size */
    get_vc_memory(&VC4Base->vc4_MemBase, &VC4Base->vc4_MemSize, VC4Base);

    /* Set basic data in BoardInfo structure */
    bi->MemoryBase = VC4Base->vc4_MemBase;
    bi->MemorySize = VC4Base->vc4_MemSize;
    bi->RegisterBase = NULL;

    return 1;
}

static int InitCard(struct BoardInfo* bi asm("a0"), struct VC4Base *VC4Base asm("a6"))
{
    bi->CardBase = (struct CardBase *)VC4Base;
    bi->ExecBase = VC4Base->vc4_SysBase;
    bi->BoardName = "Emu68 VC4";
    bi->BoardType = 14;
    bi->PaletteChipType = PCT_S3ViRGE;
    bi->GraphicsControllerType = GCT_S3ViRGE;

    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_FLICKERFIXER;// | BIF_HARDWARESPRITE;// | BIF_BLITTER;
    bi->RGBFormats = RGBFF_HICOLOR | RGBFF_TRUEALPHA | RGBFF_CLUT;
    bi->SoftSpriteFlags = 0;
    bi->BitsPerCannon = 8;

    for(int i = 0; i < MAXMODES; i++) {
        bi->MaxHorValue[i] = 8192;
        bi->MaxVerValue[i] = 8192;
        bi->MaxHorResolution[i] = 8192;
        bi->MaxVerResolution[i] = 8192;
        bi->PixelClockCount[i] = 1;
    }

    bi->MemoryClock = CLOCK_HZ;

    // Basic P96 functions needed for "dumb frame buffer" operation
    bi->SetSwitch = (void *)SetSwitch;
    bi->SetColorArray = (void *)SetColorArray;
    bi->SetDAC = (void *)SetDAC;
    bi->SetGC = (void *)SetGC;
    bi->SetPanning = (void *)SetPanning;
    bi->CalculateBytesPerRow = (void *)CalculateBytesPerRow;
    bi->CalculateMemory = (void *)CalculateMemory;
    bi->GetCompatibleFormats = (void *)GetCompatibleFormats;
    bi->SetDisplay = (void *)SetDisplay;

    bi->ResolvePixelClock = (void *)ResolvePixelClock;
    bi->GetPixelClock = (void *)GetPixelClock;
    bi->SetClock = (void *)SetClock;

    bi->SetMemoryMode = (void *)SetMemoryMode;
    bi->SetWriteMask = (void *)SetWriteMask;
    bi->SetClearMask = (void *)SetClearMask;
    bi->SetReadPlane = (void *)SetReadPlane;

    bi->WaitVerticalSync = (void *)WaitVerticalSync;

    // Additional functions for "blitter" acceleration and vblank handling
    //bi->SetInterrupt = (void *)NULL;

    //bi->WaitBlitter = (void *)NULL;

    //bi->ScrollPlanar = (void *)NULL;
    //bi->UpdatePlanar = (void *)NULL;

    //bi->BlitPlanar2Chunky = (void *)BlitPlanar2Chunky;
    //bi->BlitPlanar2Direct = (void *)BlitPlanar2Direct;

    //bi->FillRect = (void *)FillRect;
    //bi->InvertRect = (void *)InvertRect;
    //bi->BlitRect = (void *)BlitRect;
    //bi->BlitTemplate = (void *)BlitTemplate;
    //bi->BlitPattern = (void *)BlitPattern;
    //bi->DrawLine = (void *)DrawLine;
    //bi->BlitRectNoMaskComplete = (void *)BlitRectNoMaskComplete;
    //bi->EnableSoftSprite = (void *)NULL;

    //bi->AllocCardMemAbs = (void *)NULL;
    //bi->SetSplitPosition = (void *)NULL;
    //bi->ReInitMemory = (void *)NULL;
    //bi->WriteYUVRect = (void *)NULL;
    //bi->GetVSyncState = (void *)GetVSyncState;
    //bi->GetVBeamPos = (void *)NULL;
    //bi->SetDPMSLevel = (void *)NULL;
    //bi->ResetChip = (void *)NULL;
    //bi->GetFeatureAttrs = (void *)NULL;
    //bi->AllocBitMap = (void *)NULL;
    //bi->FreeBitMap = (void *)NULL;
    //bi->GetBitMapAttr = (void *)NULL;

    //bi->SetSprite = (void *)SetSprite;
    //bi->SetSpritePosition = (void *)SetSpritePosition;
    //bi->SetSpriteImage = (void *)SetSpriteImage;
    //bi->SetSpriteColor = (void *)SetSpriteColor;

    //bi->CreateFeature = (void *)NULL;
    //bi->SetFeatureAttrs = (void *)NULL;
    //bi->DeleteFeature = (void *)NULL;

    return 1;
}

static struct VC4Base * OpenLib(ULONG version asm("d0"), struct VC4Base *VC4Base asm("a6"))
{
    VC4Base->vc4_LibNode.LibBase.lib_OpenCnt++;
    VC4Base->vc4_LibNode.LibBase.lib_Flags &= ~LIBF_DELEXP;

    return VC4Base;
}

static ULONG ExpungeLib(struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    BPTR segList = 0;

    if (VC4Base->vc4_LibNode.LibBase.lib_OpenCnt == 0)
    {
        /* Free memory of mailbox request buffer */
        FreeMem(VC4Base->vc4_RequestBase, 4*256);

        /* Remove library from Exec's list */
        Remove(&VC4Base->vc4_LibNode.LibBase.lib_Node);

        /* Close all eventually opened libraries */
        if (VC4Base->vc4_ExpansionBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
        if (VC4Base->vc4_DOSBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_DOSBase);
        if (VC4Base->vc4_IntuitionBase != NULL)
            CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);

        /* Save seglist */
        segList = VC4Base->vc4_SegList;

        /* Remove VC4Base itself - free the memory */
        ULONG size = VC4Base->vc4_LibNode.LibBase.lib_NegSize + VC4Base->vc4_LibNode.LibBase.lib_PosSize;
        FreeMem((APTR)((ULONG)VC4Base - VC4Base->vc4_LibNode.LibBase.lib_NegSize), size);
    }
    else
    {
        /* Library is still in use, set delayed expunge flag */
        VC4Base->vc4_LibNode.LibBase.lib_Flags |= LIBF_DELEXP;
    }

    /* Return 0 or segList */
    return segList;
}

static ULONG CloseLib(struct VC4Base *VC4Base asm("a6"))
{
    if (VC4Base->vc4_LibNode.LibBase.lib_OpenCnt != 0)
        VC4Base->vc4_LibNode.LibBase.lib_OpenCnt--;
    
    if (VC4Base->vc4_LibNode.LibBase.lib_OpenCnt == 0)
    {
        if (VC4Base->vc4_LibNode.LibBase.lib_Flags & LIBF_DELEXP)
            return ExpungeLib(VC4Base);
    }

    return 0;
}


static uint32_t ExtFunc()
{
    return 0;
}

struct VC4Base * vc4_Init(struct VC4Base *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"))
{
    struct VC4Base *VC4Base = base;
    VC4Base->vc4_SegList = seglist;
    VC4Base->vc4_SysBase = SysBase;
    VC4Base->vc4_LibNode.LibBase.lib_Revision = VC4CARD_REVISION;

    return VC4Base;
}

static uint32_t vc4_functions[] = {
    (uint32_t)OpenLib,
    (uint32_t)CloseLib,
    (uint32_t)ExpungeLib,
    (uint32_t)ExtFunc,
    (uint32_t)FindCard,
    (uint32_t)InitCard,
    -1
};

const uint32_t InitTable[4] = {
    sizeof(struct VC4Base), 
    (uint32_t)vc4_functions, 
    0, 
    (uint32_t)vc4_Init
};




UWORD CalculateBytesPerRow (__REGA0(struct BoardInfo *b), __REGD0(UWORD width), __REGD7(RGBFTYPE format)) {
    if (!b)
        return 0;

    UWORD pitch = width;

    switch(format) {
        case RGBFB_CLUT:
            return pitch;
        default:
            return 128;
        case RGBFB_R5G6B5PC: case RGBFB_R5G5B5PC:
        case RGBFB_R5G6B5: case RGBFB_R5G5B5:
        case RGBFB_B5G6R5PC: case RGBFB_B5G5R5PC:
            return (width * 2);
        case RGBFB_R8G8B8: case RGBFB_B8G8R8:
            // Should actually return width * 3, but I'm not sure if
            // the Pi VC supports 24-bit color formats.
            // P96 will sometimes magically pad these to 32-bit anyway.
            //return (width * 3);
        case RGBFB_B8G8R8A8: case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8: case RGBFB_A8R8G8B8:
            return (width * 4);
    }
}

void SetDAC (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
    // Used to set the color format of the video card's RAMDAC.
    // This needs no handling, since the PiStorm doesn't really have a RAMDAC or a video card chipset.
}

void SetGC (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(BOOL border)) {
    b->ModeInfo = mode_info;
    // TODO: Set frame buffer width, height and format using mailbox interface?
    // I have no idea.
}

int setswitch = -1;
UWORD SetSwitch (__REGA0(struct BoardInfo *b), __REGD0(UWORD enabled)) {
    if (setswitch != enabled) {
        setswitch = enabled;
    }
    
    // TODO: Enable or disable the RTG display using mailbox interface or something

    return 1 - enabled;
}


void SetPanning (__REGA0(struct BoardInfo *b), __REGA1(UBYTE *addr), __REGD0(UWORD width), __REGD1(WORD x_offset), __REGD2(WORD y_offset), __REGD7(RGBFTYPE format)) {
    // TODO: Set the framebuffer offset to the absolute address provided in addr.
    // "width" contains the pitch of the screen being panned to.
}

unsigned int palette[256];

void SetColorArray (__REGA0(struct BoardInfo *b), __REGD0(UWORD start), __REGD1(UWORD num)) {
    // Sets the color components of X color components for 8-bit paletted display modes.
    if (!b->CLUT)
        return;
    
    int j = start + num;
    
    for(int i = start; i < j; i++) {
        unsigned long xrgb = 0 | (b->CLUT[i].Red << 16) | (b->CLUT[i].Green << 8) | (b->CLUT[i].Blue);
        palette[i] = xrgb;
    }
}


APTR CalculateMemory (__REGA0(struct BoardInfo *b), __REGA1(unsigned long addr), __REGD7(RGBFTYPE format)) {
    return (APTR)addr;
}


enum fake_rgbftypes {
    RGBF_8BPP_CLUT,
    RGBF_24BPP_RGB,
    RGBF_24BPP_BGR,
    RGBF_16BPP_RGB565_PC,
    RGBF_16BPP_RGB555_PC,
	RGBF_32BPP_ARGB,
    RGBF_32BPP_ABGR,
	RGBF_32BPP_RGBA,
    RGBF_32BPP_BGRA,
    RGBF_16BPP_RGB565,
    RGBF_16BPP_RGB555,
    RGBF_16BPP_BGR565_PC,
    RGBF_16BPP_BGR555_PC,
    RGBF_YUV_422_0,  // (Actually 4:2:0?) Just a duplicate of RGBF_YUV_422?
    RGBF_YUV_411,    // No, these are 4:2:0
    RGBF_YUV_411_PC, // No, these are 4:2:0
    RGBF_YUV_422,
    RGBF_YUV_422_PC,
    RGBF_YUV_422_PLANAR,
    RGBF_YUV_422_PLANAR_PC,
};
#define BIP(a) (1 << a)

ULONG GetCompatibleFormats (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
    //return BIP(RGBF_8BPP_CLUT) | BIP(RGBF_24BPP_RGB) | BIP(RGBF_24BPP_BGR) | BIP(RGBF_32BPP_ARGB) | BIP(RGBF_32BPP_ABGR) | BIP(RGBF_32BPP_RGBA) | BIP(RGBF_32BPP_BGRA);
    return 0xFFFFFFFF;
}

//static int display_enabled = 0;
UWORD SetDisplay (__REGA0(struct BoardInfo *b), __REGD0(UWORD enabled)) {
    // Blanks or unblanks the RTG display

    return 1;
}


LONG ResolvePixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG pixel_clock), __REGD7(RGBFTYPE format)) {
    mode_info->PixelClock = CLOCK_HZ;
    mode_info->pll1.Clock = 0;
    mode_info->pll2.ClockDivide = 1;

    return 0;
}

ULONG GetPixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG index), __REGD7(RGBFTYPE format)) {
    // Just return 100MHz.
    return CLOCK_HZ;
}

// None of these five really have to do anything.
void SetClock (__REGA0(struct BoardInfo *b)) {
}
void SetMemoryMode (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format)) {
}

void SetWriteMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask)) {
}

void SetClearMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask)) {
}

void SetReadPlane (__REGA0(struct BoardInfo *b), __REGD0(UBYTE plane)) {
}

static uint16_t vblank = 1;

void WaitVerticalSync (__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle)) {
    // TODO: Wait for vertical sync interrupt on the Raspberry Pi or whatever
    // Always returning instantly here will wreak havoc with some crap like the mouse wheel thing that waits for vertical blank to poll the mouse.
    return;
}
