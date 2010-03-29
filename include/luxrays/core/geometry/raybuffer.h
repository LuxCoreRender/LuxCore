/***************************************************************************
 *   Copyright (C) 1998-2009 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
 *                                                                         *
 *   Lux Renderer is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Lux Renderer is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   This project is based on PBRT ; see http://www.pbrt.org               *
 *   Lux Renderer website : http://www.luxrender.net                       *
 ***************************************************************************/

#ifndef _LUXRAYS_RAYBUFFER_H
#define _LUXRAYS_RAYBUFFER_H

#include <deque>
#include <vector>
#include <cmath>
#include <limits>

#include "luxrays/core/geometry/ray.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

namespace luxrays {


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
		return currentFreeRayIndex++;
	}

	size_t AddRay(const Ray &ray) {
		rays[currentFreeRayIndex] = ray;

		return currentFreeRayIndex++;
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

	virtual void PushToDo(RayBuffer *rayBuffer, const unsigned int index) = 0;
	virtual RayBuffer *PopToDo() = 0;
	// Pop up to 3 buffers out of a queue
	virtual void Pop3xToDo(RayBuffer **rayBuffer0, RayBuffer **rayBuffer1, RayBuffer **rayBuffer2) = 0;

	virtual void PushDone(RayBuffer *rayBuffer) = 0;
	virtual RayBuffer *PopDone(const unsigned int index = 0) = 0;
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

	void Pop3x(RayBuffer **rayBuffer0, RayBuffer **rayBuffer1, RayBuffer **rayBuffer2) {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		while (queue.size() < 1) {
			// Wait for a new buffer to arrive
			condition.wait(lock);
		}

		switch (queue.size()) {
			default:
			case 3:
				*rayBuffer0 = queue[0];
				*rayBuffer1 = queue[1];
				*rayBuffer2 = queue[2];
				queue.erase(queue.begin(), queue.begin() + 3);
				break;
			case 2:
				*rayBuffer0 = queue[0];
				*rayBuffer1 = queue[1];
				*rayBuffer2 = NULL;
				queue.erase(queue.begin(), queue.begin() + 2);
				break;
			case 1:
				*rayBuffer0 = queue[0];
				*rayBuffer1 = NULL;
				*rayBuffer2 = NULL;
				queue.pop_front();
				break;
		}
	}

	//--------------------------------------------------------------------------

	void Push(RayBuffer *rayBuffer, const unsigned int queueIndex) {
		{
			boost::unique_lock<boost::mutex> lock(queueMutex);
			rayBuffer->PushUserData(queueIndex);
			queue.push_back(rayBuffer);
		}

		condition.notify_all();
	}

	RayBuffer *Pop(const unsigned int queueIndex) {
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

	void Push(RayBuffer *rayBuffer, const unsigned int queueIndex, const unsigned int queueProgressive) {
		{
			boost::unique_lock<boost::mutex> lock(queueMutex);

			rayBuffer->PushUserData(queueProgressive);
			rayBuffer->PushUserData(queueIndex);
			queue.push_back(rayBuffer);
		}

		condition.notify_all();
	}

	RayBuffer *Pop(const unsigned int queueIndex, const unsigned int queueProgressive) {
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

	void PushToDo(RayBuffer *rayBuffer, const unsigned int queueIndex) { todoQueue.Push(rayBuffer); }
	RayBuffer *PopToDo() { return todoQueue.Pop(); }
	void Pop3xToDo(RayBuffer **rayBuffer0, RayBuffer **rayBuffer1, RayBuffer **rayBuffer2) {
		todoQueue.Pop3x(rayBuffer0, rayBuffer1, rayBuffer2);
	}

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const unsigned int queueIndex) { return doneQueue.Pop(); }

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

	void PushToDo(RayBuffer *rayBuffer, const unsigned int queueIndex) { todoQueue.Push(rayBuffer, queueIndex); }
	RayBuffer *PopToDo() { return todoQueue.Pop(); }
	void Pop3xToDo(RayBuffer **rayBuffer0, RayBuffer **rayBuffer1, RayBuffer **rayBuffer2) {
		todoQueue.Pop3x(rayBuffer0, rayBuffer1, rayBuffer2);
	}

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const unsigned int queueIndex) { return doneQueue.Pop(queueIndex); }

private:
	RayBufferSingleQueue todoQueue;
	RayBufferSingleQueue doneQueue;
};

// A many producers, many consumers queue
class RayBufferQueueM2M : public RayBufferQueue {
public:
	RayBufferQueueM2M(const unsigned int consumersCount) {
		queueToDoCounters.resize(consumersCount, 0);
		queueDoneCounters.resize(consumersCount, 0);
	}
	~RayBufferQueueM2M() { }

	void Clear() {
		todoQueue.Clear();
		doneQueue.Clear();
	}

	size_t GetSizeToDo() { return todoQueue.GetSize(); }
	size_t GetSizeDone() { return doneQueue.GetSize(); }

	void PushToDo(RayBuffer *rayBuffer, const unsigned int queueIndex) {
		todoQueue.Push(rayBuffer, queueIndex, queueToDoCounters[queueIndex]);
		queueToDoCounters[queueIndex]++;
	}
	RayBuffer *PopToDo() { return todoQueue.Pop(); }
	void Pop3xToDo(RayBuffer **rayBuffer0, RayBuffer **rayBuffer1, RayBuffer **rayBuffer2) {
		todoQueue.Pop3x(rayBuffer0, rayBuffer1, rayBuffer2);
	}

	void PushDone(RayBuffer *rayBuffer) { doneQueue.Push(rayBuffer); }
	RayBuffer *PopDone(const unsigned int queueIndex) {
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
