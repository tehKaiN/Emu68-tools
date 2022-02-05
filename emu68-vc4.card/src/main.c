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
#include "vpu/block_copy.h"

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

static void putch(UBYTE data asm("d0"), APTR ignore asm("a3"))
{
    *(UBYTE*)0xdeadbeef = data;
}

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

    RawDoFmt("[vc4] FindCard\n", NULL, (APTR)putch, NULL);

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

    RawDoFmt("[vc4] Request buffer at %08lx\n", &VC4Base->vc4_Request, (APTR)putch, NULL);

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

    RawDoFmt("[vc4] MailBox at %08lx\n", &VC4Base->vc4_MailBox, (APTR)putch, NULL);

    /* Find out base address of framebuffer and video memory size */
    get_vc_memory(&VC4Base->vc4_MemBase, &VC4Base->vc4_MemSize, VC4Base);

    {
        ULONG args[] = {
            (ULONG)VC4Base->vc4_MemBase,
            (ULONG)VC4Base->vc4_MemSize / 1024
        };

        RawDoFmt("[vc4] GPU memory at %08lx, size: %ld KB\n", args, (APTR)putch, NULL);
    }

    /* Set basic data in BoardInfo structure */

    /* Get the block memory which was reserved by Emu68 on early startup. It has proper caching already */
    key = DT_OpenKey("/emu68");
    if (key)
    {
        const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "vc4-mem"));

        if (reg == NULL)
        {
            FreeMem(VC4Base->vc4_RequestBase, MBOX_SIZE);          
            CloseLibrary((struct Library *)VC4Base->vc4_DOSBase);
            CloseLibrary((struct Library *)VC4Base->vc4_IntuitionBase);
            CloseLibrary((struct Library *)VC4Base->vc4_ExpansionBase);
            
            return 0;
        }

        bi->MemoryBase = (APTR)reg[0];
        bi->MemorySize = reg[1];

        DT_CloseKey(key);
    }

    bi->RegisterBase = NULL;

    {
        ULONG args[] = {
            (ULONG)bi->MemoryBase,
            bi->MemorySize / (1024*1024)
        };
        RawDoFmt("[vc4] Memory base at %08lx, size %ldMB\n", args, (APTR)putch, NULL);
    }

    VC4Base->vc4_DispSize = get_display_size(VC4Base);

    {
        ULONG args[] = {
            VC4Base->vc4_DispSize.width,
            VC4Base->vc4_DispSize.height
        };

        RawDoFmt("[vc4] Physical display size: %ld x %ld\n", args, (APTR)putch, NULL);
    }

    VC4Base->vc4_ActivePlane = -1;

#if 0
    VC4Base->vc4_VPU_CopyBlock = (APTR)upload_code(vpu_block_copy, sizeof(vpu_block_copy), VC4Base);

    RawDoFmt("[vc4] VPU CopyBlock pointer at %08lx\n", &VC4Base->vc4_VPU_CopyBlock, (APTR)putch, NULL);
#endif

    return 1;
}

