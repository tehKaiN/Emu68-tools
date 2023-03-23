/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TIME_HPP
#define TIME_HPP

#include <chrono>
extern "C" {
#include <clib/dos_protos.h>
}

namespace deniseThingy::time {

template<typename t_tDuration>
void sleepFor(t_tDuration Duration)
{
	auto Millis = std::chrono::duration_cast<std::chrono::milliseconds>(Duration);
	Delay(Millis.count() / 50u);
}

}

#endif // TIME_HPP
