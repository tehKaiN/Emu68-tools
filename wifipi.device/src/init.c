#include <exec/devices.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/devicetree.h>

#include "wifipi.h"

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
    WiFiBase->w_Device.dd_Library.lib_Revision = WIFIPI_REVISION;

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

            D(bug("[WiFi]   Mailbox at %08lx\n", (ULONG)WiFiBase->w_MailBox));
            D(bug("[WiFi]   SDIO regs at %08lx\n", (ULONG)WiFiBase->w_SDIO));

            DT_CloseKey(key);
        }
    }

    D(bug("[WiFi] WiFi_Init done\n"));

    return WiFiBase;
}
