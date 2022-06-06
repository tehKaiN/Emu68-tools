#include <exec/devices.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/devicetree.h>

#include "wifipi.h"
#include "sdio.h"
#include "mbox.h"

#define D(x) x

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

struct WiFiBase * WiFi_Init(struct WiFiBase *base asm("d0"), BPTR seglist asm("a0"), struct ExecBase *SysBase asm("a6"))
{
    APTR DeviceTreeBase;
    struct WiFiBase *WiFiBase = base;

    D(bug("[WiFi] WiFi_Init(%08lx, %08lx, %08lx)\n", (ULONG)base, seglist, (ULONG)SysBase));

    WiFiBase->w_SegList = seglist;
    WiFiBase->w_SysBase = SysBase;
    WiFiBase->w_UtilityBase = OpenLibrary("utility.library", 0);
    WiFiBase->w_Device.dd_Library.lib_Revision = WIFIPI_REVISION;
    
    WiFiBase->w_RequestOrig = AllocMem(512, MEMF_CLEAR);
    WiFiBase->w_Request = (APTR)(((ULONG)WiFiBase->w_RequestOrig + 31) & ~31);

    WiFiBase->w_DeviceTreeBase = DeviceTreeBase = OpenResource("devicetree.resource");

    if (DeviceTreeBase)
    {
        APTR key;

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

                    WiFiBase->w_MailBox = (APTR)reg[address_cells - 1];

                    DT_CloseKey(key);
                }
            }
            DT_CloseKey(key);
        }

        /* Open /aliases and find out the "link" to the emmc */
        key = DT_OpenKey("/aliases");
        if (key)
        {
            CONST_STRPTR mmc_alias = DT_GetPropValue(DT_FindProperty(key, "mmc"));

            DT_CloseKey(key);
               
            if (mmc_alias != NULL)
            {
                /* Open the alias and find out the MMIO VC4 physical base address */
                key = DT_OpenKey(mmc_alias);
                if (key) {
                    int size_cells = 1;
                    int address_cells = 1;

                    const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                    const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                    if (siz != NULL)
                        size_cells = *siz;
                        
                    if (addr != NULL)
                        address_cells = *addr;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                    WiFiBase->w_SDIO = (APTR)reg[address_cells - 1];
                    DT_CloseKey(key);
                }
            }               
            DT_CloseKey(key);
        }

        /* Open /aliases and find out the "link" to the emmc */
        key = DT_OpenKey("/aliases");
        if (key)
        {
            CONST_STRPTR gpio_alias = DT_GetPropValue(DT_FindProperty(key, "gpio"));

            DT_CloseKey(key);
               
            if (gpio_alias != NULL)
            {
                /* Open the alias and find out the MMIO VC4 physical base address */
                key = DT_OpenKey(gpio_alias);
                if (key) {
                    int size_cells = 1;
                    int address_cells = 1;

                    const ULONG * siz = GetPropValueRecursive(key, "#size_cells", DeviceTreeBase);
                    const ULONG * addr = GetPropValueRecursive(key, "#address-cells", DeviceTreeBase);

                    if (siz != NULL)
                        size_cells = *siz;
                        
                    if (addr != NULL)
                        address_cells = *addr;

                    const ULONG *reg = DT_GetPropValue(DT_FindProperty(key, "reg"));
                    WiFiBase->w_GPIO = (APTR)reg[address_cells - 1];
                    DT_CloseKey(key);
                }
            }               
            DT_CloseKey(key);
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

            WiFiBase->w_MailBox = (APTR)((ULONG)WiFiBase->w_MailBox - phys_vc4 + phys_cpu);
            WiFiBase->w_SDIO = (APTR)((ULONG)WiFiBase->w_SDIO - phys_vc4 + phys_cpu);
            WiFiBase->w_GPIO = (APTR)((ULONG)WiFiBase->w_GPIO - phys_vc4 + phys_cpu);

            D(bug("[WiFi]   Mailbox at %08lx\n", (ULONG)WiFiBase->w_MailBox));
            D(bug("[WiFi]   SDIO regs at %08lx\n", (ULONG)WiFiBase->w_SDIO));
            D(bug("[WiFi]   GPIO regs at %08lx\n", (ULONG)WiFiBase->w_GPIO));

            DT_CloseKey(key);
        }

        D(bug("[WiFi] Configuring GPIO alternate functions\n"));

        ULONG tmp = rd32(WiFiBase->w_GPIO, 0x0c);
        tmp &= 0xfff;       // Leave data for GPIO 30..33 intact
        tmp |= 0x3ffff000;  // GPIO 34..39 are ALT3 now
        wr32(WiFiBase->w_GPIO, 0x0c, tmp);

        D(bug("[WiFi] Enabling pull-ups \n"));

        tmp = rd32(WiFiBase->w_GPIO, 0xec);
        tmp &= 0xffff000f;  // Clear PU/PD setting for GPIO 34..39
        tmp |= 0x00005550;  // 01 in 35..59 == pull-up
        wr32(WiFiBase->w_GPIO, 0xec, tmp);

        D(bug("[WiFi] Enable GPCLK2, 32kHz on GPIO43 and output on GPIO41\n"));

        tmp = rd32(WiFiBase->w_GPIO, 0x10);
        tmp &= ~(7 << 9);   // Clear ALT-config for GPIO43
        tmp |= 4 << 9;      // GPIO43 to ALT0 == low speed clock
        tmp &= ~(7 << 3);   // Clear ALT-config for GPIO41
        tmp |= 1 << 3;      // Set GPIO41 as output
        wr32(WiFiBase->w_GPIO, 0x10, tmp);

        D(bug("[WiFi] Setting GPCLK2 to 32kHz\n"));
        wr32((void*)0xf2101000, 0x84, 0x00249f00);
        wr32((void*)0xf2101000, 0x80, 0x291);

        D(bug("[WiFi] Enabling EMMC clock\n"));
        ULONG clk = get_clock_state(1, WiFiBase);
        D(bug("[WiFi] Old clock state: %lx\n", clk));
        set_clock_state(1, 1, WiFiBase);
        clk = get_clock_state(1, WiFiBase);
        D(bug("[WiFi] New clock state: %lx\n", clk));
        clk = get_clock_rate(1, WiFiBase);
        D(bug("[WiFi] Clock speed: %ld MHz\n", clk / 1000000));
        WiFiBase->w_SDIOClock = clk;

//        D(bug("[WiFi] Setting GPIO41 to 1\n"));
//        wr32(WiFiBase->w_GPIO, 0x20, 1 << (41 - 32));

        sdio_init(WiFiBase);
    }

    D(bug("[WiFi] WiFi_Init done\n"));

    return WiFiBase;
}
