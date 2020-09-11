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

#ifndef _LUXRAYS_STRUTILS_H
#define	_LUXRAYS_STRUTILS_H

#include <string>
#include <sstream>
#include <limits>

#include "luxrays/luxrays.h"

namespace luxrays {

template <class T> inline std::string ToString(const T &t) {
	std::ostringstream ss;

	ss.imbue(cLocale);

	ss << t;
	
	return ss.str();
}

inline std::string ToString(const float t) {
	std::ostringstream ss;
	
	ss.imbue(cLocale);

	ss << std::setprecision(std::numeric_limits<float>::digits10 + 1) << t;

	return ss.str();
}

template <class T> inline T FromString(const std::string &str) {
	std::istringstream ss(str);

	ss.imbue(cLocale);

	T result;
	ss >> result;
	
	return result;
}

inline std::string ToMemString(const size_t size) {
	return (size < 10000 ? (ToString(size) + "bytes") : (ToString(size / 1024) + "Kbytes"));
}

}

#endif	/* _LUXRAYS_STRUTILS_H */
