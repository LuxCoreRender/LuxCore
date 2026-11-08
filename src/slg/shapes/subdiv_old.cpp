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


#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/buffer.h"
#include <memory>
#include <unordered_map>
#include <format>

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/osd/tbbEvaluator.h>
#define OSD_EVALUATOR Osd::TbbEvaluator

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/subdiv_old.h"
#include "slg/scene/scene.h"



namespace {
// Everything inside this namespace is local to this translation
// unit (behaves like static)

using namespace luxrays;
using namespace slg;
using namespace OpenSubdiv;

using BufferPtr = std::unique_ptr<Osd::CpuVertexBuffer>;
using StencilTablePtr = std::unique_ptr<const Far::StencilTable>;
using PatchTablePtr = std::unique_ptr<Far::PatchTable>;
using PatchMapPtr = std::unique_ptr<Far::PatchMap>;
using TopologyRefinerPtr = std::unique_ptr<Far::TopologyRefiner>;


//////////////////////////
//  Simple subdivision  //
//////////////////////////

// This is a "simple" implementation (not so simple, however), with the
// following characteristics:
// - Subdivision is uniform (no special focus on singularities)
// - Tessellation is directly derivated from subdivision.
// - Implementation is partly mono-threaded

namespace simple {

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

template <int DIMENSIONS> static BufferPtr
BuildBuffer(
	StencilTablePtr& stencilTable,  // Stencil table
	const float *data,  // Primvar data
	const int nCoarse,  // Coarse vertex count
	const int nRefined  // Refined vertex count
)
{
    BufferPtr buffer(
		Osd::CpuVertexBuffer::Create(DIMENSIONS, nCoarse + nRefined)
	);

	// Pack the primvar data at the start of the vertex buffer
	// and update every time primvar data changes
	buffer->UpdateData(data, 0, nCoarse);

    Osd::BufferDescriptor desc(0, DIMENSIONS, DIMENSIONS);
    Osd::BufferDescriptor newDesc(nCoarse * DIMENSIONS, DIMENSIONS, DIMENSIONS);

	// Refine points (coarsePoints -> refinedPoints)
	OSD_EVALUATOR::EvalStencils(
		buffer.get(),
		desc,
		buffer.get(),
		newDesc,
		stencilTable.get()
	);

	return buffer;
}


static Far::TopologyRefiner * createFarTopologyRefiner(ExtTriangleMeshConstRef);

static ExtTriangleMeshUPtr ApplySubdivAdaptive(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel
);

static ExtTriangleMeshUPtr ApplyAdaptiveSubdiv(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel
);



ExtTriangleMeshUPtr ApplySubdiv(ExtTriangleMeshRef srcMesh, const u_int maxLevel) {
	//--------------------------------------------------------------------------
	// Refine topology
	//--------------------------------------------------------------------------
	// https://graphics.pixar.com/opensubdiv/docs/osd_tutorial_0.html

	// Set-up refiner
	std::unique_ptr<Far::TopologyRefiner> refiner(createFarTopologyRefiner(srcMesh));

	// Refine the topology up to 'maxlevel'
	SDL_LOG("Subdivision - Refining (uniform)");
	Far::TopologyRefiner::UniformOptions refiner_options(maxLevel);
	refiner_options.fullTopologyInLastLevel = true;
	refiner->RefineUniform(refiner_options);

	Far::TopologyLevel const& refFirstLevel = refiner->GetLevel(0);
	Far::TopologyLevel const& refLastLevel = refiner->GetLevel(maxLevel);

	// Check validity
	if (refiner->GetMaxLevel() < maxLevel) {
		SDL_LOG("WARNING - SUBDIVISION FAILURE");
		SDL_LOG("'maxlevel' may be too high, please check. Continuing with input mesh...");
		return srcMesh.Copy();
	}

	// Get topology constants
	const int nCoarseVerts = refFirstLevel.GetNumVertices(); // Coarse vertex count
	const int nCoarseFaces = refFirstLevel.GetNumFaces(); // Coarse face count
	const int nRefinedVerts = refLastLevel.GetNumVertices(); // Refined vertex count
	const int nRefinedFaces = refLastLevel.GetNumFaces(); // Coarse face count
	const int totalVertsCount = nCoarseVerts + nRefinedVerts;  // Total vertex count

	SDL_LOG("Subdivision - Refined vertices: " << nRefinedVerts);
	SDL_LOG("Subdivision - Refined triangles: " << nRefinedFaces);


	//--------------------------------------------------------------------------
	// Create stencil and patch tables
	//--------------------------------------------------------------------------

	StencilTablePtr stencilTable;
	PatchTablePtr patchTable;

	{
		// Create stencil table
		Far::StencilTableFactory::Options stencilOptions;
		stencilOptions.generateOffsets = true;
		stencilOptions.generateIntermediateLevels = false;
		stencilOptions.maxLevel = maxLevel;

		stencilTable = StencilTablePtr(
			Far::StencilTableFactory::Create(*refiner, stencilOptions)
		);

		// Create patch table
		Far::PatchTableFactory::Options patchOptions;
		patchOptions.SetEndCapType(
				Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS
		);

		patchTable = PatchTablePtr(Far::PatchTableFactory::Create(*refiner, patchOptions));

		// Append local point stencils
		const auto *localPointStencilTable = patchTable->GetLocalPointStencilTable();
		if (localPointStencilTable) {
			StencilTablePtr combinedTable(
				Far::StencilTableFactory::AppendLocalPointStencilTable(
					*refiner, stencilTable.get(), localPointStencilTable
				)
			);
			if (combinedTable) {
				std::swap(stencilTable, combinedTable);
			}
		}
	}

	SDL_LOG("Subdivision - Stencil and patch tables created");

	//--------------------------------------------------------------------------
	// Set buffers and evaluate primvar from stencils
	//--------------------------------------------------------------------------


	// Vertices
	auto vertsBuffer = BuildBuffer<3>(
		stencilTable,
		srcMesh.GetVerticesAsFloats().data(),
		nCoarseVerts,
		nRefinedVerts
	);

	// Normals
    BufferPtr normsBuffer;
	if (srcMesh.HasNormals()) {
        normsBuffer = BuildBuffer<3>(
			stencilTable,
			reinterpret_cast<float *>(srcMesh.GetNormals().Data()),
			nCoarseVerts,
			nRefinedVerts
		);
	}

	// UVs
	std::vector<BufferPtr> uvsBuffers(EXTMESH_MAX_DATA_COUNT);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasUVs(i)) {
			uvsBuffers[i] = BuildBuffer<2>(
				stencilTable,
				(const float *)srcMesh.GetUVs(i).get(),
				nCoarseVerts,
nRefinedVerts
			);
		}
	}

