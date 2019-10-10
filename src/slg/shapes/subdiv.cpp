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

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>

#include "slg/shapes/subdiv.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace OpenSubdiv;

SubdivShape::SubdivShape(ExtTriangleMesh *srcMesh) : Shape() {
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

	const u_int maxLevel = 2;

    // Uniformly refine the topology up to 'maxlevel'
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));

    // Allocate a buffer for vertex primvar data. The buffer length is set to
    // be the sum of all children vertices up to the highest level of refinement.
    vector<Point> vertsBuffer(refiner->GetNumVerticesTotal());
    Point *verts = &vertsBuffer[0];
	
	// Initialize coarse mesh positions
	copy(srcMesh->GetVertices(), srcMesh->GetVertices() + srcMesh->GetTotalVertexCount(), verts);
	
    // Interpolate vertex primvar data
    Far::PrimvarRefiner primvarRefiner(*refiner);

    Point *src = verts;
	for (u_int level = 1; level <= maxLevel; ++level) {
		Point *dst = src + refiner->GetLevel(level - 1).GetNumVertices();
		primvarRefiner.Interpolate(level, src, dst);
		src = dst;
	}

	// Create a new mesh of the highest level refined
	Far::TopologyLevel const &refLastLevel = refiner->GetLevel(maxLevel);

	const u_int newVertsCount = refLastLevel.GetNumVertices();
	Point *newVerts = TriangleMesh::AllocVerticesBuffer(newVertsCount);

	const u_int newTrisCount = refLastLevel.GetNumFaces();
	Triangle *newTris = TriangleMesh::AllocTrianglesBuffer(newTrisCount);

	const u_int firstOfLastVerts = refiner->GetNumVerticesTotal() - newVertsCount;

	for (u_int vertIndex = 0; vertIndex < newVertsCount; ++vertIndex)
		newVerts[vertIndex] = verts[firstOfLastVerts + vertIndex];

	for (u_int triIndex = 0; triIndex < newTrisCount; ++triIndex) {
		Far::ConstIndexArray triVerts = refLastLevel.GetFaceVertices(triIndex);
		assert(triVerts.size() == 3);
		
		Triangle &tri = newTris[triIndex];
		tri.v[0] = triVerts[0];
		tri.v[1] = triVerts[1];
		tri.v[2] = triVerts[2];
    }
	
	mesh = new ExtTriangleMesh(newVertsCount, newTrisCount, newVerts, newTris);
}

SubdivShape::~SubdivShape() {
	if (!refined)
		delete mesh;
}

ExtTriangleMesh *SubdivShape::RefineImpl(const Scene *scene) {
	return mesh;
}
