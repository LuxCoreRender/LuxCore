/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <boost/format.hpp>

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>

#include "slg/shapes/subdiv.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace OpenSubdiv;

SubdivShape::SubdivShape(ExtTriangleMesh *srcMesh, const u_int maxLvl) :
		Shape(), maxLevel(maxLvl) {
	SDL_LOG("Subdividing shape " << srcMesh->GetName() << " at level: " << maxLevel);

	const double startTime = WallClockTime();

	Sdc::SchemeType type = Sdc::SCHEME_LOOP;

	Sdc::Options options;
	options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

	Far::TopologyDescriptor desc;
	desc.numVertices = srcMesh->GetTotalVertexCount();
	desc.numFaces = srcMesh->GetTotalTriangleCount();
	vector<int> vertPerFace(desc.numFaces, 3);
	desc.numVertsPerFace = &vertPerFace[0];
	desc.vertIndicesPerFace = (const int *)srcMesh->GetTriangles();

	// Instantiate a Far::TopologyRefiner from the descriptor
	Far::TopologyRefiner *refiner = Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Create(desc,
			Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Options(type, options));

    // Uniformly refine the topology up to 'maxlevel'
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));

	//--------------------------------------------------------------------------
	// We want buffers for the last/finest subdivision level to persist.  We
	// have no interest in the intermediate levels.
    //
    // Determine the sizes for our needs:
    const u_int tmpVertsCount = refiner->GetNumVerticesTotal() -
		srcMesh->GetTotalVertexCount() - refiner->GetLevel(maxLevel).GetNumVertices();

	// Allocate intermediate and final storage to be populated
	vector<Point> tmpVertsBuffer(tmpVertsCount);
    vector<Point> vertsBuffer(refiner->GetLevel(maxLevel).GetNumVertices());
	
	vector<Normal> tmpNormsBuffer;
    vector<Normal> normsBuffer;
	if (srcMesh->HasNormals()) {
		tmpNormsBuffer.resize(tmpVertsCount);
		normsBuffer.resize(refiner->GetLevel(maxLevel).GetNumVertices());
	}
	
    // Interpolate vertex primvar data
    Far::PrimvarRefiner primvarRefiner(*refiner);

    Point *srcVert = srcMesh->GetVertices();
	Point *dstVert = &tmpVertsBuffer[0];

	Normal *srcNorm = srcMesh->HasNormals() ? srcMesh->GetNormals() : nullptr;
	Normal *dstNorm = srcMesh->HasNormals() ? &tmpNormsBuffer[0] : nullptr;

	for (u_int level = 1; level < maxLevel; ++level) {
		primvarRefiner.Interpolate(level, srcVert, dstVert);

		srcVert = dstVert;
		dstVert += refiner->GetLevel(level).GetNumVertices();

		if (srcMesh->HasNormals()) {
			primvarRefiner.InterpolateVarying(level, srcNorm, dstNorm);
			srcNorm = dstNorm;
			dstNorm += refiner->GetLevel(level).GetNumVertices();
		}
	}

	//--------------------------------------------------------------------------
	// Create a new mesh of the highest level refined
	//--------------------------------------------------------------------------

	Far::TopologyLevel const &refLastLevel = refiner->GetLevel(maxLevel);

	const u_int newVertsCount = refLastLevel.GetNumVertices();
	Point *newVerts = TriangleMesh::AllocVerticesBuffer(newVertsCount);
	// Interpolate the last level into the separate buffers for our final data
    primvarRefiner.Interpolate(maxLevel, srcVert, newVerts);

	const u_int newTrisCount = refLastLevel.GetNumFaces();
	Triangle *newTris = TriangleMesh::AllocTrianglesBuffer(newTrisCount);

	for (u_int triIndex = 0; triIndex < newTrisCount; ++triIndex) {
		Far::ConstIndexArray triVerts = refLastLevel.GetFaceVertices(triIndex);
		assert(triVerts.size() == 3);
		
		Triangle &tri = newTris[triIndex];
		tri.v[0] = triVerts[0];
		tri.v[1] = triVerts[1];
		tri.v[2] = triVerts[2];
    }

	Normal *newNorms = nullptr;
	if (srcMesh->HasNormals()) {
		newNorms = new Normal[newVertsCount];
		// Interpolate the last level into the separate buffers for our final data
		primvarRefiner.InterpolateVarying(maxLevel, srcNorm, newNorms);
	}
	
	SDL_LOG("Subdividing shape from " << desc.numFaces << " to " << newTrisCount << " faces");
	mesh = new ExtTriangleMesh(newVertsCount, newTrisCount, newVerts, newTris, newNorms);
	
	const double endTime = WallClockTime();
	SDL_LOG("Subdividing time: " << (boost::format("%.3f") % (endTime - startTime)) << "secs");
}

SubdivShape::~SubdivShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *SubdivShape::RefineImpl(const Scene *scene) {
	return mesh;
}
