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

    // Base was below 0x08000000? Check both ranges, below that base and above!
    if (base < 0x08000000) {

        // Large enough to be eventually splitted by the system?
        if (base + size > 0x08000000) {
            ULONG t1 = TypeOfMem((APTR)base + (0x08000000 - base) / 2);
            ULONG t2 = TypeOfMem((APTR)0x08000000 + (size - (0x08000000 - base)) / 2);

            if (t1 == 0 && t2 == 0) {
                AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)base, "expansion memory");
            }
            else {
                if (t1 == 0) {
                    AddMemList(0x08000000 - base, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)base, "expansion memory");
                }
                if (t2 == 0) {
                    AddMemList(size - (0x08000000 - base), MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)0x08000000, "expansion memory");
                }
            }
        }
        else
        {
            // Check middle of this memory for the type
            type = TypeOfMem((APTR)base + size/2);

            // If TypeOfMem returned 0, then Exec has not added this block of memory yet. Do it now.
            if (type == 0) {
                AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)base, "expansion memory");
            }
        }
    }
    else {
        // Check middle of this memory for the type
        type = TypeOfMem((APTR)base + size/2);

        // If TypeOfMem returned 0, then Exec has not added this block of memory yet. Do it now.
        if (type == 0) {
            AddMemList(size, MEMF_FAST | MEMF_LOCAL | MEMF_KICK | MEMF_PUBLIC , 40, (APTR)base, "expansion memory");
        }
    }
}
