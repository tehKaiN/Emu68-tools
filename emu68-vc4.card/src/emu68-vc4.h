#ifndef _EMU68_VC4_H
#define _EMU68_VC4_H

#include <exec/types.h>
#include <exec/libraries.h>

#include <dos/dos.h>
#include <intuition/intuitionbase.h>
#include <libraries/expansionbase.h>

#include "boardinfo.h"

#define STR(s) #s
#define XSTR(s) STR(s)

#define VC4CARD_VERSION  0
#define VC4CARD_REVISION 1
#define VC4CARD_PRIORITY 0
#define MBOX_SIZE        (512 * 4)

#define CLOCK_HZ        100000000

struct Size {
    UWORD width;
    UWORD height;
};

struct VC4Base {
    struct CardBase         vc4_LibNode;
    BPTR                    vc4_SegList;
    struct ExecBase *       vc4_SysBase;
    struct ExpansionBase *  vc4_ExpansionBase;
    struct DOSBase *        vc4_DOSBase;
    struct IntuitionBase *  vc4_IntuitionBase;
    APTR                    vc4_DeviceTreeBase;
    APTR                    vc4_MailBox;
    APTR                    vc4_HVS;
    APTR                    vc4_RequestBase;
    APTR                    vc4_Request;
    APTR                    vc4_MemBase;
    uint32_t                vc4_MemSize;
    APTR                    vc4_Framebuffer;
    uint32_t                vc4_Pitch;
    uint16_t                vc4_Enabled;

    struct Size             vc4_DispSize;

    APTR                    vc4_VPU_CopyBlock;
};




void SetDAC(struct BoardInfo *bi asm("a0"), RGBFTYPE format asm("d7"));
void SetGC(struct BoardInfo *bi asm("a0"), struct ModeInfo *mode_info asm("a1"), BOOL border asm("d0"));
UWORD SetSwitch (__REGA0(struct BoardInfo *b), __REGD0(UWORD enabled));
void SetPanning (__REGA0(struct BoardInfo *b), __REGA1(UBYTE *addr), __REGD0(UWORD width), __REGD1(WORD x_offset), __REGD2(WORD y_offset), __REGD7(RGBFTYPE format));
void SetColorArray (__REGA0(struct BoardInfo *b), __REGD0(UWORD start), __REGD1(UWORD num));
UWORD CalculateBytesPerRow (__REGA0(struct BoardInfo *b), __REGD0(UWORD width), __REGD7(RGBFTYPE format));
APTR CalculateMemory (__REGA0(struct BoardInfo *b), __REGA1(unsigned long addr), __REGD7(RGBFTYPE format));
ULONG GetCompatibleFormats (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));
UWORD SetDisplay (__REGA0(struct BoardInfo *b), __REGD0(UWORD enabled));
LONG ResolvePixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG pixel_clock), __REGD7(RGBFTYPE format));
ULONG GetPixelClock (__REGA0(struct BoardInfo *b), __REGA1(struct ModeInfo *mode_info), __REGD0(ULONG index), __REGD7(RGBFTYPE format));
void SetClock (__REGA0(struct BoardInfo *b));
void SetMemoryMode (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));
void SetWriteMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask));
void SetClearMask (__REGA0(struct BoardInfo *b), __REGD0(UBYTE mask));
void SetReadPlane (__REGA0(struct BoardInfo *b), __REGD0(UBYTE plane));
void WaitVerticalSync (__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle));
BOOL GetVSyncState(__REGA0(struct BoardInfo *b), __REGD0(BOOL toggle));
void FillRect (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(ULONG color), __REGD5(UBYTE mask), __REGD7(RGBFTYPE format));
void InvertRect (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(UBYTE mask), __REGD7(RGBFTYPE format));
void BlitRect (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD dx), __REGD3(WORD dy), __REGD4(WORD w), __REGD5(WORD h), __REGD6(UBYTE mask), __REGD7(RGBFTYPE format));
void BlitRectNoMaskComplete (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *rs), __REGA2(struct RenderInfo *rt), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD dx), __REGD3(WORD dy), __REGD4(WORD w), __REGD5(WORD h), __REGD6(UBYTE minterm), __REGD7(RGBFTYPE format));
void BlitTemplate (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGA2(struct Template *t), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(UBYTE mask), __REGD7(RGBFTYPE format));
void BlitPattern (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGA2(struct Pattern *p), __REGD0(WORD x), __REGD1(WORD y), __REGD2(WORD w), __REGD3(WORD h), __REGD4(UBYTE mask), __REGD7(RGBFTYPE format));
void DrawLine (__REGA0(struct BoardInfo *b), __REGA1(struct RenderInfo *r), __REGA2(struct Line *l), __REGD0(UBYTE mask), __REGD7(RGBFTYPE format));
void BlitPlanar2Chunky (__REGA0(struct BoardInfo *b), __REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *r), __REGD0(SHORT x), __REGD1(SHORT y), __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT w), __REGD5(SHORT h), __REGD6(UBYTE minterm), __REGD7(UBYTE mask));
void BlitPlanar2Direct (__REGA0(struct BoardInfo *b), __REGA1(struct BitMap *bm), __REGA2(struct RenderInfo *r), __REGA3(struct ColorIndexMapping *clut), __REGD0(SHORT x), __REGD1(SHORT y), __REGD2(SHORT dx), __REGD3(SHORT dy), __REGD4(SHORT w), __REGD5(SHORT h), __REGD6(UBYTE minterm), __REGD7(UBYTE mask));
void SetSprite (__REGA0(struct BoardInfo *b), __REGD0(BOOL enable), __REGD7(RGBFTYPE format));
void SetSpritePosition (__REGA0(struct BoardInfo *b), __REGD0(WORD x), __REGD1(WORD y), __REGD7(RGBFTYPE format));
void SetSpriteImage (__REGA0(struct BoardInfo *b), __REGD7(RGBFTYPE format));
void SetSpriteColor (__REGA0(struct BoardInfo *b), __REGD0(UBYTE idx), __REGD1(UBYTE R), __REGD2(UBYTE G), __REGD3(UBYTE B), __REGD7(RGBFTYPE format));

