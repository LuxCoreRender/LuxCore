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

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/primvarRefiner.h>

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
		pos = src.pos;
		normal = src.normal;
		uv = src.uv;
		col = src.col;
		alpha = src.alpha;
	}

	void Clear(void * = 0) {
		pos = Point();
		normal = Normal();
		uv = UV();
		col = Spectrum();
		alpha = 0.f;
	}

	void AddWithWeight(OpenSubdivVertex const &src, float weight) {
		pos += weight * src.pos;
		normal += weight * src.normal;
		uv += weight * src.uv;
		col += weight * src.col;
		alpha += weight * src.alpha;
	}

	void AddVaryingWithWeight(OpenSubdivVertex const &, float) {
	}

	Point pos;
	Normal normal;
	UV uv;
	Spectrum col;
	float alpha;
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

	Far::TopologyDescriptor desc;
	desc.numVertices = mesh->GetTotalVertexCount();
	desc.numFaces = mesh->GetTotalTriangleCount();
	vector<int> numVertsPerFace(mesh->GetTotalTriangleCount(), 3);
	desc.numVertsPerFace = &numVertsPerFace[0];
	desc.vertIndicesPerFace = (Far::Index *)mesh->GetTriangles();

	// Instantiate a FarTopologyRefiner from the descriptor
	Far::TopologyRefiner *refiner = Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Create(desc,
			Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Options(type, options));

	// Uniformly refine the topology up to 'maxlevel'
	const u_int maxLevel = 3;
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));

	// Allocate a buffer for vertex primvar data. The buffer length is set to
	// be the sum of all children vertices up to the highest level of refinement.
	std::vector<OpenSubdivVertex> vertBuffer(refiner->GetNumVerticesTotal());

	// Initialize coarse mesh data
	const u_int nCoarseVerts = mesh->GetTotalVertexCount();
	const Point *meshVerts = mesh->GetVertices();
	const Normal *meshNorms = mesh->GetNormals();
	const UV *meshUVs = mesh->GetUVs();
	const Spectrum *meshCols = mesh->GetCols();
	const float *meshAlphas = mesh->GetAlphas();
	for (u_int i = 0; i < nCoarseVerts; ++i) {
		OpenSubdivVertex &v = vertBuffer[i];
		
		v.pos = meshVerts[i];
		v.normal = meshNorms ? meshNorms[i] : Normal();
		v.uv = meshUVs ? meshUVs[i] : UV();
		v.col = meshCols ? meshCols[i] : Spectrum();
		v.alpha = meshAlphas ? meshAlphas[i] : 0.f;
	}

	// Interpolate both vertex and face-varying primvar data
	Far::PrimvarRefiner primvarRefiner(*refiner);

	OpenSubdivVertex *srcVert = &vertBuffer[0];
	for (u_int level = 1; level <= maxLevel; ++level) {
		OpenSubdivVertex *dstVert = srcVert + refiner->GetLevel(level - 1).GetNumVertices();

		primvarRefiner.Interpolate(level, srcVert, dstVert);

		srcVert = dstVert;
	}

	// Get interpolated vertices
	Far::TopologyLevel const &refLastLevel = refiner->GetLevel(maxLevel);
	const u_int meshVertCount = refLastLevel.GetNumVertices();
	const u_int firstOfLastVerts = refiner->GetNumVerticesTotal() - meshVertCount;

	Point *interpolatedVerts = TriangleMesh::AllocVerticesBuffer(meshVertCount);
	Normal *interpolatedNorms = meshNorms ? (new Normal[meshVertCount]) : NULL;
	UV *interpolatedUVs = meshUVs ? (new UV[meshVertCount]) : NULL;
	Spectrum *interpolatedCols = meshCols ? (new Spectrum[meshVertCount]) : NULL;
	float *interpolatedAlphas = meshAlphas ? (new float[meshVertCount]) : NULL;

	for (u_int vert = 0; vert < meshVertCount; ++vert) {
		const OpenSubdivVertex &v = vertBuffer[firstOfLastVerts + vert];

		interpolatedVerts[vert] = v.pos;
		if (interpolatedNorms)
			interpolatedNorms[vert] = Normalize(v.normal);
		if (interpolatedUVs)
			interpolatedUVs[vert] = v.uv;
		if (interpolatedCols)
			interpolatedCols[vert] = v.col;
		if (interpolatedAlphas)
			interpolatedAlphas[vert] = v.alpha;
	}

	// Check the interpolated triangles count
	u_int count = 0;
	for (int face = 0; face < refLastLevel.GetNumFaces(); ++face) {
		Far::ConstIndexArray faceIndices = refLastLevel.GetFaceVertices(face);

		for (int vIndex = 1; vIndex < faceIndices.size(); vIndex += 2)
			++count;
	}
	const u_int meshTriCount = count;

	// Get interpolated vertices
	Triangle *interpolatedTris = TriangleMesh::AllocTrianglesBuffer(meshTriCount);
	for (int face = 0, interpolatedIndex = 0; face < refLastLevel.GetNumFaces(); ++face) {
		Far::ConstIndexArray faceIndices = refLastLevel.GetFaceVertices(face);

		for (int vIndex = 1; vIndex < faceIndices.size() - 1; ++vIndex) {
			interpolatedTris[interpolatedIndex].v[0] = faceIndices[0];
			interpolatedTris[interpolatedIndex].v[1] = faceIndices[vIndex];
			interpolatedTris[interpolatedIndex].v[2] = faceIndices[vIndex + 1];

			++interpolatedIndex;
		}
	}

	// I can free the refiner
	delete refiner;

	// Allocated the interpolated mesh	
	ExtTriangleMesh *interpolatedMesh = new ExtTriangleMesh(meshVertCount, meshTriCount,
		interpolatedVerts, interpolatedTris, interpolatedNorms, interpolatedUVs,
		interpolatedCols, interpolatedAlphas);

	// Free source mesh
	delete mesh;
	mesh = NULL;

	return interpolatedMesh;
}
