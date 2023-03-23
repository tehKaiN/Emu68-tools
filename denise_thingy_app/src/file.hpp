/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _FILE_HPP_
#define _FILE_HPP_

#include <fstream>
#include <common/endian.hpp>

namespace deniseThingy::file {

template<typename t_tStream, typename t_tData>
t_tStream readLittleEndian(t_tStream &File, t_tData &Data)
{
	File.read(reinterpret_cast<char*>(&Data), sizeof(Data));
	Data = common::endian::littleToNative(Data);
}

template<typename t_tStream, typename t_tData>
void readBigEndian(t_tStream &File, t_tData &Data)
{
	File.read(reinterpret_cast<char*>(&Data), sizeof(Data));
	Data = common::endian::bigToNative(Data);
}

template<typename t_tStream, typename t_tData>
void readData(t_tStream &File, t_tData *pData, size_t ElementCount)
{
	File.read(reinterpret_cast<char*>(pData), ElementCount * sizeof(*pData));
}

} // namespace deniseThingy::file

#endif // _FILE_HPP_
