/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include "devicetree.h"

APTR DT_FindProperty(of_node_t *node asm("a0"), CONST_STRPTR propname asm("a1"), struct DeviceTreeBase *DTBase asm("a6"))
{
    of_property_t *p, *prop = NULL;

    if (node)
    {
        for (p=node->on_properties; p; p=p->op_next)
        {
            if (!_strcmp(p->op_name, propname))
            {
                prop = p;
                break;
            }
        }
    }
    return prop;
}
