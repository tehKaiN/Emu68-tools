#include <exec/types.h>
#include <exec/resident.h>
#include <exec/io.h>
#include <exec/devices.h>
#include <exec/errors.h>
#include <dos/dosextens.h>
#include <libraries/configregs.h>

#include <proto/exec.h>

#include <stdint.h>

asm(
"       .text               \n"
"       .globl _device_start\n"
"_device_start:             \n"
"       moveq #-1, d0       \n"
"       rts                 \n"
"       .globl _DT_DiagArea \n"
"_DT_DiagArea:              \n"
"       .byte 0x90          \n"
"       .byte 0x00          \n"
"       .word _foo - _DT_DiagArea   \n"
"       .word _DT_DiagPoint - _DT_DiagArea  \n"
"       .word _DT_BootPoint - _DT_DiagArea  \n"
"       .word _deviceName - _DT_DiagArea    \n"
"       .word 0x0000        \n"
"       .word 0x0000        \n"
);

const struct Resident RomTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&RomTag,
};

static const char deviceName[] = "devicetree.resource";
static const char deviceIdString[] = VERSION_STRING;

void DT_DiagPoint() {    
}

void DT_BootPoint() {

}
void foo() {}
