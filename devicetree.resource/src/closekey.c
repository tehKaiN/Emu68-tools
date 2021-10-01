#include <exec/types.h>
#include "devicetree.h"

void DT_CloseKey(of_node_t *node asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    (void)node;
    (void)DTBase;
}