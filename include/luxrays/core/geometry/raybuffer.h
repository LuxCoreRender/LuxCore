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

#ifndef _LUXRAYS_RAYBUFFER_H
#define _LUXRAYS_RAYBUFFER_H

#include <deque>
#include <vector>
#include <cmath>
#include <limits>
#include <algorithm>

#include "luxrays/core/geometry/ray.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace luxrays {


#define RAYBUFFER_DEFAULT_SIZE 65536

class RayBuffer {
public:
	RayBuffer(const size_t bufferSize) : size(bufferSize), currentFreeRayIndex(0) {
		rays = new Ray[size];
		rayHits = new RayHit[size];
	}

	~RayBuffer() {
		delete[] rays;
		delete[] rayHits;
	}

	void PushUserData(size_t data) {
		userData.push_back(data);
	}

	size_t GetUserData() {
		return userData.back();
	}

	size_t GetUserData(const size_t index) {
		assert (index < userData.size());

		return userData[userData.size() - index - 1];
	}

	size_t PopUserData() {
		size_t data = userData.back();
		userData.pop_back();

		return data;
	}

	size_t GetUserDataCount() {
		return userData.size();
	}

	void ResetUserData() {
		userData.clear();
	}

	void Reset() {
		currentFreeRayIndex = 0;
	}

	size_t ReserveRay() {
		assert (currentFreeRayIndex < size);
		return currentFreeRayIndex++;
	}

	size_t AddRay(const Ray &ray) {
		assert (currentFreeRayIndex < size);
		rays[currentFreeRayIndex] = ray;

		return currentFreeRayIndex++;
	}
	
	size_t AddRays(const Ray *ray, const size_t raysCount) {
		assert ((raysCount == 0) || (currentFreeRayIndex + raysCount - 1 < size));

		memcpy(&rays[currentFreeRayIndex], ray, sizeof(Ray) * raysCount);

		const size_t firstIndex = currentFreeRayIndex;
		currentFreeRayIndex += raysCount;

		return firstIndex;
	}

	const Ray *GetRay(const size_t index) const {
		return &rays[index];
	}

	const RayHit *GetRayHit(const size_t index) const {
		return &rayHits[index];
	}

	size_t GetSize() const {
		return size;
	}

	size_t GetRayCount() const {
		return currentFreeRayIndex;
	}

	bool IsFull() const {
		return (currentFreeRayIndex >= size);
	}

	size_t LeftSpace() const {
		return size - currentFreeRayIndex;
	}

	Ray *GetRayBuffer() const {
		return rays;
	}

	RayHit *GetHitBuffer() {
		return rayHits;
	}

private:
	size_t size;
	size_t currentFreeRayIndex;
	std::vector<size_t> userData;

	Ray *rays;
	RayHit *rayHits;
};

// NOTE: this class must be thread safe
class RayBufferQueue {
public:
	virtual ~RayBufferQueue() { }

	virtual void Clear() = 0;
	virtual size_t GetSizeToDo() = 0;
	virtual size_t GetSizeDone() = 0;

	virtual void PushToDo(RayBuffer *rayBuffer, const size_t index) = 0;
	virtual RayBuffer *PopToDo() = 0;

	virtual void PushDone(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopDone(const size_t index = 0) = 0;
};

class RayBufferSingleQueue {
public:
	RayBufferSingleQueue() {
	}

	~RayBufferSingleQueue() {
	}

	void Clear() {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		queue.clear();
	}

	size_t GetSize() {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		return queue.size();
	}

	//--------------------------------------------------------------------------

	void Push(RayBuffer *rayBuffer) {
		{
			boost::unique_lock<boost::mutex> lock(queueMutex);
			queue.push_back(rayBuffer);
		}

		condition.notify_all();
	}

	RayBuffer *Pop() {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		while (queue.size() < 1) {
			// Wait for a new buffer to arrive
			condition.wait(lock);
		}

		RayBuffer *rayBuffer = queue.front();
		queue.pop_front();
		return rayBuffer;
	}

	//--------------------------------------------------------------------------

	void Push(RayBuffer *rayBuffer, const size_t queueIndex) {
		{
			boost::unique_lock<boost::mutex> lock(queueMutex);
			rayBuffer->PushUserData(queueIndex);
			queue.push_back(rayBuffer);
		}

		condition.notify_all();
	}

	RayBuffer *Pop(const size_t queueIndex) {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		for (;;) {
			for (size_t i = 0; i < queue.size(); ++i) {
				// Check if it matches the requested queueIndex
				if (queue[i]->GetUserData() == queueIndex) {
					RayBuffer *rayBuffer = queue[i];
					queue.erase(queue.begin() + i);
					rayBuffer->PopUserData();

					return rayBuffer;
				}
			}

			// Wait for a new buffer to arrive
			condition.wait(lock);
		}
	}

