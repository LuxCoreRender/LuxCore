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
#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>

#if defined(_OPENMP)

#include <opensubdiv/osd/ompEvaluator.h>
#include <unordered_map>
#define OSD_EVALUATOR Osd::OmpEvaluator

#else

#include <opensubdiv/osd/cpuEvaluator.h>
#define OSD_EVALUATOR Osd::CpuEvaluator

#endif

#include "slg/shapes/subdiv.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace OpenSubdiv;

struct Edge {
	Edge(const u_int v0Index, const u_int v1Index) {
		if (v0Index <= v1Index) {
			vIndex[0] = v0Index;
			vIndex[1] = v1Index;
		} else {
			vIndex[0] = v1Index;
			vIndex[1] = v0Index;
		}
	}

	bool operator==(const Edge &edge) const {
		return (vIndex[0] == edge.vIndex[0]) && (vIndex[1] == edge.vIndex[1]);
	}

	u_int vIndex[2];
};

class EdgeHashFunction {
public:
	size_t operator()(const Edge &edge) const {
		return (edge.vIndex[0] * 0x1f1f1f1fu) ^ edge.vIndex[1];
	}
};

SubdivShape::SubdivShape(ExtTriangleMesh *srcMesh, const u_int maxLevel) {
	SDL_LOG("Subdividing shape " << srcMesh->GetName() << " at level: " << maxLevel);

	const double startTime = WallClockTime();

	//--------------------------------------------------------------------------
	// Define the mesh
	//--------------------------------------------------------------------------

	Sdc::SchemeType type = Sdc::SCHEME_LOOP;

	Sdc::Options options;
	options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);

	Far::TopologyDescriptor desc;
	desc.numVertices = srcMesh->GetTotalVertexCount();
	desc.numFaces = srcMesh->GetTotalTriangleCount();
	vector<int> vertPerFace(desc.numFaces, 3);
	desc.numVertsPerFace = &vertPerFace[0];
	desc.vertIndicesPerFace = (const int *)srcMesh->GetTriangles();

	// Look for mesh boundary edges
	unordered_map<Edge, u_int, EdgeHashFunction> edgesMap;
		const u_int triCount = srcMesh->GetTotalTriangleCount();
	const Triangle *tris = srcMesh->GetTriangles();

	// Count how many times an edge is shared
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];
		
		 const Edge edge0(tri.v[0], tri.v[1]);
		 if (edgesMap.find(edge0) != edgesMap.end())
			edgesMap[edge0] += 1;
		 else
			edgesMap[edge0] = 1;
		
		 const Edge edge1(tri.v[1], tri.v[2]);
		 if (edgesMap.find(edge1) != edgesMap.end())
			edgesMap[edge1] += 1;
		 else
			edgesMap[edge1] = 1;
		
		 const Edge edge2(tri.v[2], tri.v[0]);
		 if (edgesMap.find(edge2) != edgesMap.end())
			edgesMap[edge2] += 1;
		 else
			edgesMap[edge2] = 1;
	}

	vector<bool> isBoundaryVertex(srcMesh->GetTotalVertexCount(), false);
	vector<Far::Index> cornerVertexIndices;
	vector<float> cornerWeights;
	for (auto em : edgesMap) {
		if (em.second == 1) {
			// It is a boundary edge
			
			const Edge &e = em.first;
			
			if (!isBoundaryVertex[e.vIndex[0]]) {
				cornerVertexIndices.push_back(e.vIndex[0]);
				cornerWeights.push_back(10.f);
				isBoundaryVertex[e.vIndex[0]] = true;
			}
			
			if (!isBoundaryVertex[e.vIndex[1]]) {
				cornerVertexIndices.push_back(e.vIndex[1]);
				cornerWeights.push_back(10.f);
				isBoundaryVertex[e.vIndex[1]] = true;
			}
		}
	}
	
	// Initialize TopologyDescriptor corners if I have some
	if (cornerVertexIndices.size() > 0) {
		desc.numCorners = cornerVertexIndices.size();
		desc.cornerVertexIndices = &cornerVertexIndices[0];
		desc.cornerWeights = &cornerWeights[0];
	}

	// Instantiate a Far::TopologyRefiner from the descriptor
	Far::TopologyRefiner *refiner = Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Create(desc,
			Far::TopologyRefinerFactory<Far::TopologyDescriptor>::Options(type, options));

    // Uniformly refine the topology up to 'maxlevel'
    refiner->RefineUniform(Far::TopologyRefiner::UniformOptions(maxLevel));

	//--------------------------------------------------------------------------
	// Setup a factory and create StencilTable
	//--------------------------------------------------------------------------

	Far::StencilTableFactory::Options stencilOptions;
    stencilOptions.generateOffsets = true;
    stencilOptions.generateIntermediateLevels = false;
	
	const Far::StencilTable *stencilTable =
		Far::StencilTableFactory::Create(*refiner, stencilOptions);
	
	//--------------------------------------------------------------------------
    // Setup a factory to create PatchTable
	//--------------------------------------------------------------------------

    Far::PatchTableFactory::Options patchOptions;
	patchOptions.SetEndCapType(
			Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS);

	const Far::PatchTable *patchTable =
			Far::PatchTableFactory::Create(*refiner, patchOptions);

	// Append local point stencils
	if (const Far::StencilTable *localPointStencilTable =
			patchTable->GetLocalPointStencilTable()) {
		if (const Far::StencilTable *combinedTable =
				Far::StencilTableFactory::AppendLocalPointStencilTable(
				*refiner, stencilTable, localPointStencilTable)) {
			delete stencilTable;
			stencilTable = combinedTable;
		}
	}

	//--------------------------------------------------------------------------
	// Set buffers and evaluate the stencil
	//--------------------------------------------------------------------------

	// Setup a buffer for vertex primvar data
	const u_int vertsCount = refiner->GetLevel(0).GetNumVertices();
    const u_int newVertsCount = refiner->GetNumVerticesTotal();
	const u_int totalVertsCount = vertsCount + newVertsCount;

	// Vertices
    Osd::CpuVertexBuffer *vertsBuffer =
        Osd::CpuVertexBuffer::Create(3, totalVertsCount);

    Osd::BufferDescriptor vertsDesc(0, 3, 3);
    Osd::BufferDescriptor newVertsDesc(vertsCount * 3, 3, 3);

	// Pack the control vertex data at the start of the vertex buffer
	// and update every time control data changes
	vertsBuffer->UpdateData((const float *)srcMesh->GetVertices(), 0, vertsCount);

	// Refine vertex points (coarsePoints -> refinedPoints)
	OSD_EVALUATOR::EvalStencils(vertsBuffer, vertsDesc,
		vertsBuffer, newVertsDesc,
		stencilTable);
	
	// Normals
    Osd::CpuVertexBuffer *normsBuffer = nullptr;
	if (srcMesh->HasNormals()) {
        normsBuffer = Osd::CpuVertexBuffer::Create(3, totalVertsCount);

		Osd::BufferDescriptor normsDesc(0, 3, 3);
		Osd::BufferDescriptor newNormsDesc(vertsCount * 3, 3, 3);
		
		normsBuffer->UpdateData((const float *)srcMesh->GetNormals(), 0, vertsCount);

		OSD_EVALUATOR::EvalStencils(normsBuffer, normsDesc,
				normsBuffer, newNormsDesc,
				stencilTable);
	}

	// UVs
    Osd::CpuVertexBuffer *uvsBuffer = nullptr;
	if (srcMesh->HasUVs()) {
        uvsBuffer = Osd::CpuVertexBuffer::Create(2, totalVertsCount);

		Osd::BufferDescriptor uvsDesc(0, 2, 2);
		Osd::BufferDescriptor newUVsDesc(vertsCount * 2, 2, 2);
		
		uvsBuffer->UpdateData((const float *)srcMesh->GetUVs(), 0, vertsCount);

		OSD_EVALUATOR::EvalStencils(uvsBuffer, uvsDesc,
				uvsBuffer, newUVsDesc,
				stencilTable);
	}

	// Cols
    Osd::CpuVertexBuffer *colsBuffer = nullptr;
	if (srcMesh->HasColors()) {
        colsBuffer = Osd::CpuVertexBuffer::Create(3, totalVertsCount);

		Osd::BufferDescriptor colsDesc(0, 3, 3);
		Osd::BufferDescriptor newColsDesc(vertsCount * 3, 3, 3);
		
		colsBuffer->UpdateData((const float *)srcMesh->GetColors(), 0, vertsCount);

		OSD_EVALUATOR::EvalStencils(colsBuffer, colsDesc,
				colsBuffer, newColsDesc,
				stencilTable);
	}

	// Alphas
    Osd::CpuVertexBuffer *alphasBuffer = nullptr;
	if (srcMesh->HasAlphas()) {
        alphasBuffer = Osd::CpuVertexBuffer::Create(1, totalVertsCount);

		Osd::BufferDescriptor alphasDesc(0, 1, 1);
		Osd::BufferDescriptor newAlphasDesc(vertsCount * 1, 1, 1);
		
		alphasBuffer->UpdateData((const float *)srcMesh->GetAlphas(), 0, vertsCount);

		OSD_EVALUATOR::EvalStencils(alphasBuffer, alphasDesc,
				alphasBuffer, newAlphasDesc,
				stencilTable);
	}

	//--------------------------------------------------------------------------
	// Build the new mesh
	//--------------------------------------------------------------------------

	// New vertices
	Point *newVerts = TriangleMesh::AllocVerticesBuffer(newVertsCount);

	const float *refinedVerts = vertsBuffer->BindCpuBuffer() + 3 * vertsCount;
	copy(refinedVerts, refinedVerts + 3 * newVertsCount, &newVerts->x);

	// New triangles
	u_int newTrisCount = 0;
	for (int array = 0; array < patchTable->GetNumPatchArrays(); ++array)
		for (int patch = 0; patch < patchTable->GetNumPatches(array); ++patch)
			++newTrisCount;

	Triangle *newTris = TriangleMesh::AllocTrianglesBuffer(newTrisCount);
	u_int triIndex = 0;
	for (int array = 0; array < patchTable->GetNumPatchArrays(); ++array) {
		for (int patch = 0; patch < patchTable->GetNumPatches(array); ++patch) {
			const Far::ConstIndexArray faceVerts =
				patchTable->GetPatchVertices(array, patch);
			
			assert (faceVerts.size() == 3);
			newTris[triIndex].v[0] = faceVerts[0] - vertsCount;
			newTris[triIndex].v[1] = faceVerts[1] - vertsCount;
			newTris[triIndex].v[2] = faceVerts[2] - vertsCount;

			++triIndex;
		}
	}

	// New normals
	Normal *newNorms = nullptr;
	if (srcMesh->HasNormals()) {
		newNorms = new Normal[newVertsCount];

		const float *refinedNorms = normsBuffer->BindCpuBuffer() + 3 * vertsCount;
		copy(refinedNorms, refinedNorms + 3 * newVertsCount, &newNorms->x);
	}

	// New UVs
	UV *newUVs = nullptr;
	if (srcMesh->HasUVs()) {
		newUVs = new UV[newVertsCount];

		const float *refinedUVs = uvsBuffer->BindCpuBuffer() + 2 * vertsCount;
		copy(refinedUVs, refinedUVs + 2 * newVertsCount, &newUVs->u);
	}

	// New colors
	Spectrum *newCols = nullptr;
	if (srcMesh->HasColors()) {
		newCols = new Spectrum[newVertsCount];

		const float *refinedCols = colsBuffer->BindCpuBuffer() + 3 * vertsCount;
		copy(refinedCols, refinedCols + 3 * newVertsCount, &newCols->c[0]);
	}

	// New alphas
	float *newAlphas = nullptr;
	if (srcMesh->HasAlphas()) {
		newAlphas = new float[newVertsCount];

		const float *refinedAlphas = alphasBuffer->BindCpuBuffer() + 1 * vertsCount;
		copy(refinedAlphas, refinedAlphas + 1 * newVertsCount, newAlphas);
	}

	// Free memory
	delete refiner;
	delete stencilTable;
	delete patchTable;
    delete vertsBuffer;
	delete normsBuffer;
	delete uvsBuffer;
	delete colsBuffer;
	delete alphasBuffer;

	// Allocate the new mesh
	SDL_LOG("Subdividing shape from " << desc.numFaces << " to " << newTrisCount << " faces");
	mesh = new ExtTriangleMesh(newVertsCount, newTrisCount, newVerts, newTris,
			newNorms, newUVs, newCols, newAlphas);

	// For some debugging
	//mesh->Save("debug.ply");

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