int _strcmp(const char *s1, const char *s2)
{
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

static int mitchell_netravali(double x, double b, double c)
{
    double k;

    if (x < 0)
        x = -x;
    
    if (x < 1) {
        k = (12.0 - 9.0 * b - 6.0 * c) * x * x * x + (-18.0 + 12.0 * b + 6.0 * c) * x * x + (6.0 - 2.0 * b);
    }
    else if (x < 2) {
        k = (-b - 6.0 * c) * x * x * x + (6.0 * b + 30.0 * c) * x * x + (-12.0 * b - 48.0 * c) * x + 8.0 * b + 24.0 * c;
    }
    else
        k = 0;
    
    k = 255.0 * k / 6.0 + 0.5;

    return (int)k;
}

static void compute_scaling_kernel(uint32_t *dlist_memory, double b, double c)
{
    uint32_t half_kernel[6] = {0, 0, 0, 0, 0, 0};
    int kernel_start = 0xff0;

    for (int i=0; i < 16; i++) {
        int val = mitchell_netravali(2.0 - (double)i / 7.5, b, c);
        LONG args[] = {
            i, val
        };
        half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
    }
    half_kernel[5] |= half_kernel[5] << 9;

    for (int i=0; i<11; i++) {
        if (i < 6) {
            dlist_memory[kernel_start + i] = LE32(half_kernel[i]);
        } else {
            dlist_memory[kernel_start + i] = LE32(half_kernel[11 - i - 1]);
        }
    }
}

static void compute_nearest_neighbour_kernel(uint32_t *dlist_memory)
{
    uint32_t half_kernel[6] = {0, 0, 0, 0, 0, 0};
    int kernel_start = 0xff0;

    for (int i=0; i < 16; i++) {
        int val = i < 8 ? 0 : 255;
        LONG args[] = {
            i, val
        };
        half_kernel[i / 3] |= (val & 0x1ff) << (9 * (i % 3));
    }
    half_kernel[5] |= half_kernel[5] << 9;

    for (int i=0; i<11; i++) {
        if (i < 6) {
            dlist_memory[kernel_start + i] = LE32(half_kernel[i]);
        } else {
            dlist_memory[kernel_start + i] = LE32(half_kernel[11 - i - 1]);
        }
    }
}

static int InitCard(struct BoardInfo* bi asm("a0"), const char **ToolTypes asm("a1"), struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    bi->CardBase = (struct CardBase *)VC4Base;
    bi->ExecBase = VC4Base->vc4_SysBase;
    bi->BoardName = "Emu68 VC4";
    bi->BoardType = 14;
    bi->PaletteChipType = PCT_S3ViRGE;
    bi->GraphicsControllerType = GCT_S3ViRGE;

    bi->Flags |= BIF_GRANTDIRECTACCESS | BIF_FLICKERFIXER;// | BIF_HARDWARESPRITE;// | BIF_BLITTER;
    bi->RGBFormats = 
        RGBFF_TRUEALPHA | 
        RGBFF_TRUECOLOR | 
        RGBFF_R5G6B5PC | RGBFF_R5G5B5PC | RGBFF_B5G6R5PC | RGBFF_B5G5R5PC | // RGBFF_HICOLOR | 
        RGBFF_CLUT; //RGBFF_HICOLOR | RGBFF_TRUEALPHA | RGBFF_CLUT;
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
    
    RawDoFmt("[vc4] Measuring refresh rate\n", NULL, (APTR)putch, NULL);

    Disable();

    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

    ULONG cnt1 = *stat & LE32(0x3f << 12);
    ULONG cnt2;

    /* Wait for the very next frame */
    do { cnt2 = *stat & LE32(0x3f << 12); } while(cnt2 == cnt1);
    
    /* Get current tick number */
    ULONG tick1 = LE32(*(volatile uint32_t*)0xf2003004);

    /* Wait for the very next frame */
    do { cnt1 = *stat & LE32(0x3f << 12); } while(cnt2 == cnt1);

    /* Get current tick number */
    ULONG tick2 = LE32(*(volatile uint32_t*)0xf2003004);

    Enable();

    double delta = (double)(tick2 - tick1);
    double hz = 1000000.0 / delta;

    ULONG mHz = 1000.0 * hz;

    ULONG args[] = {
        mHz / 1000,
        mHz % 1000
    };

    RawDoFmt("[vc4] Detected refresh rate of %ld.%03ld Hz\n", args, (APTR)putch, NULL);

    VC4Base->vc4_VertFreq = (ULONG)(hz+0.5);

    VC4Base->vc4_Phase = 128;
    VC4Base->vc4_Scaler = 0xc0000000;
    int kernel = 1;

    for (;ToolTypes[0] != NULL; ToolTypes++)
    {
        const char *tt = ToolTypes[0];
        ULONG args[] = {
            (ULONG)tt
        };
        RawDoFmt("[vc4] Checking ToolType `%s`\n", args, (APTR)putch, NULL);

        if (_strcmp(tt, "VC4_PHASE") == '=')
        {
            const char *c = &tt[10];
            ULONG num = 0;

            while (*c) {
                if (*c < '0' || *c > '9')
                    break;
                
                num = num * 10 + (*c++ - '0');
            }

            VC4Base->vc4_Phase = num;
            args[0] = VC4Base->vc4_Phase;
            RawDoFmt("[vc4] Setting VC4 phase to %ld\n", args, (APTR)putch, NULL);
        }
        else if (_strcmp(tt, "VC4_VERT") == '=')
        {
            const char *c = &tt[10];
            ULONG num = 0;

            while (*c) {
                if (*c < '0' || *c > '9')
                    break;
                
                num = num * 10 + (*c++ - '0');
            }

            VC4Base->vc4_VertFreq = num;
            args[0] = VC4Base->vc4_VertFreq;
            RawDoFmt("[vc4] Setting vertical frequency to %ld\n", args, (APTR)putch, NULL);
        }
        else if (_strcmp(tt, "VC4_SCALER") == '=')
        {
            switch(tt[11]) {
                case '0':
                    VC4Base->vc4_Scaler = 0x00000000;
                    break;
                case '1':
                    VC4Base->vc4_Scaler = 0x40000000;
                    break;
                case '2':
                    VC4Base->vc4_Scaler = 0x80000000;
                    break;
                case '3':
                    VC4Base->vc4_Scaler = 0xc0000000;
                    break;
            }
            args[0] = VC4Base->vc4_Scaler;
            RawDoFmt("[vc4] Setting VC4 scaler to %lx\n", args, (APTR)putch, NULL);
        }
        else if (_strcmp(tt, "VC4_KERNEL") == '=')
        {
            const char *c = &tt[11];
            ULONG num = 0;

            while (*c) {
                if (*c < '0' || *c > '9')
                    break;
                
                num = num * 10 + (*c++ - '0');
            }

            if (num == 0)
                kernel = 0;
            else
                kernel = 1;
        }
        else if (_strcmp(tt, "VC4_KERNEL_B") == '=')
        {
            const char *c = &tt[13];
            ULONG num = 0;

            while (*c) {
                if (*c < '0' || *c > '9')
                    break;
                
                num = num * 10 + (*c++ - '0');
            }

            VC4Base->vc4_Kernel_B = (double)num / 1000.0;
            args[0] = num;
            RawDoFmt("[vc4] Mitchel-Netravali B %ld\n", args, (APTR)putch, NULL);
        }
        else if (_strcmp(tt, "VC4_KERNEL_C") == '=')
        {
            const char *c = &tt[13];
            ULONG num = 0;

            while (*c) {
                if (*c < '0' || *c > '9')
                    break;
                
                num = num * 10 + (*c++ - '0');
            }

            VC4Base->vc4_Kernel_C = (double)num / 1000.0;
            args[0] = num;
            RawDoFmt("[vc4] Mitchel-Netravali C %ld\n", args, (APTR)putch, NULL);
        }
    }

    if (kernel)
        compute_scaling_kernel((uint32_t *)0xf2402000, VC4Base->vc4_Kernel_B, VC4Base->vc4_Kernel_C);
    else
        compute_nearest_neighbour_kernel((uint32_t *)0xf2402000);

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
    bi->GetVBeamPos = (void *)GetVBeamPos;
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

    RawDoFmt("[vc4] InitCard ready\n", NULL, (APTR)putch, NULL);

    return 1;
}

static struct VC4Base * OpenLib(ULONG version asm("d0"), struct VC4Base *VC4Base asm("a6"))
{
    struct ExecBase *SysBase = *(struct ExecBase **)4;
    VC4Base->vc4_LibNode.LibBase.lib_OpenCnt++;
    VC4Base->vc4_LibNode.LibBase.lib_Flags &= ~LIBF_DELEXP;

    RawDoFmt("[vc4] OpenLib\n", NULL, (APTR)putch, NULL);

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
    VC4Base->vc4_Enabled = -1;

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

UWORD CalculateBytesPerRow(struct BoardInfo *b asm("a0"), UWORD width asm("d0"), RGBFTYPE format asm("d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (!b)
        return 0;

    UWORD pitch = width;

    if (0)
    {
        ULONG args[] = {
            pitch, format
        };
        RawDoFmt("[vc4] CalculateBytesPerRow pitch %ld, format %lx\n", args, (APTR)putch, NULL);
    }
    

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
            return (width * 3);
        case RGBFB_B8G8R8A8: case RGBFB_R8G8B8A8:
        case RGBFB_A8B8G8R8: case RGBFB_A8R8G8B8:
            return (width * 4);
    }
}

void SetDAC(struct BoardInfo *b asm("a0"), RGBFTYPE format asm("d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    
    if (0)
        RawDoFmt("[vc4] SetDAC\n", NULL, (APTR)putch, NULL);
    // Used to set the color format of the video card's RAMDAC.
    // This needs no handling, since the PiStorm doesn't really have a RAMDAC or a video card chipset.
}

void SetGC(struct BoardInfo *b asm("a0"), struct ModeInfo *mode_info asm("a1"), BOOL border asm("d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    struct Size dim;
    int need_switch = 0;


    if (b->ModeInfo != mode_info) {
        need_switch = 1;
        b->ModeInfo = mode_info;
    }

    dim.width = mode_info->Width;
    dim.height = mode_info->Height;
    
    if (0)
    {
        ULONG args[] = {
            dim.width, dim.height, mode_info->Depth
        };
        RawDoFmt("[vc4] SetGC %ld x %ld x %ld\n", args, (APTR)putch, NULL);
    }

    if (need_switch) {
        VC4Base->vc4_LastPanning.lp_Addr = NULL;
        //init_display(dim, mode_info->Depth, &VC4Base->vc4_Framebuffer, &VC4Base->vc4_Pitch, VC4Base);
    }
}

UWORD SetSwitch(struct BoardInfo *b asm("a0"), UWORD enabled asm("d0"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (0)
    {
        ULONG args[] = {
            enabled
        };
        RawDoFmt("[vc4] SetSwitch %ld\n", args, (APTR)putch, NULL);
    }

    if (VC4Base->vc4_Enabled != enabled) {
        VC4Base->vc4_Enabled = enabled;

        switch(enabled) {
            case 0:
                blank_screen(1, VC4Base);
                break;
            default:
                blank_screen(0, VC4Base);
                break;
        }
    }
    
    return 1 - enabled;
}

static const ULONG mode_table[] = {
    [RGBFB_A8R8G8B8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_RGBA),
    [RGBFB_A8B8G8R8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_BGRA),
    [RGBFB_B8G8R8A8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_ABGR),
    [RGBFB_R8G8B8A8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGBA8888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_ARGB),

    [RGBFB_R8G8B8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),
    [RGBFB_B8G8R8] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB888) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),

    [RGBFB_R5G6B5PC] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    [RGBFB_R5G5B5PC] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    
    [RGBFB_R5G6B5] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    [RGBFB_R5G5B5] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XRGB),
    
    [RGBFB_B5G6R5PC] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB565) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),
    [RGBFB_B5G5R5PC] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_RGB555) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR),

    [RGBFB_CLUT] = CONTROL_FORMAT(HVS_PIXEL_FORMAT_PALETTE) | CONTROL_PIXEL_ORDER(HVS_PIXEL_ORDER_XBGR)
};

