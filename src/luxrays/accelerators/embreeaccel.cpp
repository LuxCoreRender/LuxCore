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

#include <string>
#include <limits>

#if defined (_MSC_VER) || defined (__INTEL_COMPILER)
#include <intrin.h>
#endif

#include <boost/foreach.hpp>

#include "luxrays/core/context.h"
#include "luxrays/accelerators/embreeaccel.h"
#include "luxrays/utils/strutils.h"

namespace luxrays {

EmbreeAccel::EmbreeAccel(const Context *context) : ctx(context),
		uniqueRTCSceneByMesh(MeshPtrCompare), uniqueGeomByMesh(MeshPtrCompare),
		uniqueInstMatrixByMesh(MeshPtrCompare) {
	embreeDevice = rtcNewDevice(NULL);
	embreeScene = NULL;
}

EmbreeAccel::~EmbreeAccel() {
	if (embreeScene) {
		rtcReleaseScene(embreeScene);

		// I have to free all Embree scenes used for instances
		std::pair<const Mesh *, RTCScene> elem;
		BOOST_FOREACH(elem, uniqueRTCSceneByMesh)
			rtcReleaseScene(elem.second);
	}

	rtcReleaseDevice(embreeDevice);
}

void EmbreeAccel::ExportTriangleMesh(const RTCScene embreeScene, const Mesh *mesh) const {
	const RTCGeometry geom = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	
	// Share with Embree the mesh vertices
	Point *meshVerts = mesh->GetVertices();
	rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, meshVerts,
			0, sizeof(Point), mesh->GetTotalVertexCount());


	// Share with Embree the mesh triangles
	Triangle *meshTris = mesh->GetTriangles();
	rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, meshTris,
			0, sizeof(Triangle), mesh->GetTotalTriangleCount());
	
	rtcCommitGeometry(geom);
	
	rtcAttachGeometry(embreeScene, geom);
	
	rtcReleaseGeometry(geom);

}

void EmbreeAccel::ExportMotionTriangleMesh(const RTCScene embreeScene, const MotionTriangleMesh *mtm) const {
	const MotionSystem &ms = mtm->GetMotionSystem();

	// Check if I would need more than the max. number of steps (i.e. 129) supported by Embree
	if (ms.times.size() > RTC_MAX_TIME_STEP_COUNT)
		throw std::runtime_error("Embree accelerator supports up to " + ToString(RTC_MAX_TIME_STEP_COUNT) +
				" motion blur steps, unable to use " + ToString(ms.times.size()));

	const RTCGeometry geom = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
	rtcSetGeometryTimeStepCount(geom, ms.times.size());

	for (u_int step = 0; step < ms.times.size(); ++step) {
		// Copy the mesh start position vertices
		Point *vertices = (Point *)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, step, RTC_FORMAT_FLOAT3,
				sizeof(Point), mtm->GetTotalVertexCount());

		Transform local2World;
		mtm->GetLocal2World(ms.times[step], local2World);
		for (u_int i = 0; i < mtm->GetTotalVertexCount(); ++i)
			vertices[i] = mtm->GetVertex(local2World, i);
	}

	// Share the mesh triangles
	Triangle *meshTris = mtm->GetTriangles();
	rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, (RTCBuffer)meshTris,
			0, sizeof(Triangle), mtm->GetTotalTriangleCount());

	rtcCommitGeometry(geom);
	
	rtcAttachGeometry(embreeScene, geom);

	rtcReleaseGeometry(geom);
	
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
			maxTime = Max(maxTime, mtm->GetMotionSystem().EndTime());
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

	embreeScene = rtcNewScene(embreeDevice);
	rtcSetSceneBuildQuality(embreeScene, RTC_BUILD_QUALITY_HIGH);
	rtcSetSceneFlags(embreeScene, RTC_SCENE_FLAG_DYNAMIC);

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
					instScene = rtcNewScene(embreeDevice);
					ExportTriangleMesh(instScene, instancedMesh);
					rtcCommitScene(instScene);

					uniqueRTCSceneByMesh[instancedMesh] = instScene;
				} else
					instScene = it->second;

				RTCGeometry geom = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_INSTANCE);
				rtcSetGeometryInstancedScene(geom, instScene);
				rtcSetGeometryTransform(geom, 0, RTC_FORMAT_FLOAT3X4_ROW_MAJOR, &(itm->GetTransformation().m.m[0][0]));
				rtcCommitGeometry(geom);

				rtcAttachGeometry(embreeScene, geom);

				rtcReleaseGeometry(geom);

				// Save the instance ID
				uniqueGeomByMesh[mesh] = geom;
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

	rtcCommitScene(embreeScene);

	LR_LOG(ctx, "EmbreeAccel build time: " << int((WallClockTime() - t0) * 1000) << "ms");
}

void EmbreeAccel::Update() {
	// Update all Embree scenes used for instances
	bool updated = false;
	std::pair<const Mesh *, RTCGeometry> elem;
	BOOST_FOREACH(elem, uniqueGeomByMesh) {
		const InstanceTriangleMesh *itm = dynamic_cast<const InstanceTriangleMesh *>(elem.first);

		// Check if the transformation has changed
		if (uniqueInstMatrixByMesh[elem.first] != itm->GetTransformation().m) {
			rtcSetGeometryTransform(elem.second, 0, RTC_FORMAT_FLOAT3X4_ROW_MAJOR, &(itm->GetTransformation().m.m[0][0]));
			rtcCommitGeometry(elem.second);
			updated = true;
		}
	}

	if (updated)
		rtcCommitScene(embreeScene);
}

bool EmbreeAccel::MeshPtrCompare(const Mesh *p0, const Mesh *p1) {
	return p0 < p1;
}

bool EmbreeAccel::Intersect(const Ray *ray, RayHit *hit) const {
	RTCIntersectContext context;
	rtcInitIntersectContext(&context);

	RTCRayHit embreeRayHit;

	embreeRayHit.ray.org_x = ray->o.x;
	embreeRayHit.ray.org_y = ray->o.y;
	embreeRayHit.ray.org_z = ray->o.z;

	embreeRayHit.ray.dir_x = ray->d.x;
	embreeRayHit.ray.dir_y = ray->d.y;
	embreeRayHit.ray.dir_z = ray->d.z;

	embreeRayHit.ray.tnear = ray->mint;
	embreeRayHit.ray.tfar = ray->maxt;

	embreeRayHit.ray.mask = 0xFFFFFFFF;
	embreeRayHit.ray.time = (ray->time - minTime) * timeScale;

	embreeRayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
	embreeRayHit.hit.primID = RTC_INVALID_GEOMETRY_ID;
	embreeRayHit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
	
	rtcIntersect1(embreeScene, &context, &embreeRayHit);

	if ((embreeRayHit.hit.geomID != RTC_INVALID_GEOMETRY_ID) &&
			// A safety check in case of not enough numerical precision. Embree
			// can return some intersection out of [mint, maxt] range for
			// some extremely large floating point number.
			(embreeRayHit.ray.tfar >= ray->mint) && (embreeRayHit.ray.tfar <= ray->maxt)) {
		hit->meshIndex = (embreeRayHit.hit.instID[0] == RTC_INVALID_GEOMETRY_ID) ? embreeRayHit.hit.geomID : embreeRayHit.hit.instID[0];
		hit->triangleIndex = embreeRayHit.hit.primID;

		hit->t = embreeRayHit.ray.tfar;

		hit->b1 = embreeRayHit.hit.u;
		hit->b2 = embreeRayHit.hit.v;

		return true;
	} else
		return false;
}

}