/* Endian support */

static inline uint64_t LE64(uint64_t x) { return __builtin_bswap64(x); }
static inline uint32_t LE32(uint32_t x) { return __builtin_bswap32(x); }
static inline uint16_t LE16(uint16_t x) { return __builtin_bswap16(x); }

#define CONTROL_FORMAT(n)       (n & 0xf)
#define CONTROL_END             (1<<31)
#define CONTROL_VALID           (1<<30)
#define CONTROL_WORDS(n)        (((n) & 0x3f) << 24)
#define CONTROL0_FIXED_ALPHA    (1<<19)
#define CONTROL0_HFLIP          (1<<16)
#define CONTROL0_VFLIP          (1<<15)
#define CONTROL_PIXEL_ORDER(n)  ((n & 3) << 13)
#define CONTROL_UNITY           (1<<4)

enum hvs_pixel_format {
        /* 8bpp */
        HVS_PIXEL_FORMAT_RGB332 = 0,
        /* 16bpp */
        HVS_PIXEL_FORMAT_RGBA4444 = 1,
        HVS_PIXEL_FORMAT_RGB555 = 2,
        HVS_PIXEL_FORMAT_RGBA5551 = 3,
        HVS_PIXEL_FORMAT_RGB565 = 4,
        /* 24bpp */
        HVS_PIXEL_FORMAT_RGB888 = 5,
        HVS_PIXEL_FORMAT_RGBA6666 = 6,
        /* 32bpp */
        HVS_PIXEL_FORMAT_RGBA8888 = 7,

        HVS_PIXEL_FORMAT_YCBCR_YUV420_3PLANE = 8,
        HVS_PIXEL_FORMAT_YCBCR_YUV420_2PLANE = 9,
        HVS_PIXEL_FORMAT_YCBCR_YUV422_3PLANE = 10,
        HVS_PIXEL_FORMAT_YCBCR_YUV422_2PLANE = 11,
        HVS_PIXEL_FORMAT_H264 = 12,
        HVS_PIXEL_FORMAT_PALETTE = 13,
        HVS_PIXEL_FORMAT_YUV444_RGB = 14,
        HVS_PIXEL_FORMAT_AYUV444_RGB = 15,
        HVS_PIXEL_FORMAT_RGBA1010102 = 16,
        HVS_PIXEL_FORMAT_YCBCR_10BIT = 17,
};

#define HVS_PIXEL_ORDER_RGBA                    0
#define HVS_PIXEL_ORDER_BGRA                    1
#define HVS_PIXEL_ORDER_ARGB                    2
#define HVS_PIXEL_ORDER_ABGR                    3

#define HVS_PIXEL_ORDER_XBRG                    0
#define HVS_PIXEL_ORDER_XRBG                    1
#define HVS_PIXEL_ORDER_XRGB                    2
#define HVS_PIXEL_ORDER_XBGR                    3

#define SCALER_CTL0_SCL_H_PPF_V_PPF             0
#define SCALER_CTL0_SCL_H_TPZ_V_PPF             1
#define SCALER_CTL0_SCL_H_PPF_V_TPZ             2
#define SCALER_CTL0_SCL_H_TPZ_V_TPZ             3
#define SCALER_CTL0_SCL_H_PPF_V_NONE            4
#define SCALER_CTL0_SCL_H_NONE_V_PPF            5
#define SCALER_CTL0_SCL_H_NONE_V_TPZ            6
#define SCALER_CTL0_SCL_H_TPZ_V_NONE            7

#define POS0_X(n) (n & 0xfff)
#define POS0_Y(n) ((n & 0xfff) << 12)
#define POS0_ALPHA(n) ((n & 0xff) << 24)

#define POS1_W(n) (n & 0xffff)
#define POS1_H(n) ((n & 0xffff) << 16)

#define POS2_W(n) (n & 0xffff)
#define POS2_H(n) ((n & 0xffff) << 16)

#define SCALER_POS2_ALPHA_MODE_MASK             0xc0000000
#define SCALER_POS2_ALPHA_MODE_SHIFT            30
#define SCALER_POS2_ALPHA_MODE_PIPELINE         0
#define SCALER_POS2_ALPHA_MODE_FIXED            1
#define SCALER_POS2_ALPHA_MODE_FIXED_NONZERO    2
#define SCALER_POS2_ALPHA_MODE_FIXED_OVER_0x07  3
#define SCALER_POS2_ALPHA_PREMULT               (1 << 29)
#define SCALER_POS2_ALPHA_MIX                   (1 << 28)

#define SCALER_POS2_HEIGHT_MASK                 0x0fff0000
#define SCALER_POS2_HEIGHT_SHIFT                16

#define SCALER_POS2_WIDTH_MASK                  0x00000fff
#define SCALER_POS2_WIDTH_SHIFT                 0

#endif /* _EMU68_VC4_H */