int AllocSlot(UWORD size, struct VC4Base *VC4Base)
{
    int ret = VC4Base->vc4_FreePlane;
    int next_free = VC4Base->vc4_FreePlane + size;

    if (next_free >= 0x300)
    {
        ret = 0;
        next_free = ret + size;
    }

    VC4Base->vc4_FreePlane = next_free;

    return ret;
}

void SetPanning (struct BoardInfo *b asm("a0"), UBYTE *addr asm("a1"), UWORD width asm("d0"), WORD x_offset asm("d1"), WORD y_offset asm("d2"), RGBFTYPE format asm("d7"))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    int unity = 0;
    ULONG scale_x = 0;
    ULONG scale_y = 0;
    ULONG scale = 0;
    ULONG recip_x = 0;
    ULONG recip_y = 0;
    UWORD offset_x = 0;
    UWORD offset_y = 0;
    ULONG calc_width = 0;
    ULONG calc_height = 0;
    ULONG bytes_per_row = CalculateBytesPerRow(b, width, format);
    ULONG bytes_per_pix = bytes_per_row / width;
    UWORD pos = 0;
    int offset_only = 0;

    if (0) {
        ULONG args[] = {
            (ULONG)addr, width, x_offset, y_offset, format
        };
        RawDoFmt("[vc4] SetPanning %lx %ld %ld %ld %lx\n", args, (APTR)putch, NULL);
    }

    if (VC4Base->vc4_LastPanning.lp_Addr != NULL && 
        width == VC4Base->vc4_LastPanning.lp_Width &&
        format == VC4Base->vc4_LastPanning.lp_Format)
    {
        if (addr == VC4Base->vc4_LastPanning.lp_Addr && x_offset == VC4Base->vc4_LastPanning.lp_X && y_offset == VC4Base->vc4_LastPanning.lp_Y) {
            if (0) {
                RawDoFmt("[vc4] same panning as before. Skipping now\n", NULL, (APTR)putch, NULL);
            }
            return;
        }

        offset_only = 1;
    }

    VC4Base->vc4_LastPanning.lp_Addr = addr;
    VC4Base->vc4_LastPanning.lp_Width = width;
    VC4Base->vc4_LastPanning.lp_X = x_offset;
    VC4Base->vc4_LastPanning.lp_Y = y_offset;
    VC4Base->vc4_LastPanning.lp_Format = format;

    if (format != RGBFB_CLUT &&
        b->ModeInfo->Width == VC4Base->vc4_DispSize.width &&
        b->ModeInfo->Height == VC4Base->vc4_DispSize.height)
    {
        unity = 1;
    }
    else
    {
        ULONG factor_y = (b->ModeInfo->Flags & GMF_DOUBLESCAN) ? 0x20000 : 0x10000;
        scale_x = 0x10000 * b->ModeInfo->Width / VC4Base->vc4_DispSize.width;
        scale_y = factor_y * b->ModeInfo->Height / VC4Base->vc4_DispSize.height;

        recip_x = 0xffffffff / scale_x;
        recip_y = 0xffffffff / scale_y;

        // Select larger scaling factor from X and Y, but it need to fit
        if (((factor_y * b->ModeInfo->Height) / scale_x) > VC4Base->vc4_DispSize.height) {
            scale = scale_y;
        }
        else {
            scale = scale_x;
        }

        calc_width = (0x10000 * b->ModeInfo->Width) / scale;
        calc_height = (factor_y * b->ModeInfo->Height) / scale;

        offset_x = (VC4Base->vc4_DispSize.width - calc_width) >> 1;
        offset_y = (VC4Base->vc4_DispSize.height - calc_height) >> 1;

        ULONG args[] = {
            scale, scale_x, scale_y, recip_x, recip_y,
            calc_width, calc_height, offset_x, offset_y
        };

        if (0)
            RawDoFmt("[vc4] Selected scale: %08lx (X: %08lx, Y: %08lx, 1/X: %08lx, 1/Y: %08lx)\n"
                    "[vc4] Scaled size: %ld x %ld, offset X %ld, offset Y %ld\n", args, (APTR)putch, NULL);
    }

    volatile uint32_t *displist = (uint32_t *)0xf2402000;
   
    if (unity) {
        if (offset_only) {
            pos = VC4Base->vc4_ActivePlane;
            displist[pos + 4] = LE32(0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
        }
        else {
            pos = AllocSlot(8, VC4Base);

            displist[pos + 0] = LE32(
                CONTROL_VALID
                | CONTROL_WORDS(7)
                | CONTROL_UNITY
                | mode_table[format]);

            displist[pos + 1] = LE32(POS0_X(offset_x) | POS0_Y(offset_y) | POS0_ALPHA(0xff));
            displist[pos + 2] = LE32(POS2_H(b->ModeInfo->Height) | POS2_W(b->ModeInfo->Width) | (1 << 30));
            displist[pos + 3] = LE32(0xdeadbeef);
            displist[pos + 4] = LE32(0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            displist[pos + 5] = LE32(0xdeadbeef);
            displist[pos + 6] = LE32(bytes_per_row);
            displist[pos + 7] = LE32(0x80000000);
        }
    } else {
        if (offset_only) {
            pos = VC4Base->vc4_ActivePlane;
            displist[pos + 5] = LE32(0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
        }
        else 
        {
            pos = AllocSlot(18, VC4Base);
            int cnt = pos + 1;

            displist[cnt++] = LE32(POS0_X(offset_x) | POS0_Y(offset_y) | POS0_ALPHA(0xff));
            displist[cnt++] = LE32(POS1_H(calc_height) | POS1_W(calc_width));
            displist[cnt++] = LE32(POS2_H(b->ModeInfo->Height) | POS2_W(b->ModeInfo->Width) | (SCALER_POS2_ALPHA_MODE_FIXED << SCALER_POS2_ALPHA_MODE_SHIFT));
            displist[cnt++] = LE32(0xdeadbeef); // Scratch written by HVS

            displist[cnt++] = LE32(0xc0000000 | (ULONG)addr + y_offset * bytes_per_row + x_offset * bytes_per_pix);
            displist[cnt++] = LE32(0xdeadbeef); // Scratch written by HVS

            // Write pitch
            displist[cnt++] = LE32(bytes_per_row);

            // Palette mode - offset of palette placed in dlist
            if (format == RGBFB_CLUT) {
                displist[cnt++] = LE32(0xc0000000 | (0x300 << 2));
            }

            // LMB address
            displist[cnt++] = LE32(0);

            // Write PPF Scaling
            displist[cnt++] = LE32((scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
                displist[cnt++] = LE32(((scale << 7) & ~0xff) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            else
                displist[cnt++] = LE32((scale << 8) | VC4Base->vc4_Scaler | VC4Base->vc4_Phase);
            displist[cnt++] = LE32(0); // Scratch written by HVS

            // Write scaling kernel offset in dlist
            displist[cnt++] = LE32(0xff0);
            displist[cnt++] = LE32(0xff0);
            displist[cnt++] = LE32(0xff0);
            displist[cnt++] = LE32(0xff0);

            displist[cnt++] = LE32(0x80000000);

            displist[pos] = LE32(
                CONTROL_VALID               |
                CONTROL_WORDS(cnt-pos-1)    |
                0x01800 | 
                mode_table[format]
            );
        }
    }

    if (pos != VC4Base->vc4_ActivePlane)
    {
        volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

        // Wait for vertical blank before updating the display list
        do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VC4Base->vc4_DispSize.height);

        *(volatile uint32_t *)0xf2400024 = LE32(pos);
        VC4Base->vc4_ActivePlane = pos;
    }
}

void SetColorArray (__REGA0(struct BoardInfo *b), __REGD0(UWORD start), __REGD1(UWORD num)) {
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    volatile uint32_t *displist = (uint32_t *)0xf2402000;

    // Sets the color components of X color components for 8-bit paletted display modes.
    if (!b->CLUT)
        return;
    
    if (0)
    {
        ULONG args[] = {
            start, num
        };
        RawDoFmt("[vc4] SetColorArray %ld %ld\n", args, (APTR)putch, NULL);
    }

    int j = start + num;
    
    for(int i = start; i < j; i++) {
        unsigned long xrgb = 0xff000000 | (b->CLUT[i].Blue) | (b->CLUT[i].Green << 8) | (b->CLUT[i].Red << 16);
        displist[0x300 + i] = LE32(xrgb);
    }
}


APTR CalculateMemory (__REGA0(struct BoardInfo *b), __REGA1(unsigned long addr), __REGD7(RGBFTYPE format)) {
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;

    if (0)
    {
        ULONG args[] = {
            addr, format
        };
        RawDoFmt("[vc4] CalculateMemory %lx %lx\n", args, (APTR)putch, NULL);
    }

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
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    if (0)
    {
        ULONG args[] = {
            format
        };
        RawDoFmt("[vc4] GetCompatibleFormats %lx\n", args, (APTR)putch, NULL);
    }
    //return BIP(RGBF_8BPP_CLUT) | BIP(RGBF_24BPP_RGB) | BIP(RGBF_24BPP_BGR) | BIP(RGBF_32BPP_ARGB) | BIP(RGBF_32BPP_ABGR) | BIP(RGBF_32BPP_RGBA) | BIP(RGBF_32BPP_BGRA);
    return 0xFFFFFFFF;
}

//static int display_enabled = 0;
UWORD SetDisplay (__REGA0(struct BoardInfo *b), __REGD0(UWORD enabled))
{
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    
    if (0)
    {
        ULONG args[] = {
            enabled
        };
        RawDoFmt("[vc4] SetDisplay %ld\n", args, (APTR)putch, NULL);
    }
    if (enabled) {
        blank_screen(0, VC4Base);
    } else {
        blank_screen(1, VC4Base);
    }

    return 1;
}


LONG ResolvePixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG pixel_clock), __REGD7(RGBFTYPE format)) {

    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    struct ExecBase *SysBase = VC4Base->vc4_SysBase;
    
    if (0)
    {
        ULONG args[] = {
            (ULONG)mode_info, pixel_clock, format
        };
        RawDoFmt("[vc4] ResolvePixelClock %lx %ld %lx\n", args, (APTR)putch, NULL);
    }

    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VC4Base->vc4_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    mode_info->PixelClock = clock;
    mode_info->pll1.Clock = 0;
    mode_info->pll2.ClockDivide = 1;

    return 0;
}

ULONG GetPixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG index), __REGD7(RGBFTYPE format)) {
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    
    ULONG clock = mode_info->HorTotal * mode_info->VerTotal * VC4Base->vc4_VertFreq;

    if (b->ModeInfo->Flags & GMF_DOUBLESCAN)
        clock <<= 1;

    return clock;
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

ULONG GetVBeamPos(struct BoardInfo *b asm("a0"))
{
    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);
    ULONG vbeampos = LE32(*stat) & 0xfff;

    return vbeampos;
}

