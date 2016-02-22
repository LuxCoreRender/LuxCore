/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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
#include <limits>

#if defined (_MSC_VER) || defined (__INTEL_COMPILER)
#include <intrin.h>
#endif

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

EmbreeAccel::EmbreeAccel(const Context *context) : ctx(context),
		uniqueRTCSceneByMesh(MeshPtrCompare), uniqueInstIDByMesh(MeshPtrCompare),
		uniqueInstMatrixByMesh(MeshPtrCompare) {
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
	if (embreeScene) {
		rtcDeleteScene(embreeScene);

		// I have to free all Embree scenes used for instances
		std::pair<const Mesh *, RTCScene> elem;
		BOOST_FOREACH(elem, uniqueRTCSceneByMesh)
			rtcDeleteScene(elem.second);
	}

	// Shutdown Embree if I was the last one
	boost::unique_lock<boost::mutex> lock(initMutex);
	if (initCount == 1)
		rtcExit();
	--initCount;
}

u_int EmbreeAccel::ExportTriangleMesh(const RTCScene embreeScene, const Mesh *mesh) const {
	const u_int geomID = rtcNewTriangleMesh(embreeScene, RTC_GEOMETRY_STATIC,
			mesh->GetTotalTriangleCount(), mesh->GetTotalVertexCount(), 1);

	// Share with Embree the mesh vertices
	Point *meshVerts = mesh->GetVertices();
	rtcSetBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER, meshVerts, 0, 3 * sizeof(float));

	// Share with Embree the mesh triangles
	Triangle *meshTris = mesh->GetTriangles();
	rtcSetBuffer(embreeScene, geomID, RTC_INDEX_BUFFER, meshTris, 0, 3 * sizeof(int));
	
	return geomID;
}

u_int EmbreeAccel::ExportMotionTriangleMesh(const RTCScene embreeScene, const MotionTriangleMesh *mtm) const {
	const u_int geomID = rtcNewTriangleMesh(embreeScene, RTC_GEOMETRY_STATIC,
			mtm->GetTotalTriangleCount(), mtm->GetTotalVertexCount(), 2);

	const MotionSystem &ms = mtm->GetMotionSystem();

	// Copy the mesh start position vertices
	float *vertices0 = (float *)rtcMapBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER0);
	for (u_int i = 0; i < mtm->GetTotalVertexCount(); ++i) {
		const Point v = mtm->GetVertex(ms.StartTime(), i);
		*vertices0++ = v.x;
		*vertices0++ = v.y;
		*vertices0++ = v.z;
		++vertices0;
	}
	rtcUnmapBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER0);

	// Copy the mesh end position vertices
	float *vertices1 = (float *)rtcMapBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER1);
	for (u_int i = 0; i < mtm->GetTotalVertexCount(); ++i) {
		const Point v = mtm->GetVertex(ms.EndTime(), i);
		*vertices1++ = v.x;
		*vertices1++ = v.y;
		*vertices1++ = v.z;
		++vertices1;
	}
	rtcUnmapBuffer(embreeScene, geomID, RTC_VERTEX_BUFFER1);

	// Share the mesh triangles
	Triangle *meshTris = mtm->GetTriangles();
	rtcSetBuffer(embreeScene, geomID, RTC_INDEX_BUFFER, meshTris, 0, 3 * sizeof(int));
	
	return geomID;
}

