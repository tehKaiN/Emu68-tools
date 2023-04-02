/*
    Copyright © 2021 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "bcm_mbox_buffer.h"
#include <stdexcept>

namespace common {

class tMboxBufferScoped {
public:
    tMboxBufferScoped(uint32_t ulSizeInLongs)
    {
        m_pBuffer = bcm_mbox_buffer_alloc(ulSizeInLongs);
        if(m_pBuffer == nullptr) {
            throw std::runtime_error("MBox buffer allocation failed");
        }
    }

    ~tMboxBufferScoped()
    {
        bcm_mbox_buffer_free(m_pBuffer);
    }

    uint32_t *data()
    {
        return m_pBuffer->pData;
    }

private:
    tMboxBuffer *m_pBuffer;
};

} // namespace common
