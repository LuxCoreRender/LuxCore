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

#ifndef _LUXRAYS_UTILS_H
#define	_LUXRAYS_UTILS_H

#if defined(__linux__) || defined(__APPLE__)
#include <stddef.h>
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
        Unsupported Platform !!!
#endif

namespace luxrays {

inline double WallClockTime() {
#if defined(__linux__) || defined(__APPLE__)
	struct timeval t;
	gettimeofday(&t, NULL);

	return t.tv_sec + t.tv_usec / 1000000.0;
#elif defined (WIN32)
	return GetTickCount() / 1000.0;
#else
	Unsupported Platform !!!
#endif
}

template<class T> inline T Clamp(T val, T low, T high) {
	return val > low ? (val < high ? val : high) : low;
}

template<class T> inline T Max(T a, T b) {
	return a > b ? a : b;
}

template<class T> inline T Min(T a, T b) {
	return a < b ? a : b;
}

template<class T> inline void Swap(T &a, T &b) {
	const T tmp = a;
	a = b;
	b = tmp;
}

inline float Sgn(float a) {
	return a < 0.f ? -1.f : 1.f;
}

inline int Sgn(int a) {
	return a < 0 ? -1 : 1.f;
}

}

#endif	/* _LUXRAYS_UTILS_H */
