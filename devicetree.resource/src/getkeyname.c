/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include "devicetree.h"

CONST_STRPTR DT_GetKeyName(of_node_t *p asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    /* If property address is given, look into it */
    if (p)
    {
        /* If property has a name (it must!) return it, otherwise return null string */
        if (p->on_name)
        {
            return p->on_name;
        }
        else
        {
            return DTBase->dt_StrNull;
        }
    }
    else
        return DTBase->dt_StrNull;
}
