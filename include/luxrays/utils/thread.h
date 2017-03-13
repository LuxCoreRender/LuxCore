/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_THREAD_H
#define	_LUXRAYS_THREAD_H

#include <boost/thread.hpp>

#include "luxrays/utils/utils.h"

namespace luxrays {

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

#endif	/* _LUXRAYS_THREAD_H */