void EmbreeAccel::Init(const std::deque<const Mesh *> &meshes,
		const u_longlong totalVertexCount,
		const u_longlong totalTriangleCount) {
	const double t0 = WallClockTime();

	//--------------------------------------------------------------------------
	// Extract the meshes min. and max. time. To normalize between 0.f and 1.f.
	//--------------------------------------------------------------------------

	minTime = std::numeric_limits<float>::max();
	maxTime = std::numeric_limits<float>::min();
	BOOST_FOREACH(const Mesh *mesh, meshes) {
		const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(mesh);
		
		if (mtm) {
			minTime = Min(minTime, mtm->GetMotionSystem().StartTime());
			maxTime = Min(maxTime, mtm->GetMotionSystem().EndTime());
		}
	}

	if ((minTime == std::numeric_limits<float>::max()) ||
			(maxTime == std::numeric_limits<float>::min())) {
		minTime = 0.f;
		maxTime = 1.f;
		timeScale = 1.f;
	} else
		timeScale = 1.f / (maxTime - minTime);

	//--------------------------------------------------------------------------
	// Convert the meshes to an Embree Scene
	//--------------------------------------------------------------------------

	embreeScene = rtcNewScene(RTC_SCENE_DYNAMIC, RTC_INTERSECT1);

	BOOST_FOREACH(const Mesh *mesh, meshes) {
		switch (mesh->GetType()) {
			case TYPE_TRIANGLE:
			case TYPE_EXT_TRIANGLE:
				ExportTriangleMesh(embreeScene, mesh);
				break;
			case TYPE_TRIANGLE_INSTANCE:
			case TYPE_EXT_TRIANGLE_INSTANCE: {
				const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(mesh);

				// Check if a RTCScene has already been created
				std::map<const Mesh *, RTCScene, bool (*)(const Mesh *, const Mesh *)>::iterator it =
						uniqueRTCSceneByMesh.find(itm->GetTriangleMesh());

				RTCScene instScene;
				if (it == uniqueRTCSceneByMesh.end()) {
					TriangleMesh *instancedMesh = itm->GetTriangleMesh();

					// Create a new RTCScene
					instScene = rtcNewScene(RTC_SCENE_STATIC, RTC_INTERSECT1);
					ExportTriangleMesh(instScene, instancedMesh);
					rtcCommit(instScene);

					uniqueRTCSceneByMesh[instancedMesh] = instScene;
				} else
					instScene = it->second;

				const u_int instID = rtcNewInstance(embreeScene, instScene);
				rtcSetTransform(embreeScene, instID, RTC_MATRIX_ROW_MAJOR, &(itm->GetTransformation().m.m[0][0]));

				// Save the instance ID
				uniqueInstIDByMesh[mesh] = instID;
				// Save the matrix
				uniqueInstMatrixByMesh[mesh] = itm->GetTransformation().m;
				break;
			}
			case TYPE_TRIANGLE_MOTION:
			case TYPE_EXT_TRIANGLE_MOTION: {
				const MotionTriangleMesh *mtm = dynamic_cast<const MotionTriangleMesh *>(mesh);
				ExportMotionTriangleMesh(embreeScene, mtm);
				break;
			}
			default:
				throw std::runtime_error("Unknown Mesh type in EmbreeAccel::Init(): " + ToString(mesh->GetType()));
		}
	}

	rtcCommit(embreeScene);

	LR_LOG(ctx, "EmbreeAccel build time: " << int((WallClockTime() - t0) * 1000) << "ms");
}

void EmbreeAccel::Update() {
	// Update all Embree scenes used for instances
	bool updated = false;
	std::pair<const Mesh *, u_int> elem;
	BOOST_FOREACH(elem, uniqueInstIDByMesh) {
		const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(elem.first);

		// Check if the transformation has changed
		if (uniqueInstMatrixByMesh[elem.first] != itm->GetTransformation().m) {
			rtcSetTransform(embreeScene, elem.second, RTC_MATRIX_ROW_MAJOR, &(itm->GetTransformation().m.m[0][0]));
			rtcUpdate(embreeScene, elem.second);
			updated = true;
		}
	}

	if (updated)
		rtcCommit(embreeScene);
}

bool EmbreeAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
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
	embreeRay.time = (ray->time - minTime) * timeScale;

	rtcIntersect(embreeScene, embreeRay);

	if (embreeRay.geomID != RTC_INVALID_GEOMETRY_ID) {
		hit->meshIndex = (embreeRay.instID == RTC_INVALID_GEOMETRY_ID) ? embreeRay.geomID : embreeRay.instID;
		hit->triangleIndex = embreeRay.primID;

		hit->t = embreeRay.tfar;

		hit->b1 = embreeRay.u;
		hit->b2 = embreeRay.v;

		return true;
	} else
		return false;
}

}