	// Colors
	std::vector<BufferPtr> colsBuffers(EXTMESH_MAX_DATA_COUNT);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasColors(i)) {
			colsBuffers[i] = BuildBuffer<3>(
				stencilTable,
				(const float *)srcMesh.GetColors(i).get(),
				nCoarseVerts,
				nRefinedVerts
			);
		}
	}

	// Alphas
	std::vector<BufferPtr> alphasBuffers(EXTMESH_MAX_DATA_COUNT);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasAlphas(i)) {
			alphasBuffers[i] = BuildBuffer<1>(
				stencilTable,
				(const float *)srcMesh.GetAlphas(i).get(),
				nCoarseVerts,
				nRefinedVerts
			);
		}
	}

	// VertAOVs
	std::vector<BufferPtr> vertAOVSsBuffers(EXTMESH_MAX_DATA_COUNT);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasVertexAOV(i)) {
			vertAOVSsBuffers[i] = BuildBuffer<1>(
				stencilTable,
				(const float *)srcMesh.GetVertexAOVs(i).get(),
				nCoarseVerts,
				nRefinedVerts
			);
		}
	}

	SDL_LOG("Subdivision - Buffers built");

	//--------------------------------------------------------------------------
	// Build the new mesh
	//--------------------------------------------------------------------------

	// New triangles
	TriangleBuffer newTris(nRefinedFaces);
	for (int face = 0; face < nRefinedFaces; ++face) {
		Vtr::ConstIndexArray faceVerts = refLastLevel.GetFaceVertices(face);
		for (u_int vertex = 0; vertex < 3; ++vertex) {
			newTris[face].v[vertex] = faceVerts[vertex];
		}
	}

	// New vertices
	VertexBuffer newVerts(nRefinedVerts);
	const float *refinedVerts = vertsBuffer->BindCpuBuffer() + 3 * nCoarseVerts;
	newVerts.Set(std::span<const float>(refinedVerts, size_t(3 * nRefinedVerts)));

	// New normals
	NormalBuffer newNorms;
	if (srcMesh.HasNormals()) {
		newNorms.Allocate(nRefinedVerts);
		const float *refinedNorms = normsBuffer->BindCpuBuffer() + 3 * nCoarseVerts;
		std::copy_n(
			refinedNorms,
			3 * nRefinedVerts,
			newNorms.GetSubObjects().begin()
		);
	}

	// New UVs
	ExtMeshProp<UV> newUVs;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasUVs(i)) {
			// AllocateLayer also records the layer size in _size
			newUVs.AllocateLayer(i, nRefinedVerts);

			const float *refinedUVs = uvsBuffers[i]->BindCpuBuffer() + 2 * nCoarseVerts;
			std::copy(refinedUVs, refinedUVs + 2 * nRefinedVerts, newUVs[i].get());
		} else
			newUVs[i] = nullptr;
	}

	// New colors
	ExtMeshProp<Spectrum> newCols;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasColors(i)) {
			newCols.AllocateLayer(i, nRefinedVerts);

			const float *refinedCols = colsBuffers[i]->BindCpuBuffer() + 3 * nCoarseVerts;
			std::copy(refinedCols, refinedCols + 3 * nRefinedVerts, newCols[i].get());
		} else
			newCols[i] = nullptr;
	}

	// New alphas
	ExtMeshProp<float> newAlphas;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasAlphas(i)) {
			newAlphas.AllocateLayer(i, nRefinedVerts);

			const float *refinedAlphas = alphasBuffers[i]->BindCpuBuffer() + 1 * nCoarseVerts;
			std::copy(refinedAlphas, refinedAlphas + 1 * nRefinedVerts, newAlphas[i].get());
		} else
			newAlphas[i] = nullptr;
	}

	// New vertAOVs
	ExtMeshProp<float> newVertAOVs;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasVertexAOV(i)) {
			SDL_LOG("Subdivision (enhanced) - Evaluating AOV layer #" << i);
			newVertAOVs.AllocateLayer(i, nRefinedVerts);

			const float *refinedVertAOVs = vertAOVSsBuffers[i]->BindCpuBuffer() + 1 * nCoarseVerts;
			std::copy(refinedVertAOVs, refinedVertAOVs + 1 * nRefinedVerts, newVertAOVs[i].get());
		} else
			newVertAOVs[i] = nullptr;
	}

	// Allocate the new mesh
	ExtTriangleMeshUPtr newMesh = std::make_unique<ExtTriangleMesh>(
		std::move(newVerts), std::move(newTris), std::move(newNorms),
		newUVs, newCols, newAlphas
	);

	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		newMesh->SetVertexAOV(i, newVertAOVs.GetLayer(i), newVertAOVs.GetLayerSize());
	}

	return newMesh;
}

