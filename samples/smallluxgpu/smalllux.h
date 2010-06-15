/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _SMALLLUX_H
#define	_SMALLLUX_H

#include <cmath>
#include <sstream>
#include <fstream>
#include <iostream>

#include <boost/interprocess/detail/atomic.hpp>

#if defined(__linux__) || defined(__APPLE__)
#include <stddef.h>
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
	Unsupported Platform !!!
#endif

#include "luxrays/luxrays.h"
#include "luxrays/core/utils.h"
#include "luxrays/utils/sdl/scene.h"
#include "luxrays/utils/film/film.h"

#include "slgcfg.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

inline void AtomicAdd(float *val, const float delta) {
	union bits {
		float f;
		boost::uint32_t i;

	};

	bits oldVal, newVal;

	do {
#if (defined(__i386__) || defined(__amd64__))
		__asm__ __volatile__("pause\n");
#endif

		oldVal.f = *val;
		newVal.f = oldVal.f + delta;
	} while (boost::interprocess::detail::atomic_cas32(((boost::uint32_t *)val), newVal.i, oldVal.i) != oldVal.i);
}

inline void AtomicAdd(unsigned int *val, const unsigned int delta) {
	boost::interprocess::detail::atomic_add32(((boost::uint32_t *)val), (boost::uint32_t)delta);
}

inline void AtomicInc(unsigned int *val) {
	boost::interprocess::detail::atomic_inc32(((boost::uint32_t *)val));
}

#endif	/* _SMALLLUX_H */
