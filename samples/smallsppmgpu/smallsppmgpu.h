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

#ifndef _SMALLSPPMGPU_H
#define	_SMALLSPPMGPU_H

#include <boost/thread.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/interprocess/detail/atomic.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/pixel/framebuffer.h"
#include "luxrays/utils/film/film.h"

#include "hitpoints.h"

extern std::string SPPMG_LABEL;

extern luxrays::Context *ctx;
extern luxrays::sdl::Scene *scene;

extern luxrays::utils::Film *film;
extern luxrays::SampleBuffer *sampleBuffer;
extern unsigned int imgWidth;
extern unsigned int imgHeight;
extern std::string imgFileName;

extern unsigned int stochasticInterval;

extern double startTime;
extern unsigned int scrRefreshInterval;

extern std::vector<boost::thread *> renderThreads;
extern boost::barrier *barrierStop;
extern boost::barrier *barrierStart;

extern unsigned long long photonTracedTotal;
extern unsigned int photonTracedPass;
extern HitPoints *hitPoints;

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
	boost::interprocess::detail::atomic_add32(val, delta);
}

inline void AtomicInc(unsigned int *val) {
	boost::interprocess::detail::atomic_inc32(val);
}

#endif	/* _SMALLSPPMGPU_H */
