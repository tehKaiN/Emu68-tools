/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <exec/types.h>
#include "devicetree.h"

APTR DT_OpenKey(CONST_STRPTR key asm("a0"), struct DeviceTreeBase *DTBase asm("a6"))
{
    char ptrbuf[64];
    int i;
    of_node_t *node, *ret = NULL;
    
    if (key[0] == '/' && key[1] == 0)
        return DTBase->dt_Root;
    
    if (*key == '/')
    {   
        ret = DTBase->dt_Root;
        
        while(*key)
        {   
            key++;
            for (i=0; i < 63; i++)
            {   
                if (*key == '/' || *key == 0)
                    break;
                ptrbuf[i] = *key;
                key++;
            }
            
            ptrbuf[i] = 0;
            
            for (node = ret->on_children; node; node = node->on_next)
            {   
                if (!dt_strcmp(ptrbuf, node->on_name))
                {   
                    return node;
                }
            }
        }
    }

    return NULL;
}
