/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include "devicetree.h"

void DT_CloseKey(of_node_t *node asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    (void)node;
    (void)DTBase;
}