/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include <common/bcm_mbox_buffer.h>

tMboxBuffer *bcm_mbox_buffer_alloc(uint32_t ulSizeInLongs) {
    tMboxBuffer *pMboxBuffer = (tMboxBuffer *)AllocMem(
        sizeof(*pMboxBuffer), MEMF_FAST
    );
    if(pMboxBuffer == NULL) {
        return NULL;
    }

    uint32_t ulUnalignedSize = ulSizeInLongs * sizeof(uint32_t) + 31;
    pMboxBuffer->pUnaligned = (uint8_t*)AllocMem(
        pMboxBuffer->ulUnalignedSize, MEMF_FAST | MEMF_CLEAR
    );

    if(pMboxBuffer->pUnaligned == NULL) {
        FreeMem(pMboxBuffer, sizeof(*pMboxBuffer));
        return NULL;
    }

    pMboxBuffer->ulUnalignedSize = ulUnalignedSize;
	pMboxBuffer->pData = (uint32_t*)(((ULONG)pMboxBuffer->pUnaligned + 31) & ~31);
    return pMboxBuffer;
}

void bcm_mbox_buffer_free(tMboxBuffer *pMboxBuffer) {
    FreeMem(pMboxBuffer->pUnaligned, pMboxBuffer->ulUnalignedSize);
    FreeMem(pMboxBuffer, sizeof(*pMboxBuffer));
}
