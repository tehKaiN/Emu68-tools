#include <proto/devicetree.h>
#include <proto/exec.h>
#include <exec/memory.h>

void Add_DT_Memory(struct ExecBase *SysBase, APTR DeviceTreeBase)
{
    ULONG base;
    ULONG size;
    ULONG type;

    // Obtain pointer to reg property of /memory. Get base and size
    const ULONG *reg = DT_GetPropValue(DT_FindProperty(DT_OpenKey("/memory"), "reg"));
    base = reg[0];
    size = reg[1];

    // Check middle of this memory for the type
    type = TypeOfMem((APTR)base + size/2);

    // If TypeOfMem returned 0, then Exec has not added this block of memory yet. Do it now.
    if (type == 0) {
        AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)base, "expansion memory");
    }
}
