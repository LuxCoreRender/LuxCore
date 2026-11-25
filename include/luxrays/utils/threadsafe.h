/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_THREADSAFE_H
#define _LUXRAYS_THREADSAFE_H


namespace luxrays {

template <class T> class ThreadSafeQueue {
public:
	ThreadSafeQueue() {
	}

	virtual ~ThreadSafeQueue() {
	}

	void Clear() {
		std::unique_lock<std::mutex> lock(queueMutex);

		itemQueue.clear();
	}
	
	size_t GetSize() {
		std::unique_lock<std::mutex> lock(queueMutex);

		return itemQueue.size();
	}

	void Push(T item) {
		{
			std::unique_lock<std::mutex> lock(queueMutex);
			itemQueue.push_back(item);
		}

		queueCondition.notify_all();
	}

	T Pop() {
		std::unique_lock<std::mutex> lock(queueMutex);

		while (itemQueue.size() < 1) {
			// Wait for a new buffer to arrive
			queueCondition.wait(lock);
		}

		T item = itemQueue.front();
		itemQueue.pop_front();

		return item;
	}

private:
	std::mutex queueMutex;
	std::condition_variable queueCondition;
	std::deque<T> itemQueue;
};

}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
