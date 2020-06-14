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

#include "luxrays/utils/thread.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// Thread related, utility functions
//------------------------------------------------------------------------------

size_t luxrays::GetHardwareThreadCount() {
	// For Windows version greater than Windows 7,modern way of calculating processor count is used 
	// May not work with Windows version prior to Windows 7
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(WIN64)
	return (size_t)GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
#else 
	return (size_t)boost::thread::hardware_concurrency();
#endif
}

void luxrays::SetThreadGroupAffinity(const size_t threadIndex) {
	// Set thread affinity the modern way.May not work for Windows version prior to Windows7
#if defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined (WIN64)
	auto totalProcessors = 0U;
	size_t processorIndex = threadIndex % GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);

	// Determine which processor group to bind the thread to.
	for (auto i = 0U; i < GetActiveProcessorGroupCount(); ++i) {
		totalProcessors += GetActiveProcessorCount(i);
		if (totalProcessors >= processorIndex) {
			auto mask = (1ULL << GetActiveProcessorCount(i)) - 1;
			GROUP_AFFINITY groupAffinity = { mask, static_cast<WORD>(i), { 0, 0, 0 } };
			SetThreadGroupAffinity(GetCurrentThread(), &groupAffinity, nullptr);
			break;
		}
	}
#endif
}

bool luxrays::SetThreadRRPriority(boost::thread *thread, int pri) {
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