void WaitVerticalSync (__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle)) {
    struct VC4Base *VC4Base = (struct VC4Base *)b->CardBase;
    volatile ULONG *stat = (ULONG*)(0xf2400000 + SCALER_DISPSTAT1);

    // Wait until current vbeampos is lower than the one obtained above
    do { asm volatile("nop"); } while((LE32(*stat) & 0xfff) != VC4Base->vc4_DispSize.height);
}

#if 0

unsigned char a[] = {
  0x10, 0xf0, 0x00, 0xc0, 0x80, 0x03, 0x10, 0xf8, 0x40, 0xc0, 0x40, 0x00,
  0xc0, 0xf3, 0x00, 0x00, 0x10, 0xf8, 0x80, 0xc0, 0x00, 0x00, 0xc0, 0xf3,
  0x01, 0x00, 0x10, 0xf8, 0xc0, 0xc0, 0x40, 0x00, 0xc0, 0xf3, 0x01, 0x00,
  0x90, 0xf0, 0x30, 0x00, 0x81, 0x03, 0x90, 0xf8, 0x30, 0x00, 0x40, 0x10,
  0xc0, 0xf3, 0x04, 0x00, 0x90, 0xf8, 0x30, 0x00, 0x00, 0x20, 0xc0, 0xf3,
  0x05, 0x00, 0x90, 0xf8, 0x30, 0x00, 0x40, 0x30, 0xc0, 0xf3, 0x05, 0x00,
  0x40, 0xe8, 0x00, 0x01, 0x00, 0x00, 0x41, 0xe8, 0x00, 0x01, 0x00, 0x00,
  0x12, 0x66, 0x02, 0x6a, 0xd4, 0x18, 0x5a, 0x00
};

