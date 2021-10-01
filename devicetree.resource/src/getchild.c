#include <exec/types.h>
#include "devicetree.h"

APTR DT_GetChild(of_node_t *key asm("a0"), of_node_t *prev asm("a1"), struct DeviceTreeBase *DTBase asm("a6"))
{
    if (prev != NULL)
        return prev->on_next;

    if (key == NULL)
        key = DTBase->dt_Root;
    
    return key->on_children;
}
