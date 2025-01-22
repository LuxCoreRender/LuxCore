/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
 *                                                                         *
 * Licensed under the Apache License, Version 2.0 (the "License");         *
 * you may not use this file except in compliance with the License.        *
 * You may obtain a copy of the License at                                 *
 *                                                                         *
 *     http://www.apache.org/licenses/LICENSE-2.0                          *
 *                                                                         *
 * Unless required by applicable law or agreed to in writing, software     *
 * distributed under the License is distributed on an "AS IS" BASIS,       *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and     *
 * limitations under the License.                                          *
 ***************************************************************************/

#ifndef _SLG_HALFSERIALIZATION_H
#define	_SLG_HALFSERIALIZATION_H

#include <Imath/half.h>

#include "luxrays/utils/utils.h"
#include "luxrays/utils/serializationutils.h"

BOOST_SERIALIZATION_SPLIT_FREE(half)

namespace boost {
namespace serialization {

template<class Archive> void save(Archive &ar, const half &h, unsigned int version) {
	const u_short bits = h.bits();
	ar & bits;
}

template<class Archive> void load(Archive &ar, half &h, unsigned int version) {
	u_short bits;
    ar & bits;
	h.setBits(bits);
}

}
}

#endif	/* _SLG_HALFSERIALIZATION_H */
