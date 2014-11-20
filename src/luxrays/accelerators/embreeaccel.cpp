/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include <string>

#include <boost/foreach.hpp>

#include "luxrays/core/context.h"
#include "luxrays/accelerators/embreeaccel.h"

namespace luxrays {

void Embree_error_handler(const RTCError code, const char *str) {
	std::string errType;

	switch (code) {
		case RTC_UNKNOWN_ERROR:
			errType = "RTC_UNKNOWN_ERROR";
			break;
		case RTC_INVALID_ARGUMENT:
			errType = "RTC_INVALID_ARGUMENT";
			break;
		case RTC_INVALID_OPERATION:
			errType = "RTC_INVALID_OPERATION";
			break;
		case RTC_OUT_OF_MEMORY:
			errType = "RTC_OUT_OF_MEMORY";
			break;
		case RTC_UNSUPPORTED_CPU:
			errType = "RTC_UNSUPPORTED_CPU";
			break;
		default:
			errType = "invalid error code";
			break;
	}

	std::cout << "Embree error: " << str << "\n";

	abort();
}

boost::mutex EmbreeAccel::initMutex;
u_int EmbreeAccel::initCount = 0;

EmbreeAccel::EmbreeAccel(const Context *context) : ctx(context) {
	embreeScene = NULL;

	// Initialize Embree
	boost::unique_lock<boost::mutex> lock(initMutex);
	if (initCount == 0) {
		rtcInit(NULL);
		rtcSetErrorFunction(Embree_error_handler);
	}
	++initCount;
}

EmbreeAccel::~EmbreeAccel() {
	if (embreeScene)
		rtcDeleteScene(embreeScene);

	// Shutdown Embree if I was the last one
	boost::unique_lock<boost::mutex> lock(initMutex);
	if (initCount == 1)
		rtcExit();
	--initCount;
}

void EmbreeAccel::Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	const double t0 = WallClockTime();

	embreeScene = rtcNewScene(RTC_SCENE_STATIC, RTC_INTERSECT1);

	BOOST_FOREACH(const Mesh *mesh, meshes) {
		const u_int geomID = rtcNewTriangleMesh(embreeScene, RTC_GEOMETRY_STATIC,
				mesh->GetTotalTriangleCount(), mesh->GetTotalVertexCount(), 1);

		// Share the mesh vertices
		Point *meshVerts = mesh->GetVertices();
		rtcSetBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER, meshVerts, 0, 3 * sizeof(float));

		// Share the mesh triangles
		Triangle *meshTris = mesh->GetTriangles();
		rtcSetBuffer(embreeScene, geomID, RTC_INDEX_BUFFER, meshTris, 0, 3 * sizeof(int));
	}

	rtcCommit(embreeScene);
	
	LR_LOG(ctx, "EmbreeAccel build time: " << int((WallClockTime() - t0) * 1000) << "ms");
}

bool EmbreeAccel::Intersect(const Ray *ray, RayHit *hit) const {
	RTCRay embreeRay;

	embreeRay.org[0] = ray->o.x;
	embreeRay.org[1] = ray->o.y;
	embreeRay.org[2] = ray->o.z;

	embreeRay.dir[0] = ray->d.x;
	embreeRay.dir[1] = ray->d.y;
	embreeRay.dir[2] = ray->d.z;

	embreeRay.tnear = ray->mint;
	embreeRay.tfar = ray->maxt;

	embreeRay.geomID = RTC_INVALID_GEOMETRY_ID;
	embreeRay.primID = RTC_INVALID_GEOMETRY_ID;
	embreeRay.instID = RTC_INVALID_GEOMETRY_ID;
	embreeRay.mask = 0xFFFFFFFF;
	embreeRay.time = ray->time;

	rtcIntersect(embreeScene, embreeRay);

	if (embreeRay.geomID != RTC_INVALID_GEOMETRY_ID) {
		hit->meshIndex = embreeRay.geomID;
		hit->triangleIndex = embreeRay.primID;
		hit->t = embreeRay.tfar;

		hit->b1 = embreeRay.u;
		hit->b2 = embreeRay.v;

		return true;
	} else
		return false;
}

}
