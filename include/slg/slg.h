/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_H
#define	_SLG_H

#include <sstream>

#include "luxrays/luxrays.h"

/*!
 * \namespace slg
 *
 * \brief The SLG classes are defined within this namespace.
 */

namespace slg {

class RenderSession;
class RenderConfig;
class RenderEngine;
class EditActionList;

// The next two functions pointers (plus the one in sdl.h) have to be
// set by the application using SLG library
extern void (*LuxRays_DebugHandler)(const char *msg); // LuxRays handler
extern void (*SLG_DebugHandler)(const char *msg); // SLG handler

// Empty debug handler
extern void NullDebugHandler(const char *msg);

#define SLG_LOG(a) { if (slg::SLG_DebugHandler) { std::stringstream _SLG_LOG_LOCAL_SS; _SLG_LOG_LOCAL_SS << a; slg::SLG_DebugHandler(_SLG_LOG_LOCAL_SS.str().c_str()); } }

#if defined(WIN32) && !defined(__CYGWIN__)
inline float atanhf(float x) {
	// if outside of domain, return NaN
	// not 100% correct but should be good for now
	if(x <= -1.f || x >= 1.f)
		return sqrtf(-1.f);

	return logf((1.f + x) / (1.f - x)) / 2.f;
}
#endif


}

#endif	/* _LUXRAYS_H */
