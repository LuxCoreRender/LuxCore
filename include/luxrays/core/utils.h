/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_UTILS_H
#define	_LUXRAYS_UTILS_H

#include <cmath>

#if defined (__linux__)
#include <pthread.h>
#endif

#include <boost/thread.hpp>

#if (defined(WIN32) && defined(_MSC_VER) && _MSC_VER < 1800)
#include <float.h>
#define isnan(a) _isnan(a)
#define isinf(f) (!_finite((f)))
#else
#define isnan(a) std::isnan(a)
#define isinf(f) std::isinf(f)
#endif

#if defined(WIN32)
#define isnanf(a) _isnan(a)
typedef unsigned int u_int;
#endif

#if defined(__APPLE__)
#include <string>
typedef unsigned int u_int;
#endif

#if !defined(__APPLE__) && !defined(__OpenBSD__) && !defined(__FreeBSD__)
#  include <malloc.h> // for _alloca, memalign
#  if !defined(WIN32) || defined(__CYGWIN__)
#    include <alloca.h>
#  else
#    define memalign(a,b) _aligned_malloc(b, a)
#    define alloca _alloca
#  endif
#else
#  include <stdlib.h>
#  if defined(__APPLE__)
#    define memalign(a,b) valloc(b)
#  elif defined(__OpenBSD__) || defined(__FreeBSD__)
#    define memalign(a,b) malloc(b)
#  endif
#endif

#include <sstream>

#if defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__FreeBSD__)
#include <stddef.h>
#include <sys/time.h>
#elif defined (WIN32)
#include <windows.h>
#else
#error "Unsupported Platform !!!"
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#ifndef INFINITY
#define INFINITY (std::numeric_limits<float>::infinity())
#endif

#ifndef INV_PI
#define INV_PI  0.31830988618379067154f
#endif

#ifndef INV_TWOPI
#define INV_TWOPI  0.15915494309189533577f
#endif

#include "luxrays/core/geometry/matrix4x4.h"

