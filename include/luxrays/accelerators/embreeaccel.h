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

#ifndef _LUXRAYS_EMBREEACCEL_H
#define	_LUXRAYS_EMBREEACCEL_H

#include <boost/thread.hpp>

#include <embree3/rtcore.h>

#include "luxrays/luxrays.h"
#include "luxrays/core/accelerator.h"

namespace luxrays {

class EmbreeAccel : public Accelerator {
public:
	EmbreeAccel(const Context *context);
	virtual ~EmbreeAccel();

	virtual AcceleratorType GetType() const { return ACCEL_EMBREE; }

	virtual bool HasNativeSupport(const IntersectionDevice &device) const {
		return true;
	}
	virtual bool HasHWSupport(const IntersectionDevice &device) const {
		return false;
	}
	virtual HardwareIntersectionKernel *NewHardwareIntersectionKernel(HardwareIntersectionDevice &device) const {
		return nullptr;
	}

	virtual void Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount);

	virtual bool DoesSupportUpdate() const { return true; }
	virtual void Update();

	virtual bool Intersect(const Ray *ray, RayHit *hit) const;

private:
	static bool MeshPtrCompare(const Mesh *p0, const Mesh *p1);
	
	void ExportTriangleMesh(const RTCScene embreeScene, const Mesh *mesh) const;
	void ExportMotionTriangleMesh(const RTCScene embreeScene, const MotionTriangleMesh *mtm) const;

	// Used for Embree initialization
	static boost::mutex initMutex;
	// Used to count the number of existing EmbreeAccel instances
	static u_int initCount;

	const Context *ctx;

	RTCDevice embreeDevice;
	RTCScene embreeScene;
	std::map<const Mesh *, RTCScene, bool (*)(const Mesh *, const Mesh *)> uniqueRTCSceneByMesh;
	std::map<const Mesh *, RTCGeometry, bool (*)(const Mesh *, const Mesh *)> uniqueGeomByMesh;
	std::map<const Mesh *, Matrix4x4, bool (*)(const Mesh *, const Mesh *)> uniqueInstMatrixByMesh;
	// Used to normalize between 0.f and 1.f
	float minTime, maxTime, timeScale;
};

}

#endif
