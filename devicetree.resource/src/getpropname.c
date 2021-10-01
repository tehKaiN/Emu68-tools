#include <exec/types.h>
#include "devicetree.h"

CONST_STRPTR DT_GetPropName(of_property_t *p asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    /* If property address is given, look into it */
    if (p)
    {
        /* If property has a name (it must!) return it, otherwise return null string */
        if (p->op_name)
        {
            return p->op_name;
        }
        else
        {
            return DTBase->dt_StrNull;
        }
    }
    else
        return DTBase->dt_StrNull;
}