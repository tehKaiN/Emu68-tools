#include <exec/types.h>
#include <proto/exec.h>
#include <clib/devicetree_protos.h>

const CONST_APTR funcTable[] = {
    (CONST_APTR) DT_OpenKey,
    (CONST_APTR) DT_CloseKey,
    (CONST_APTR) DT_GetChild,
    (CONST_APTR) DT_FindProperty,
    (CONST_APTR) DT_GetProperty,
    (CONST_APTR) DT_GetPropLen,
    (CONST_APTR) DT_GetPropName,
    (CONST_APTR) DT_GetPropValue,
    (CONST_APTR) -1
};

void Init()
{

}
