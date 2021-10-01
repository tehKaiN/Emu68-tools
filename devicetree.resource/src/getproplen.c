#include <exec/types.h>
#include "devicetree.h"

ULONG DT_GetPropLen(of_property_t *p asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    /* If property address is given, look into it */
    if (p)
    {
        /* If property has a name (it must!) return it, otherwise return null string */
        if (p->op_length)
        {
            return p->op_length;
        }
        else
        {
            return 0;
        }
    }
    else
        return 0;
}