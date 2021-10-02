#include <exec/types.h>
#include <exec/execbase.h>
#include <proto/exec.h>
#include <proto/expansion.h>

#include <libraries/configregs.h>
#include <libraries/configvars.h>

#include <clib/devicetree_protos.h>

#include "devicetree.h"

extern UWORD relFuncTable[];
asm(
"       .globl _relFuncTable    \n"
"_relFuncTable:                 \n"
"       .short _DT_OpenKey      \n"
"       .short _DT_CloseKey     \n"
"       .short _DT_GetChild     \n"
"       .short _DT_FindProperty \n"
"       .short _DT_GetProperty  \n"
"       .short _DT_GetPropLen   \n"
"       .short _DT_GetPropName  \n"
"       .short _DT_GetPropValue \n"
"       .short -1               \n"
);

APTR Init(struct ExecBase *SysBase asm("a6"))
{
    struct DeviceTreeBase *DeviceTreeBase = NULL;
    struct ExpansionBase *ExpansionBase = NULL;
    struct CurrentBinding binding;

    APTR base_pointer = NULL;
    
    ExpansionBase = (struct ExpansionBase *)OpenLibrary("expansion.library", 0);
    GetCurrentBinding(&binding, sizeof(binding));

    base_pointer = AllocMem(BASE_NEG_SIZE + BASE_POS_SIZE, MEMF_PUBLIC | MEMF_CLEAR);

    DeviceTreeBase = (struct DeviceTreeBase *)((UBYTE *)base_pointer + BASE_NEG_SIZE);
    MakeFunctions(DeviceTreeBase, relFuncTable, (ULONG)binding.cb_ConfigDev->cd_BoardAddr);
    
    DeviceTreeBase->dt_Node.lib_Node.ln_Type = NT_RESOURCE;
    DeviceTreeBase->dt_Node.lib_Node.ln_Pri = 120;
    DeviceTreeBase->dt_Node.lib_Node.ln_Name = (STRPTR)((ULONG)deviceName + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);

    DeviceTreeBase->dt_Node.lib_NegSize = BASE_NEG_SIZE;
    DeviceTreeBase->dt_Node.lib_PosSize = BASE_POS_SIZE;
    DeviceTreeBase->dt_Node.lib_Version = DT_VERSION;
    DeviceTreeBase->dt_Node.lib_Revision = DT_REVISION;
    DeviceTreeBase->dt_Node.lib_IdString = (STRPTR)((ULONG)deviceIdString + (ULONG)binding.cb_ConfigDev->cd_BoardAddr);    

    DeviceTreeBase->dt_ExecBase = SysBase;
    DeviceTreeBase->dt_StrNull = "(null)";
    DeviceTreeBase->dt_StrNull += (ULONG)binding.cb_ConfigDev->cd_BoardAddr;

    SumLibrary((struct Library*)DeviceTreeBase);
    AddResource(DeviceTreeBase);

    binding.cb_ConfigDev->cd_Flags &= ~CDF_CONFIGME;
    binding.cb_ConfigDev->cd_Driver = DeviceTreeBase;

    return DeviceTreeBase;
}
