/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifndef MBOX_H
#define MBOX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <exec/execbase.h>

// Taken from bcm2711-rpi-4-b.dts
typedef enum tExtGpio {
    MBOX_EXT_GPIO_BT_ON,
    WL_ON,
    PWR_LED_OFF,
    GLOBAL_RESET,
    VDD_SD_IO_SEL,
    CAM_GPIO,
    SD_PWR_ON,
} tExtGpio;

uint32_t set_clock_rate(
    uint32_t clock_id,
    uint32_t speed,
    uint32_t FBReq[9],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t set_clock_rate(
    uint32_t clock_id,
    uint32_t speed,
    uint32_t FBReq[9],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t get_clock_state(
    uint32_t id,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t set_clock_state(
    uint32_t id,
    uint32_t state,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t get_power_state(
    uint32_t id,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t set_power_state(
    uint32_t id,
    uint32_t state,
    uint32_t FBReq[9],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t get_extgpio_state(
    tExtGpio gpio,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

uint32_t set_extgpio_state(
    tExtGpio gpio,
    uint32_t state,
    uint32_t FBReq[8],
    uint32_t *mailbox_base,
    struct ExecBase *SysBase
);

#ifdef __cplusplus
}
#endif


#endif /* MBOX_H */