namespace luxrays {

inline double WallClockTime() {
#if defined(__linux__) || defined(__APPLE__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	struct timeval t;
	gettimeofday(&t, NULL);

	return t.tv_sec + t.tv_usec / 1000000.0;
#elif defined (WIN32)
	return GetTickCount() / 1000.0;
#else
#error "Unsupported Platform !!!"
#endif
}

template<class T> inline T Lerp(float t, T v1, T v2) {
	// Linear interpolation
	return v1 + t * (v2 - v1);
}

template<class T> inline T Cerp(float t, T v0, T v1, T v2, T v3) {
	// Cubic interpolation
	return v1 + .5f *
			t * (v2 - v0 +
				t * (2.f * v0 - 5.f * v1 + 4.f * v2 - v3 +
					t * (3.f * (v1 - v2) + v3 - v0)));
}

template<class T> inline T Clamp(T val, T low, T high) {
	return val > low ? (val < high ? val : high) : low;
}

template<class T> inline void Swap(T &a, T &b) {
	const T tmp = a;
	a = b;
	b = tmp;
}

template<class T> inline T Max(T a, T b) {
	return a > b ? a : b;
}

template<class T> inline T Min(T a, T b) {
	return a < b ? a : b;
}

inline float Sgn(float a) {
	return a < 0.f ? -1.f : 1.f;
}

inline int Sgn(int a) {
	return a < 0 ? -1 : 1;
}

template<class T> inline T Sqr(T a) {
	return a * a;
}

inline int Round2Int(double val) {
	return static_cast<int>(val > 0. ? val + .5 : val - .5);
}

inline int Round2Int(float val) {
	return static_cast<int>(val > 0.f ? val + .5f : val - .5f);
}

inline u_int Round2UInt(double val) {
	return static_cast<u_int>(val > 0. ? val + .5 : 0.);
}

inline u_int Round2UInt(float val) {
	return static_cast<u_int>(val > 0.f ? val + .5f : 0.f);
}

template<class T> inline T Mod(T a, T b) {
	if (b == 0)
		b = 1;

	a %= b;
	if (a < 0)
		a += b;

	return a;
}

inline float Radians(float deg) {
	return (M_PI / 180.f) * deg;
}

inline float Degrees(float rad) {
	return (180.f / M_PI) * rad;
}

inline float Log2(float x) {
	return logf(x) / logf(2.f);
}

inline int Log2Int(float v) {
	return Round2Int(Log2(v));
}

inline bool IsPowerOf2(int v) {
	return (v & (v - 1)) == 0;
}

inline bool IsPowerOf2(u_int v) {
	return (v & (v - 1)) == 0;
}

template <class T> inline T RoundUp(const T a, const T b) {
	const unsigned int r = a % b;
	if (r == 0)
		return a;
	else
		return a + b - r;
}

template <class T> inline T RoundUpPow2(T v) {
	v--;

	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;

	return v+1;
}

inline u_int RoundUpPow2(u_int v) {
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	return v+1;
}

template<class T> inline int Float2Int(T val) {
	return static_cast<int>(val);
}

template<class T> inline u_int Float2UInt(T val) {
	return val >= 0 ? static_cast<u_int>(val) : 0;
}

inline int Floor2Int(double val) {
	return static_cast<int>(floor(val));
}

inline int Floor2Int(float val) {
	return static_cast<int>(floorf(val));
}

inline u_int Floor2UInt(double val) {
	return val > 0. ? static_cast<u_int>(floor(val)) : 0;
}

inline u_int Floor2UInt(float val) {
	return val > 0.f ? static_cast<u_int>(floorf(val)) : 0;
}

inline int Ceil2Int(double val) {
	return static_cast<int>(ceil(val));
}

inline int Ceil2Int(float val) {
	return static_cast<int>(ceilf(val));
}

inline u_int Ceil2UInt(double val) {
	return val > 0. ? static_cast<u_int>(ceil(val)) : 0;
}

inline u_int Ceil2UInt(float val) {
	return val > 0.f ? static_cast<u_int>(ceilf(val)) : 0;
}

inline bool Quadratic(float A, float B, float C, float *t0, float *t1) {
	// Find quadratic discriminant
	float discrim = B * B - 4.f * A * C;
	if (discrim < 0.f)
		return false;
	float rootDiscrim = sqrtf(discrim);
	// Compute quadratic _t_ values
	float q;
	if (B < 0)
		q = -.5f * (B - rootDiscrim);
	else
		q = -.5f * (B + rootDiscrim);
	*t0 = q / A;
	*t1 = C / q;
	if (*t0 > *t1)
		Swap(*t0, *t1);
	return true;
}

inline float SmoothStep(float min, float max, float value) {
	float v = Clamp((value - min) / (max - min), 0.f, 1.f);
	return v * v * (-2.f * v  + 3.f);
}

template <class T> int SignOf(T x)
{
	return (x > 0) - (x < 0);
}

template <class T> inline std::string ToString(const T &t) {
	std::ostringstream ss;
	ss << t;
	return ss.str();
}

inline std::string ToString(const float t) {
	std::ostringstream ss;
	ss << std::setprecision(std::numeric_limits<float>::digits10 + 1) << t;
	return ss.str();
}

inline std::string ToString(const Matrix4x4 &m) {
	std::ostringstream ss;
	ss << std::setprecision(std::numeric_limits<float>::digits10 + 1);

	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			if ((i != 0) || (j != 0))
				ss << " ";
			ss << m.m[j][i];
		}
	}
	return ss.str();
}

inline unsigned int UIntLog2(unsigned int value) {
	unsigned int l = 0;
	while (value >>= 1) l++;
	return l;
}

inline bool SetThreadRRPriority(boost::thread *thread, int pri = 0) {
#if defined (__linux__) || defined (__APPLE__) || defined(__CYGWIN__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	{
		const pthread_t tid = (pthread_t)thread->native_handle();

		int policy = SCHED_FIFO;
		int sysMinPriority = sched_get_priority_min(policy);
		struct sched_param param;
		param.sched_priority = sysMinPriority + pri;

		return pthread_setschedparam(tid, policy, &param);
	}
#elif defined (WIN32)
	{
		const HANDLE tid = (HANDLE)thread->native_handle();
		if (!SetPriorityClass(tid, HIGH_PRIORITY_CLASS))
			return false;
		else
			return true;

		/*if (!SetThreadPriority(tid, THREAD_PRIORITY_HIGHEST))
			return false;
		else
			return true;*/
	}
#endif
}

}

#endif	/* _LUXRAYS_UTILS_H */
