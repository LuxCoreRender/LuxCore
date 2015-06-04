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

#include <opensubdiv/far/topologyRefinerFactory.h>

#include "slg/shapes/opensubdiv.h"
#include "slg/scene/scene.h"
#include "slg/samplers/random.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace OpenSubdiv;

// OpenSubdiv Vertex container implementation.

struct OpenSubdivVertex {
	OpenSubdivVertex() { }

	OpenSubdivVertex(OpenSubdivVertex const &src) {
		position[0] = src.position[0];
		position[1] = src.position[1];
		position[2] = src.position[2];
	}

	OpenSubdivVertex(Point const &src) {
		position[0] = src.x;
		position[1] = src.y;
		position[2] = src.z;
	}

	void Clear(void * = 0) {
		position[0] = position[1] = position[2] = 0.f;
	}

	void AddWithWeight(OpenSubdivVertex const &src, float weight) {
		position[0] += weight * src.position[0];
		position[1] += weight * src.position[1];
		position[2] += weight * src.position[2];
	}

	void AddVaryingWithWeight(OpenSubdivVertex const &, float) {
	}

	float position[3];
};

OpenSubdivShape::OpenSubdivShape(ExtMesh *m) : Shape() {
	mesh = m;
}

OpenSubdivShape::OpenSubdivShape(const string &fileName) : Shape() {
	mesh = ExtTriangleMesh::LoadExtTriangleMesh(fileName);
}

OpenSubdivShape::~OpenSubdivShape() {
	delete mesh;
}

ExtMesh *OpenSubdivShape::RefineImpl(const Scene *scene) {
	Sdc::SchemeType type = Sdc::SCHEME_CATMARK;

	Sdc::Options options;
	options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);

	Far::TopologyRefinerFactoryBase::TopologyDescriptor desc;
	desc.numVertices = mesh->GetTotalVertexCount();
	desc.numFaces = mesh->GetTotalTriangleCount();
	vector<int> numVertsPerFace(mesh->GetTotalTriangleCount(), 3);
	desc.numVertsPerFace = &numVertsPerFace[0];
	desc.vertIndicesPerFace = (Far::Index *)mesh->GetTriangles();

	// Instantiate a FarTopologyRefiner from the descriptor
	Far::TopologyRefiner *refiner = Far::TopologyRefinerFactory<Far::TopologyRefinerFactoryBase::TopologyDescriptor>::Create(desc,
			Far::TopologyRefinerFactory<Far::TopologyRefinerFactoryBase::TopologyDescriptor>::Options(type, options));

	// Uniformly refine the topology up to 'maxlevel'
	const u_int maxLevel = 3;
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));

	// Allocate a buffer for vertex primvar data. The buffer length is set to
	// be the sum of all children vertices up to the highest level of refinement.
	std::vector<OpenSubdivVertex> vertBuffer(refiner->GetNumVerticesTotal());

	// Initialize coarse mesh positions
	const u_int nCoarseVerts = mesh->GetTotalVertexCount();
	copy(mesh->GetVertices(), mesh->GetVertices() + mesh->GetTotalVertexCount(), vertBuffer.begin());

	// Interpolate vertex data
    refiner->Interpolate(&vertBuffer[0], &vertBuffer[0] + nCoarseVerts);

	// Get interpolated vertices
	const u_int meshVertCount = refiner->GetNumVertices(maxLevel);
	Point *interpolatedVerts = TriangleMesh::AllocVerticesBuffer(meshVertCount);
	for (u_int level = 0, firstVert = 0; level <= maxLevel; ++level) {
		if (level == maxLevel) {
			for (int vert = 0; vert < refiner->GetNumVertices(maxLevel); ++vert) {
				const OpenSubdivVertex &v = vertBuffer[firstVert + vert];

				interpolatedVerts[vert].x = v.position[0];
				interpolatedVerts[vert].y = v.position[1];
				interpolatedVerts[vert].z = v.position[2];
			}
		} else
			firstVert += refiner->GetNumVertices(level);
	}

	// Check the interpolated triangles count
	u_int count = 0;
	for (int face = 0; face < refiner->GetNumFaces(maxLevel); ++face) {
		Far::ConstIndexArray faceIndices = refiner->GetFaceVertices(maxLevel, face);

		for (int vIndex = 1; vIndex < faceIndices.size(); vIndex += 2)
			++count;
	}
	const u_int meshTriCount = count;

	// Get interpolated vertices
	Triangle *interpolatedTris = TriangleMesh::AllocTrianglesBuffer(meshTriCount);
	for (int face = 0, interpolatedIndex = 0; face < refiner->GetNumFaces(maxLevel); ++face) {
		Far::ConstIndexArray faceIndices = refiner->GetFaceVertices(maxLevel, face);

		for (int vIndex = 1; vIndex < faceIndices.size() - 1; ++vIndex) {
			interpolatedTris[interpolatedIndex].v[0] = faceIndices[0];
			interpolatedTris[interpolatedIndex].v[1] = faceIndices[vIndex];
			interpolatedTris[interpolatedIndex].v[2] = faceIndices[vIndex + 1];

			++interpolatedIndex;
		}
	}

	// I can free the refiner
	delete refiner;

	// For some debugging
	Spectrum *meshCols = new Spectrum[meshVertCount];
	RandomGenerator rnd(13);
	for (u_int i = 0; i < meshVertCount; ++i) {
		meshCols[i].c[0] = rnd.floatValue();
		meshCols[i].c[1] = rnd.floatValue();
		meshCols[i].c[2] = rnd.floatValue();
	}

	// Allocated the interpolated mesh	
	ExtTriangleMesh *interpolatedMesh = new ExtTriangleMesh(meshVertCount, meshTriCount,
		interpolatedVerts, interpolatedTris, NULL, NULL, meshCols);

	// Free source mesh
	delete mesh;
	mesh = NULL;

	return interpolatedMesh;
}