	//--------------------------------------------------------------------------

	void Push(RayBuffer *rayBuffer, const size_t queueIndex, const size_t queueProgressive) {
		{
			boost::unique_lock<boost::mutex> lock(queueMutex);

			rayBuffer->PushUserData(queueProgressive);
			rayBuffer->PushUserData(queueIndex);
			queue.push_back(rayBuffer);
		}

		condition.notify_all();
	}

	RayBuffer *Pop(const size_t queueIndex, const size_t queueProgressive) {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		for (;;) {
			for (size_t i = 0; i < queue.size(); ++i) {
				// Check if it matches the requested queueIndex and queueProgressive
				if ((queue[i]->GetUserData(0) == queueIndex) &&
					(queue[i]->GetUserData(1) == queueProgressive)) {
					RayBuffer *rayBuffer = queue[i];
					queue.erase(queue.begin() + i);
					rayBuffer->PopUserData();
					rayBuffer->PopUserData();

					return rayBuffer;
				}
			}

			// Wait for a new buffer to arrive
			condition.wait(lock);
		}
	}

private:
	boost::mutex queueMutex;
	boost::condition_variable condition;

	std::deque<RayBuffer *> queue;
};

// A one producer, one consumer queue
class RayBufferQueueO2O : public RayBufferQueue {
public:
	RayBufferQueueO2O() { }
	~RayBufferQueueO2O() { }

	void Clear() {
		todoQueue.Clear();
		doneQueue.Clear();
	}

	size_t GetSizeToDo() { return todoQueue.GetSize(); }
	size_t GetSizeDone() { return doneQueue.GetSize(); }

	void PushToDo(RayBuffer *rayBuffer, const size_t queueIndex) { todoQueue.Push(rayBuffer); }
	RayBuffer *PopToDo() { return todoQueue.Pop(); }

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const size_t queueIndex) { return doneQueue.Pop(); }

private:
	RayBufferSingleQueue todoQueue;
	RayBufferSingleQueue doneQueue;
};

// A many producers, one consumer queue
class RayBufferQueueM2O : public RayBufferQueue {
public:
	RayBufferQueueM2O() { }
	~RayBufferQueueM2O() { }

	void Clear() {
		todoQueue.Clear();
		doneQueue.Clear();
	}

	size_t GetSizeToDo() { return todoQueue.GetSize(); }
	size_t GetSizeDone() { return doneQueue.GetSize(); }

	void PushToDo(RayBuffer *rayBuffer, const size_t queueIndex) { todoQueue.Push(rayBuffer, queueIndex); }
	RayBuffer *PopToDo() { return todoQueue.Pop(); }

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const size_t queueIndex) { return doneQueue.Pop(queueIndex); }

private:
	RayBufferSingleQueue todoQueue;
	RayBufferSingleQueue doneQueue;
};

// A many producers, many consumers queue
class RayBufferQueueM2M : public RayBufferQueue {
public:
	RayBufferQueueM2M(const size_t consumersCount) {
		queueToDoCounters.resize(consumersCount);
		std::fill(queueToDoCounters.begin(), queueToDoCounters.end(), 0);
		queueDoneCounters.resize(consumersCount, 0);
		std::fill(queueDoneCounters.begin(), queueDoneCounters.end(), 0);
	}
	~RayBufferQueueM2M() { }

	void Clear() {
		todoQueue.Clear();
		doneQueue.Clear();
	}

	size_t GetSizeToDo() { return todoQueue.GetSize(); }
	size_t GetSizeDone() { return doneQueue.GetSize(); }

	void PushToDo(RayBuffer *rayBuffer, const size_t queueIndex) {
		todoQueue.Push(rayBuffer, queueIndex, queueToDoCounters[queueIndex]);
		queueToDoCounters[queueIndex]++;
	}
	RayBuffer *PopToDo() { return todoQueue.Pop(); }

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const size_t queueIndex) {
		RayBuffer *rb = doneQueue.Pop(queueIndex, queueDoneCounters[queueIndex]);
		queueDoneCounters[queueIndex]++;

		return rb;
	}

private:
	std::vector<unsigned int> queueToDoCounters;
	std::vector<unsigned int> queueDoneCounters;
	RayBufferSingleQueue todoQueue;
	RayBufferSingleQueue doneQueue;
};

}

#endif	/* _LUXRAYS_RAYBUFFER_H */
