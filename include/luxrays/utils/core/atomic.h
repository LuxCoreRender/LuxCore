/***************************************************************************
 *   Copyright (C) 1998-2009 by David Bucciarelli (davibu@interfree.it)    *
 *                                                                         *
 *   This file is part of SmallLuxGPU.                                     *
 *                                                                         *
 *   SmallLuxGPU is free software; you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *  SmallLuxGPU is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   and Lux Renderer website : http://www.luxrender.net                   *
 ***************************************************************************/

#ifndef _LUXRAYS_ATOMIC_H
#define _LUXRAYS_ATOMIC_H

#include <boost/version.hpp>
#include <boost/interprocess/detail/atomic.hpp>

namespace luxrays {

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
	} while (
#if (BOOST_VERSION < 104800)
		boost::interprocess::detail::atomic_cas32(
#else
		boost::interprocess::ipcdetail::atomic_cas32(
#endif
			((boost::uint32_t *)val), newVal.i, oldVal.i) != oldVal.i);
}

inline void AtomicAdd(unsigned int *val, const unsigned int delta) {
#if defined(WIN32)
   boost::uint32_t newVal;
   do
   {
      #if (defined(__i386__) || defined(__amd64__))
         __asm__ __volatile__("pause\n");
      #endif
      newVal = *val + delta;
   } while (
#if (BOOST_VERSION < 104800)
		boost::interprocess::detail::atomic_cas32(
#else
		boost::interprocess::ipcdetail::atomic_cas32(
#endif
		   ((boost::uint32_t*)val), newVal, *val) != *val);
#else

#if (BOOST_VERSION < 104800)
	boost::interprocess::detail::atomic_add32(((boost::uint32_t *)val), (boost::uint32_t)delta);
#else
	boost::interprocess::ipcdetail::atomic_add32(((boost::uint32_t *)val), (boost::uint32_t)delta);
#endif

#endif
}

inline void AtomicInc(unsigned int *val) {
#if (BOOST_VERSION < 104800)
	boost::interprocess::detail::atomic_inc32(((boost::uint32_t *)val));
#else
	boost::interprocess::ipcdetail::atomic_inc32(((boost::uint32_t *)val));
#endif
}

}

#endif	/* _LUXRAYS_ATOMIC_H */
