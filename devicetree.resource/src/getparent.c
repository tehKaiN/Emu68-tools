/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include "devicetree.h"

APTR DT_GetParent(of_node_t *key asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    if (key == NULL)
        key = DTBase->dt_Root;
    else
        key = key->on_parent;

    return key;
}
