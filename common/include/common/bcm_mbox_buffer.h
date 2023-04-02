/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <proto/exec.h>

typedef struct tMboxBuffer {
    uint8_t *pUnaligned;
    uint32_t ulUnalignedSize;
    uint32_t *pData;
} tMboxBuffer;

tMboxBuffer *bcm_mbox_buffer_alloc(uint32_t ulSizeInLongs);

void bcm_mbox_buffer_free(tMboxBuffer *pMboxBuffer);

#ifdef __cplusplus
}
#endif
