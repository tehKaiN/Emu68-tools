#ifndef __DEVICETREE_H
#define __DEVICETREE_H

#include <exec/types.h>
#include <exec/nodes.h>
#include <exec/lists.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <stdint.h>

typedef struct of_property {
    struct of_property *op_next;
    const char *        op_name;
    uint32_t            op_length;
    const void *        op_value;
} of_property_t;

typedef struct of_node {
    struct of_node *    on_next;
    struct of_node *    on_parent;
    const char *        on_name;
    struct of_node *    on_children;
    of_property_t *     on_properties;
} of_node_t;

struct DeviceTreeBase {
    struct Library  dt_Node;
    struct SysBase *dt_ExecBase;
    of_node_t *     dt_Root;
    CONST_STRPTR    dt_StrNull;
};

static inline int dt_strcmp(const char *s1, const char *s2)
{   
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    
    if (*s1 == 0 && s2[-1] == '@') {
        return 0;
    }
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

static inline int _strcmp(const char *s1, const char *s2)
{   
    while (*s1 == *s2++)
        if (*s1++ == '\0')
            return (0);
    return (*(const unsigned char *)s1 - *(const unsigned char *)(s2 - 1));
}

#endif /* __DEVICETREE_H */