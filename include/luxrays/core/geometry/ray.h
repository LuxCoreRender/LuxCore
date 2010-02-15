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

#ifndef _LUXRAYS_RAY_H
#define _LUXRAYS_RAY_H

#include <deque>
#include <cmath>
#include <limits>

#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/point.h"

#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

#define RAY_EPSILON 1e-4f

namespace luxrays {

class  Ray {
public:
	// Ray Public Methods
	Ray() : maxt(std::numeric_limits<float>::infinity()) {
		mint = RAY_EPSILON;
	}

	Ray(const Point &origin, const Vector &direction)
		: o(origin), d(direction), maxt(std::numeric_limits<float>::infinity()) {
		mint = RAY_EPSILON;
	}

	Ray(const Point &origin, const Vector &direction,
		float start, float end = std::numeric_limits<float>::infinity())
		: o(origin), d(direction), mint(start), maxt(end) { }

	Point operator()(float t) const { return o + d * t; }
	void GetDirectionSigns(int signs[3]) const {
		signs[0] = d.x < 0.f;
		signs[1] = d.y < 0.f;
		signs[2] = d.z < 0.f;
	}

	// Ray Public Data
	Point o;
	Vector d;
	mutable float mint, maxt;
};

inline std::ostream &operator<<(std::ostream &os, Ray &r) {
	os << "org: " << r.o << "dir: " << r.d << " range [" <<
		r.mint << "," << r.maxt << "]";
	return os;
}

class RayHit {
public:
	float t;
	float b1, b2; // Barycentric coordinates of the hit point
	unsigned int index;

	bool Miss() const { return (index == 0xffffffffu); };
};

class RayBuffer {
public:
	RayBuffer(const size_t bufferSize) : size(bufferSize), currentFreeRayIndex(0) {
		rays = new Ray[size];
		rayHits = new RayHit[size];
		userData = 0;
	}

	~RayBuffer() {
		delete rays;
		delete rayHits;
	}

	void SetUserData(size_t data) { userData = data; }
	size_t GetUserData() const { return userData; }

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

	const RayHit *GetRayHit(const size_t index) const {
		assert (index >= 0);
		assert (index < size);

		return &rayHits[index];
	}

	size_t GetSize() const {
		return size;
	}

	size_t GetRayCount() const {
		return currentFreeRayIndex;
	}

	bool IsFull() { return (currentFreeRayIndex >= size); }
	size_t LeftSpace() { return size - currentFreeRayIndex; }

	Ray *GetRayBuffer() const {
		return rays;
	}

	RayHit *GetHitBuffer() {
		return rayHits;
	}

private:
	size_t size;
	size_t currentFreeRayIndex;
	size_t userData;

	Ray *rays;
	RayHit *rayHits;
};

// NOTE: this class must be thread safe
class RayBufferQueue {
public:
	RayBufferQueue() { }
	~RayBufferQueue() { }

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

	void Clear() {
		boost::unique_lock<boost::mutex> lock(queueMutex);

		queue.clear();
	}

	size_t Size() {
		boost::unique_lock<boost::mutex> lock(queueMutex);
		return queue.size();
	}

private:
	boost::mutex queueMutex;
	boost::condition_variable condition;

	std::deque<RayBuffer *> queue;
};

}

#endif	/* _LUXRAYS_RAY_H */