void test()
{
    int c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x3000c);
    FBReq[c++] = LE32(12);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(sizeof(a));  // 32 bytes
    FBReq[c++] = LE32(4);   // 4 byte align
    FBReq[c++] = LE32((3 << 2) | (1 << 6));   // COHERENT | DIRECT | HINT_PERMALOCK
    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);

    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    int handle = LE32(FBReq[5]);
    kprintf("Alloc returned %d\n", handle);

    c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x0003000d);
    FBReq[c++] = LE32(4);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(handle);  // 32 bytes
    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);
    
    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);

    uint64_t phys = LE32(FBReq[5]);
    uint64_t cpu = phys & ~0xc0000000;
    kprintf("Locl memory returned %08x, CPU addr %08x\n", phys, cpu);

    phys &= ~0xc0000000;
    for (unsigned i=0; i < sizeof(a); i++)
        ((uint8_t *)cpu)[i] = a[i];
    arm_flush_cache(cpu, sizeof(a));

    kprintf("test code uploaded\n");

    kprintf("running code with r0=%08x, r1=%08x, r2=%08x\n", 0xc0000000, fb_phys_base, 30000);

    uint32_t t0 = LE32(*(volatile uint32_t*)0xf2003004);

for (int i=0; i < 20000; i++) {
    if (i == 1)
        phys++;
    c = 1;
    FBReq[c++] = 0;
    FBReq[c++] = LE32(0x00030010);
    FBReq[c++] = LE32(28);
    FBReq[c++] = 0;
    FBReq[c++] = LE32(phys);  // code address
    FBReq[c++] = LE32(0xc0000000 + (0x3fffffff & ((uint64_t)i * 15000*256))); // r0 source address
    FBReq[c++] = LE32(fb_phys_base); // r1 dest address
    FBReq[c++] = LE32(7500); // r2 Number of 256-byte packets
    FBReq[c++] = 0; // r3
    FBReq[c++] = 0; // r4
    FBReq[c++] = 0; // r5

    FBReq[c++] = 0;
    FBReq[0] = LE32(4*c);
    
    arm_flush_cache((intptr_t)FBReq, 4*c);
    mbox_send(8, mmu_virt2phys((intptr_t)FBReq));
    mbox_recv(8);
}

    uint32_t t1 = LE32(*(volatile uint32_t*)0xf2003004);

    kprintf("Returned from test code. Retval = %08x\n", LE32(FBReq[5]));
    kprintf("Time wasted: %d milliseconds\n", (t1 - t0) / 1000);
    
}

#endif