static Far::TopologyRefiner* createFarTopologyRefiner(ExtTriangleMeshConstRef srcMesh)
{
	// Set topology descriptor
	Far::TopologyDescriptor desc;
	desc.numVertices = srcMesh.GetTotalVertexCount();
	desc.numFaces = srcMesh.GetTotalTriangleCount();
	std::vector<int> vertPerFace(desc.numFaces, 3);
	desc.numVertsPerFace = &vertPerFace[0];
	//desc.vertIndicesPerFace = reinterpret_cast<const int *>(srcMesh.GetTriangles());
	desc.vertIndicesPerFace = reinterpret_cast<const int *>(srcMesh.GetTrianglesAsInts().data());

	// Look for mesh boundary edges
	std::unordered_map<Edge, u_int, EdgeHashFunction> edgesMap;
	const auto triCount = srcMesh.GetTotalTriangleCount();
	const TriangleBuffer tris(srcMesh.GetTriangles());

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

	std::vector<bool> isBoundaryVertex(srcMesh.GetTotalVertexCount(), false);
	std::vector<Far::Index> cornerVertexIndices;
	std::vector<float> cornerWeights;
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
		assert(cornerVertexIndices.size() <= std::numeric_limits<int>::max());
		desc.numCorners = cornerVertexIndices.size();
		desc.cornerVertexIndices = &cornerVertexIndices[0];
		desc.cornerWeights = &cornerWeights[0];
	}

	// Set topology refiner factory's type & options
	Sdc::SchemeType type = Sdc::SCHEME_LOOP;
	Sdc::Options options;
	options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);

	// Instantiate a Far::TopologyRefiner from the descriptor
	using RefinerFactory = Far::TopologyRefinerFactory<Far::TopologyDescriptor>;
	auto *refiner = RefinerFactory::Create(
		desc,
		RefinerFactory::Options(type, options)
	);

	return refiner;
}

}  // ~namespace simple

}  // ~namespace (local)


//////////////////////////
//        API           //
//////////////////////////

//------------------------------------------------------------------------------
// SubdivOldShape: the former "simple" (uniform) subdivision path
//------------------------------------------------------------------------------

SubdivOldShape::SubdivOldShape(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel
) {
	const double startTime = WallClockTime();

	if (maxLevel > 0) {
		SDL_LOG("Subdividing shape " << srcMesh.GetName() << " at level: " << maxLevel << " (uniform)");
		mesh = ApplySubdiv(srcMesh, maxLevel);
		SDL_LOG("Subdivided shape from " << srcMesh.GetTotalTriangleCount() << " to " << mesh->GetTotalTriangleCount() << " faces");
	} else {
		SDL_LOG("Subdivision: level=0 - Skipping");
		mesh = srcMesh.Copy();
	}

	SDL_LOG("Subdividing time: " << WallClockTime() - startTime << " secs");
}

SubdivOldShape::~SubdivOldShape() {
}

ExtTriangleMeshUPtr SubdivOldShape::ApplySubdiv(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel
) {
	assert(srcMesh.GetTotalVertexCount() <= std::numeric_limits<int>::max());
	assert(srcMesh.GetTotalTriangleCount() <= std::numeric_limits<int>::max());

	SDL_LOG("Subdivision - Starting - "
		<< srcMesh.GetTotalVertexCount() << " points / "
		<< srcMesh.GetTotalTriangleCount() << " triangles"
	);

	return simple::ApplySubdiv(srcMesh, maxLevel);
}

ExtTriangleMeshUPtr SubdivOldShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
