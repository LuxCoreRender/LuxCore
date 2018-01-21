/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_ATOMIC_H
#define _LUXRAYS_ATOMIC_H

#include <boost/version.hpp>
#include <boost/interprocess/detail/atomic.hpp>
#include <boost/atomic.hpp>

namespace luxrays {

//------------------------------------------------------------------------------
// Atomics
//------------------------------------------------------------------------------

inline void AtomicAdd(float *val, const float delta) {
	union bits {
		float f;
		uint32_t i;

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
			((uint32_t *)val), newVal.i, oldVal.i) != oldVal.i);
		}

inline void AtomicAdd(unsigned int *val, const unsigned int delta) {
#if defined(WIN32)
   uint32_t newVal;
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
		   ((uint32_t*)val), newVal, *val) != *val);
#else

#if (BOOST_VERSION < 104800)
	boost::interprocess::detail::atomic_add32(((uint32_t *)val), (uint32_t)delta);
#else
	boost::interprocess::ipcdetail::atomic_add32(((uint32_t *)val), (uint32_t)delta);
#endif

#endif
	}

inline void AtomicInc(unsigned int *val) {
#if (BOOST_VERSION < 104800)
	boost::interprocess::detail::atomic_inc32(((uint32_t *)val));
#else
	boost::interprocess::ipcdetail::atomic_inc32(((uint32_t *)val));
#endif
	}

inline void AtomicDec(unsigned int *val) {
#if (BOOST_VERSION < 104800)
	boost::interprocess::detail::atomic_dec32(((uint32_t *)val));
#else
	boost::interprocess::ipcdetail::atomic_dec32(((uint32_t *)val));
#endif
}

//------------------------------------------------------------------------------
// SpinLock
//------------------------------------------------------------------------------

typedef boost::atomic<bool> SpinLock;

class SpinLocker {
public:
	SpinLocker(SpinLock &lock) : spinLock(lock) {
		while (spinLock.exchange(false, boost::memory_order_acquire) == true) {
			// busy-wait
		}
	}

	~SpinLocker() {
		spinLock.store(true, boost::memory_order_release);
	}

private:
	boost::atomic<bool> &spinLock;
};

}

#endif	/* _LUXRAYS_ATOMIC_H */
