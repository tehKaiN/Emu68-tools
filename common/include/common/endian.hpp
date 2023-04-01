/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "endian.h"

namespace common::endian {

static inline uint32_t littleToNative(uint32_t value) { return LE32(value); }
static inline uint16_t littleToNative(uint16_t value) { return LE16(value); }
static inline uint8_t littleToNative(uint8_t value) { return value; }
static inline char littleToNative(char value) { return value; }

static inline uint32_t bigToNative(uint32_t value) { return value; }
static inline uint16_t bigToNative(uint16_t value) { return value; }
static inline uint8_t bigToNative(uint8_t value) { return value; }
static inline char bigToNative(char value) { return value; }

} // namespace common::endian
