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

#ifndef _LUXCORE_LOGGER_H
#define	_LUXCORE_LOGGER_H

#include <memory>
#include <sstream>

#include "spdlog/spdlog.h"

#include "luxrays/utils/strutils.h"
#include "luxrays/utils/properties.h"

namespace luxcore {
namespace detail {

extern bool logLuxRaysEnabled;
extern bool logSDLEnabled;
extern bool logSLGEnabled;
extern bool logAPIEnabled;

extern std::shared_ptr<spdlog::logger> luxcoreLogger;

extern double lcInitTime;

#if defined(__GNUC__)
#define LC_FUNCTION_NAME __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define LC_FUNCTION_NAME __FUNCSIG__
#else
#define LC_FUNCTION_NAME __func__
#endif

#define API_BEGIN(fmt, ...) { \
	if (luxcore::detail::logAPIEnabled) { \
		luxcore::detail::luxcoreLogger->info("[API][{:.3f}] Begin [{}](" fmt ")", (luxrays::WallClockTime() - luxcore::detail::lcInitTime), LC_FUNCTION_NAME,  __VA_ARGS__); \
	} \
}

#define API_BEGIN_NOARGS() { \
	if (luxcore::detail::logAPIEnabled) { \
		luxcore::detail::luxcoreLogger->info("[API][{:.3f}] Begin [{}]()", (luxrays::WallClockTime() - luxcore::detail::lcInitTime), LC_FUNCTION_NAME); \
	} \
}

#define API_END() { \
	if (luxcore::detail::logAPIEnabled) { \
		luxcore::detail::luxcoreLogger->info("[API][{:.3f}] End [{}]()", (luxrays::WallClockTime() - luxcore::detail::lcInitTime), LC_FUNCTION_NAME); \
	} \
}

#define API_RETURN(fmt, ...) { \
	if (luxcore::detail::logAPIEnabled) { \
		luxcore::detail::luxcoreLogger->info("[API][{:.3f}] Return [{}](" fmt ")", (luxrays::WallClockTime() - luxcore::detail::lcInitTime), LC_FUNCTION_NAME,  __VA_ARGS__); \
	} \
}

template <class T>
inline std::string ToArgString(const T &t) {
	return luxrays::ToString(t);
}

inline std::string ToArgString(const std::string &t) {
	return "\"" + t + "\"";
}

inline std::string ToArgString(const luxrays::Property &p) {
	return "Property[\n" + p.ToString() + "]";
}

inline std::string ToArgString(const luxrays::Properties &p) {
	return "Properties[\n" + p.ToString() + "]";
}

template <class T>
inline std::string ToArgString(const std::vector<T> &v) {
	std::ostringstream ss;

	ss << "vector[";

	bool first = true;
	for (auto &e : v) {
		if (!first)
			ss << " ,";
		else
			first = false;

		ss << ToArgString(e);
	}

	ss << "]";
	
	return ss.str();
}

template <class T>
inline std::string ToArgString(const T *p, const size_t size) {
	std::ostringstream ss;

	ss << "array[";

	for (unsigned int i = 0; i < size; ++i) {
		if (i != 0)
			ss << " ,";

		ss << ToArgString(p[i]);
	}

	ss << "]";
	
	return ss.str();
}

}
}

#endif	/* _LUXCORE_LOGGER_H */
