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
#include <unordered_map>
#include <format>

#include <opensubdiv/far/topologyDescriptor.h>
#include <opensubdiv/far/patchMap.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/stencilTableFactory.h>
#include <opensubdiv/far/topologyRefinerFactory.h>
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/far/ptexIndices.h>
#include <opensubdiv/osd/tbbEvaluator.h>
#define OSD_EVALUATOR Osd::TbbEvaluator

#include <omp.h>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/subdiv.h"
#include "slg/shapes/merge_on_distance.h"
#include "slg/cameras/camera.h"
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
using PtexIndicesPtr = std::unique_ptr<Far::PtexIndices>;
using PointArrayPtr = std::unique_ptr<Point[]>;
using NormalArrayPtr = std::unique_ptr<Normal[]>;
using TriangleArrayPtr = std::unique_ptr<Triangle[]>;


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
			for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
				vertAOVSsBuffers[i] = BuildBuffer<1>(
					stencilTable,
					(const float *)srcMesh.GetVertexAOVs(i).get(),
					nCoarseVerts,
					nRefinedVerts
				);
			}
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
			newUVs[i] = std::make_shared<UV[]>(nRefinedVerts);

			const float *refinedUVs = uvsBuffers[i]->BindCpuBuffer() + 2 * nCoarseVerts;
			std::copy(refinedUVs, refinedUVs + 2 * nRefinedVerts, newUVs[i].get());
		} else
			newUVs[i] = nullptr;
	}

	// New colors
	ExtMeshProp<Spectrum> newCols;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasColors(i)) {
			newCols[i] = std::make_shared<Spectrum[]>(nRefinedVerts);

			const float *refinedCols = colsBuffers[i]->BindCpuBuffer() + 3 * nCoarseVerts;
			std::copy(refinedCols, refinedCols + 3 * nRefinedVerts, newCols[i].get());
		} else
			newCols[i] = nullptr;
	}

	// New alphas
	ExtMeshProp<float> newAlphas;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; i++) {
		if (srcMesh.HasAlphas(i)) {
			newAlphas[i] = std::make_shared<float[]>(nRefinedVerts);

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
			newVertAOVs[i] = std::make_shared<float[]>(nRefinedVerts);

			const float *refinedVertAOVs = alphasBuffers[i]->BindCpuBuffer() + 1 * nCoarseVerts;
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

////////////////////////////////////////////////////////////////////////////
//						Enhanced mode
//
// Enhanced mode characteristics:
// - Separate control on subdivision and tessellation
// - Adaptive subdivision (nota bene: in the sense of opensubdiv, not Blender)
// - Parallelized tessellation
// - Parallelized evaluation
//
// We use opensubdiv concepts and vocabulary:
//
// - Subdivision is NOT tessellation
//   (https://graphics.pixar.com/opensubdiv/docs/subdivision_surfaces.html#subdivision-versus-tessellation)
//
// - Topology and Primvar are separate things
//   (https://graphics.pixar.com/opensubdiv/docs/far_overview.html#feature-adaptive-representation-far)
//
// (please read the docs)

namespace enhanced {


// Storage of local coordinates (coordinates in a given face) during
// tessellation, in the form (face, x, y).
struct LocalCoords {
	int face = -1;
	float x = 0.f;
	float y = 0.f;

	LocalCoords() {}

	LocalCoords(int p_face, float p_x, float p_y):
		face(p_face),
		x(p_x),
		y(p_y)
	{ }

	LocalCoords(int p_face, int p_i, int p_j, int N):
		face(p_face),
		x(float(p_i) / float(N)),
		y(float(p_j) / float(N))
	{ }

	// Avoid unexpected use of LocalCoords(int, float, float)
	// with implicit conversion
	LocalCoords(int, int, int) = delete;

	// Interpolating constructor
	LocalCoords(LocalCoords p0, LocalCoords p1, u_int weight, u_int total) {
		assert(p0.face == p1.face);
		assert(weight <= total);

		face = p0.face;

		float weight_f = float(weight);
		float total_f = float(total);
		x = (weight_f * p1.x + (total_f - weight_f) * p0.x) / total_f;
		y = (weight_f * p1.y + (total_f - weight_f) * p0.y) / total_f;
	}
};


typedef std::vector<LocalCoords> CoordVector;

// Adapter for float values in subdiv buffer The main adaptation consists in
// addition of Clear and AddWithWeight methods
struct FloatAdapter {
	FloatAdapter() : x(0.f) {}
	FloatAdapter(float p_x): x(p_x) {}
	void Clear() { x = 0.f; }
	void AddWithWeight(float value, float weight) { x += value * weight; }
	operator float() const { return x; }
	operator float*() { return &x; }

	float x;
};

// Output object for interpolation process
struct InterpolatedValues {
	// Intended to contain:
	// - Refined values, ie values at refined vertices and local vertices

	InterpolatedValues(
		float* p_values,
		int p_numValues,
		int p_stride
	):
		values(p_values),
		numValues(p_numValues),
		stride(p_stride)
	{}

	template <typename T>
	const T* Get() const {
		return reinterpret_cast<const T*>(values.get());
	}

	// Property members
	const std::unique_ptr<float[]> values;
	const int numValues;
	const int stride;
};


// Structure to represent an edge with sharpness information
struct EdgeInfo {
    int v0, v1;          // Vertex indices (always stored with v0 < v1)
    float sharpness;     // Sharpness value (0.0 = smooth, 1.0 = sharp)
    float creaseWeight;  // Crease weight for subdivision

    EdgeInfo(int vertex0, int vertex1)
        : v0(std::min(vertex0, vertex1)), v1(std::max(vertex0, vertex1)),
          sharpness(0.0f), creaseWeight(0.0f) {}
};

// Calculate face normal for a triangle
Vector calculateTriangleNormal(const ExtTriangleMesh &mesh, const Triangle &tri) {
	auto vertices = mesh.GetVertices();
    const Point &p0 = vertices[tri.v[0]];
    const Point &p1 = vertices[tri.v[1]];
    const Point &p2 = vertices[tri.v[2]];

    const Vector edge1 = Vector(p1 - p0);
    const Vector edge2 = Vector(p2 - p0);

    return Normalize(Cross(edge1, edge2));
}

// Calculate dihedral angle between two adjacent triangles
float calculateDihedralAngle(const Vector &normal1, const Vector &normal2,
                           const Vector &edgeVector) {
    // Calculate the angle between the two face normals
    const float cosAngle = Dot(normal1, normal2);
    const float angle = acosf(Clamp(cosAngle, -1.0f, 1.0f));

    // Calculate the edge direction (normalized)
    const Vector edgeDir = Normalize(edgeVector);

    // Determine if the angle is convex or concave using cross product
    const Vector crossProd = Cross(normal1, normal2);
    const float sign = Dot(crossProd, edgeDir);

    // Return the angle in the range [0, π]
    return (sign >= 0) ? angle : M_PI - angle;
}

// Process edges to determine sharpness based on dihedral angles
void processEdgesForSharpness(const ExtTriangleMesh &mesh,
                             std::map<std::pair<int, int>, EdgeInfo> &edgeMap,
                             float sharpnessThresholdRadians) {

	auto triangles = mesh.GetTriangles();
	auto vertices = mesh.GetVertices();

    // Clear any existing edge information
    edgeMap.clear();

    // First pass: build edge map
	for (const auto& tri : triangles) {

        // Process each edge of the triangle
        for (int i = 0; i < 3; ++i) {
            int v0 = tri.v[i];
            int v1 = tri.v[(i + 1) % 3];

            // Create a canonical edge key (always store with v0 < v1)
            auto edgeKey = std::make_pair(std::min(v0, v1), std::max(v0, v1));

            // If edge doesn't exist in map, create it
            if (edgeMap.find(edgeKey) == edgeMap.end()) {
                edgeMap.try_emplace(edgeKey, v0, v1);
            }
        }
    }

    // Second pass: calculate dihedral angles for edges shared by two triangles
    for (auto &edgePair : edgeMap) {
        EdgeInfo &edgeInfo = edgePair.second;
        int v0 = edgeInfo.v0;
        int v1 = edgeInfo.v1;

        // Find all triangles that share this edge
        std::vector<size_t> adjacentTris;
        for (size_t triIndex = 0; triIndex < triangles.size(); ++triIndex) {
            const Triangle &tri = triangles[triIndex];
            // Check if triangle contains this edge (in either order)
            if ((tri.v[0] == v0 && tri.v[1] == v1) ||
                (tri.v[1] == v0 && tri.v[2] == v1) ||
                (tri.v[2] == v0 && tri.v[0] == v1) ||
                (tri.v[0] == v1 && tri.v[1] == v0) ||
                (tri.v[1] == v1 && tri.v[2] == v0) ||
                (tri.v[2] == v1 && tri.v[0] == v0)) {
                adjacentTris.push_back(triIndex);
            }
        }

        // If edge is on boundary (only one adjacent triangle), mark as sharp
        if (adjacentTris.size() == 1) {
            edgeInfo.sharpness = 1.0f;
            edgeInfo.creaseWeight = 1.0f;
            continue;
        }

        // For internal edges, calculate dihedral angle
        if (adjacentTris.size() >= 2) {
            const Triangle &tri1 = triangles[adjacentTris[0]];
            const Triangle &tri2 = triangles[adjacentTris[1]];

            const Vector normal1 = calculateTriangleNormal(mesh, tri1);
            const Vector normal2 = calculateTriangleNormal(mesh, tri2);

            // Calculate edge vector
            const Vector edgeVector = Vector(vertices[v1] - vertices[v0]);
            const float dihedralAngle = calculateDihedralAngle(normal1, normal2, edgeVector);

            // Determine sharpness based on threshold
            if (dihedralAngle > sharpnessThresholdRadians) {
                edgeInfo.sharpness = 1.0f;
                edgeInfo.creaseWeight = 1.0f;
            } else {
                edgeInfo.sharpness = 0.0f;
                edgeInfo.creaseWeight = 0.0f;
            }
        }
    }
}

// Debug function to output edge sharpness information
void debugEdgeSharpness(const ExtTriangleMesh &mesh, float sharpnessThresholdRadians) {
    std::map<std::pair<int, int>, EdgeInfo> edgeMap;
    processEdgesForSharpness(mesh, edgeMap, sharpnessThresholdRadians);

    int sharpCount = 0;
    int smoothCount = 0;
    int boundaryCount = 0;

    for (const auto &edgePair : edgeMap) {
        const EdgeInfo &edge = edgePair.second;
        if (edge.sharpness > 0.5f) {
            sharpCount++;
        } else {
            smoothCount++;
        }
    }

    SLG_LOG("Edge sharpness analysis:");
    SLG_LOG("  Total edges: " << edgeMap.size());
    SLG_LOG("  Sharp edges: " << sharpCount << " (" <<
           (100.0f * sharpCount / edgeMap.size()) << "%)");
    SLG_LOG("  Smooth edges: " << smoothCount << " (" <<
           (100.0f * smoothCount / edgeMap.size()) << "%)");
}


// Adaptive topology refiner factory function
TopologyRefinerPtr createTopologyAdaptiveRefiner(
	const int * vertIndicesPerFace,
	int numVertices,
	int numFaces,
	const std::map<std::pair<int, int>, EdgeInfo>& edgeMap,
	const float sharpnessThresholdRadians,
	const float creaseWeight,
	const Far::PatchTableFactory::Options& patchOptions
) {

	// Be sure patch options were intialized with the desired max level
	auto adaptiveOptions(patchOptions.GetRefineAdaptiveOptions());
	assert(adaptiveOptions.useInfSharpPatch == patchOptions.useInfSharpPatch);

	// Set topology refiner factory's type & options
	Sdc::SchemeType scheme_type = Sdc::SCHEME_LOOP;

	Sdc::Options sdc_options;
	sdc_options.SetVtxBoundaryInterpolation(Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);

	// Populate a topology descriptor with our raw data
	Far::TopologyDescriptor desc;
	desc.numVertices = numVertices;
	desc.numFaces = numFaces;
	std::vector<int> vertsPerFace(numFaces, 3);  // We only support triangle meshes
	desc.numVertsPerFace = vertsPerFace.data();
	desc.vertIndicesPerFace = vertIndicesPerFace;

	// Set creases (sharp edges)
	std::vector<int> creases;
	std::vector<float> creaseWeights;
    for (const auto &edgePair : edgeMap) {
        const EdgeInfo &edge = edgePair.second;
		if (edge.sharpness > sharpnessThresholdRadians) {
			creases.push_back(edge.v0);
			creases.push_back(edge.v1);
			creaseWeights.push_back(creaseWeight);
		}
	}
	desc.numCreases = creaseWeights.size();
	desc.creaseVertexIndexPairs = creases.data();
	desc.creaseWeights = creaseWeights.data();

	// Create refiner
	using RefinerFactory = Far::TopologyRefinerFactory<Far::TopologyDescriptor>;
    TopologyRefinerPtr refiner(
		RefinerFactory::Create(
			desc,
			RefinerFactory::Options(scheme_type, sdc_options)
		)
	);
	assert(refiner);


	return refiner;

}


/// Subdivision surface
///
/// This is the central object of enhanced subdivision. It implements the following steps:
/// - Construction
/// - Subdivision
/// - Tessellation
/// - Interpolation
/// - Evaluation
///
/// Please note Subdivision and Tessellation are 2 distinct things:
/// https://graphics.pixar.com/opensubdiv/docs/subdivision_surfaces.html#subdivision-versus-tessellation
struct Surface {

	/// Constructor
	///
	/// @param srcMesh The mesh to be subdivided
	Surface(
		ExtTriangleMeshConstRef srcMesh,
		const float sharpnessThresholdRadians,
		const float creaseWeight
	) {
		SDL_LOG("Subdivision (enhanced) - Computing patches");

		// Initialize patch table options
		//
		// Note: ENDCAP_GREGORY_BASIS can generate null normals
		// (try with Suzanne, level=3...):
		// please avoid...
		patchTableOptions.useInfSharpPatch = true;
		patchTableOptions.generateVaryingTables = false;
		patchTableOptions.endCapType =
			Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS;

		// Mark edges for sharpness
		std::map<std::pair<int, int>, EdgeInfo> edgeMap;
		processEdgesForSharpness(srcMesh, edgeMap, sharpnessThresholdRadians);

		// Construct refiner
		refiner = std::move(
			createTopologyAdaptiveRefiner(
				reinterpret_cast<const int *>(srcMesh.GetTrianglesAsInts().data()),
				srcMesh.GetTotalVertexCount(),
				srcMesh.GetTotalTriangleCount(),
				edgeMap,
				sharpnessThresholdRadians,
				creaseWeight,
				patchTableOptions
			)
		);
	}

	/// Subdivide parametric surface
	///
	///	This method applies a refinement on the topology of the control surface.
	///	The refinement is adaptive (in the sense of OpenSubdiv).
	///
	/// @param maxLevel The maximum isolation level
	/// 
	void Subdivide(int maxLevel) {
		// Set refinement level
		patchTableOptions.maxIsolationLevel = maxLevel;

		// Apply adaptive refinement
		refiner->RefineAdaptive(patchTableOptions.GetRefineAdaptiveOptions());

		// Construct the associated PatchTable to evaluate the limit surface:
		patchTable.reset(Far::PatchTableFactory::Create(*refiner, patchTableOptions));

		// Construct ptex indices table
		ptexIndices.reset(new Far::PtexIndices(*refiner));

		// Construct patch map
		patchMap.reset(new Far::PatchMap(*patchTable));
	}

	/// Tessellate input mesh
	///
	/// Apply a triforce tessellation to input mesh.
	///
	/// This tessellation splits edges in N segments,
	/// and generates N² resulting triangles by input triangle.
	/// It's been reimplemented here in order to use multithreading (osd
	/// implementation is monothreaded...).
	///
	/// Input:
	///           V2
	///          / \
	///         /  \
	///        /   \
	///      V0----V1
	///
	///
	///	Output (N=4):
	///
	///          V2
	///         / \
	///        E7-E8
	///       / \/ \
	///      E5-I3-E6
	///     / \/ \/ \
	///    E3-I0-I1-E4
	///   / \/ \/ \/ \
	///	V0-E0-E1-E2-V1
	///
	///	Nota:
	///	Vx - initial vertex (to be kept)
	///	Ex - edge vertex (to be created)
	///	Ix - internal vertex (to be created)
	///
	/// @param N Tessellation rate: each edge will be subdivided into N
	/// sub-edges
	///
	std::tuple<CoordVector, TriangleBuffer, int, int>
	Tessellate (const size_t N) {
		// This is triforce tessellation.

		// Allocate outputs
		CoordVector tessCoords;
		int numTriangles = getTesselTriangleCount(N);
		int numPoints = getTesselPosCount(N);
		TriangleBuffer tessTris(numTriangles);

		// Some constants
		const auto& topology = refiner->GetLevel(0);
		const size_t FACE_COUNT = topology.GetNumFaces();
		const int FACE_SIZE = 3;  // Of course (triangles)...

		// Check
		SDL_LOG(
			"Subdivision (enhanced) - Tessellating with factor N = "
			<< N
		);
		if (FACE_COUNT * N * N >= std::numeric_limits<int>::max()) {
			throw std::runtime_error(
				"FACE_COUNT * N * N > numeric limit ("
				+ ToString(std::numeric_limits<int>::max())
				+ ")"
			);
		}

		// Useful variables and constants
		int offset;

		// Number of coarse vertices
		const int numCoarseVertices = topology.GetNumVertices();

		// Number of interior points in an edge
		const int numEdgeInteriorPoints = N - 1;

		// Number of interior points in a triangle
		const int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;

		// Number of coords to be computed
		const int numCoords =
			topology.GetNumVertices()
			+ numEdgeInteriorPoints * topology.GetNumEdges()
			+ numTriangleInteriorPoints * topology.GetNumFaces();

		// Size tessCoords
		tessCoords.resize(numCoords);

		// Get vertex local coordinates in given face
		auto getVertexLocalCoords = [&topology](int face, int vertex) {
			auto faceVertices = topology.GetFaceVertices(face);
			if (vertex == faceVertices[0]) {
				return LocalCoords(face, 0.f, 0.f);
			} else if (vertex == faceVertices[1]) {
				return LocalCoords(face, 1.f, 0.f);
			} else if (vertex == faceVertices[2]) {
				return LocalCoords(face, 0.f, 1.f);
			}

			throw std::runtime_error("Error in getVertexLocalCoords");
		};

		// Step #1 - Initialize with initial (coarse) vertices
		#pragma omp parallel for
		for (int vertex = 0; vertex < numCoarseVertices; ++vertex) {
			int face = topology.GetVertexFaces(vertex)[0];  // Choose first face (arbitrary)
			tessCoords[vertex] = getVertexLocalCoords(face, vertex);
		}
		offset = numCoarseVertices;

		// Step #2 - Add edge vertices
		#pragma omp parallel for
		for (int edge = 0; edge < topology.GetNumEdges(); ++edge) {
			// Find face
			int face = topology.GetEdgeFaces(edge)[0];
			// Find edge vertices.
			auto edgeVerts = topology.GetEdgeVertices(edge);
			int v0 = edgeVerts[0];
			int v1 = edgeVerts[1];

			LocalCoords c0 = getVertexLocalCoords(face, v0);
			LocalCoords c1 = getVertexLocalCoords(face, v1);

			for (int i = 1; i < N; ++i) {  // Only edge interior vertices, not extremities
				LocalCoords coords(c0, c1, i, N);  // Interpolate from c0 and c1
				tessCoords[offset + edge * numEdgeInteriorPoints + i - 1 ] = coords;
			}
		}
		offset = numCoarseVertices + numEdgeInteriorPoints * topology.GetNumEdges();

		// Step #3 - Add interior points
		#pragma omp parallel for
		for (int f = 0; f < FACE_COUNT; ++f) {  // for each face
			const auto faceVertices = topology.GetFaceVertices(f);
			const auto faceEdges = topology.GetFaceEdges(f);
			assert(faceVertices.size() == 3);

			// If (i,j) are local coordinates, and edge length is N,
			// we have in the triangle: i >= 0, j >= 0, i + j <= N
			// We then consider k so that i + j + k = N.
			// If we are on an edge, i * j * k == 0.
			// If we are on a vertex, i == N or j == N or k == N.
			// In the face (triangle), after tessellation, we'll have N² points.
			// Some of them will be shared with other faces, we'll have to deal
			// with that.

			std::vector<int> pointMap;  // Map points between local numeration and global one

			for (int j = 0; j <= N ; ++j) {
				for (int i = 0; i <= N - j; ++i) {
					int k = N - i - j;
					int vertex;  // Output
					// Find point type: vertex, edge point, interior point
					if (i == N || j == N || k == N) {  // Vertex
						// Vertex point in initial triangle
						vertex = faceVertices[(i + 2 * j) / N];
					} else if (i == 0 || j == 0 || k == 0) {  // Edge
						// Find edge vertices
						int v0, v1, pos;
						// Find edge
						if (i == 0) {
							v0 = faceVertices[0];
							v1 = faceVertices[2];
							pos = j;
						} else if (j == 0) {
							v0 = faceVertices[0];
							v1 = faceVertices[1];
							pos = i;
						} else if (k == 0) {
							v0 = faceVertices[1];
							v1 = faceVertices[2];
							pos = j;
						}
						int edge = topology.FindEdge(v0, v1);
						auto edgeVertices = topology.GetEdgeVertices(edge);
						if (v0 != edgeVertices[0]) {
							pos = N - pos;
							std::swap(v0, v1);
						}
						vertex = numCoarseVertices + edge * numEdgeInteriorPoints + (pos - 1);

					} else { // Interior point
						int idx = offset
							+ numTriangleInteriorPoints * f
							+ numTriangleInteriorPoints - (N - j) * (N - j - 1) / 2
							+ i - 1;
						// Create position
						tessCoords[idx] = LocalCoords(f, i, j, N);
						// Record point in map
						vertex = idx;
					}

					pointMap.push_back(vertex);
				}  // ~for j
			}  // ~for i

			// Create triangles

			// Lambda to get a point in the pointMap, given (i, j)
			auto pnt = [N, &pointMap](int i, int j) {
				int idx = (2 * N + 3 - j) * j / 2 + i;
				return pointMap[idx];
			};

			int idxTri = f * N * N;

			// Up triangles
			for (int j = 0; j < N; ++j) {
				for (int i = 0; i < N - j; ++i, ++idxTri) {
					tessTris[idxTri] = Triangle(pnt(i, j), pnt(i + 1, j), pnt(i, j + 1));
					//tessTris[idxTri] = Triangle(pnt(i, j), pnt(i, j +1), pnt(i+1, j));
				}
			}
			// Down triangles
			for (int j = 0; j < N; ++j) {
				for (int i = 0; i < N - j - 1; ++i, ++idxTri) {
					tessTris[idxTri] = Triangle(pnt(i, j + 1), pnt(i + 1, j), pnt(i + 1, j + 1));
					//tessTris[idxTri] = Triangle(pnt(i, j + 1), pnt(i + 1, j + 1), pnt(i + 1, j));
				}
			}

		}  // ~for face

		return std::make_tuple(std::move(tessCoords), std::move(tessTris), numPoints, numTriangles);

	}  // ~Tessellate


	template <typename T>
	InterpolatedValues Interpolate(std::span<T> baseValues) const {
		int numBaseValues = baseValues.size();
		assert(numBaseValues == refiner->GetLevel(0).GetNumVertices());

		// Set dimensions
		int stride = sizeof(T) / sizeof(float);
		int numRegularRefinedValues = refiner->GetNumVerticesTotal();
		int numLocalRefinedValues   = patchTable->GetNumLocalPoints();
		int numAllRefinedValues = numRegularRefinedValues + numLocalRefinedValues;

		// Allocate output
		std::unique_ptr<T[]> refinedValues(new T[numAllRefinedValues]);

		// Copy base values in refined values
		std::memcpy(&refinedValues.get()[0], &baseValues[0], numBaseValues * sizeof(T));

		// Interpolate on refined vertices and local points (Tutorial 5_1)
		if (numRegularRefinedValues) {
			Far::PrimvarRefiner primvarRefiner(*refiner);  // TODO select refiner

			T * srcValues = refinedValues.get();
			for (int level = 1; level < refiner->GetNumLevels(); ++level) {
				T * dstValues = srcValues + refiner->GetLevel(level - 1).GetNumVertices();
				primvarRefiner.Interpolate(level, srcValues, dstValues);
				srcValues = dstValues;
			}
		}
		if (numLocalRefinedValues) {
			patchTable->GetLocalPointStencilTable()->UpdateValues(
				&refinedValues[0],
				&refinedValues[numRegularRefinedValues]
			);
		}

		return InterpolatedValues(
			(float*) refinedValues.release(),
			numAllRefinedValues,
			stride
		);
	}

	// Get number of positions after N-tessellation
	int getTesselPosCount(int N) const {
		const auto& topology = refiner->GetLevel(0);

		// Number of interior points in an edge
		const int numEdgeInteriorPoints = N - 1;

		// Number of interior points in a triangle
		const int numTriangleInteriorPoints = (N - 1) * (N - 2) / 2;

		// Number of coords to be computed
		const int numCoords =
			topology.GetNumVertices()
			+ numEdgeInteriorPoints * topology.GetNumEdges()
			+ numTriangleInteriorPoints * topology.GetNumFaces();

		return numCoords;
	}

	// Get number of triangles after N-tessellation
	int getTesselTriangleCount(int N) const {
		const auto& topology = refiner->GetLevel(0);
		return topology.GetNumFaces() * N * N;
	}

	///
	/// Evaluate positions and normals at tessellated mesh coordinates
	///
	/// @param interpolatedValues Values of the positions at the vertices
	///							  of the limit surface
	///
	///	@param tessCoords Coordinates on the tessellated mesh where to evaluate Positions
	///
	///	@return A smart pointer to a buffer containing the evaluated positions
	///	and a smart pointer to a buffer containing the evaluated normals
	///
	std::tuple<VertexBuffer, NormalBuffer>
	EvaluatePositions(
		const InterpolatedValues& interpolatedPositions,
		const CoordVector& tessCoords
	) const {

		int numCoords = tessCoords.size();

		// Allocate output structure
		VertexBuffer tessPositions(numCoords);
		NormalBuffer tessNormals(numCoords);

		const auto& topology = refiner->GetLevel(0);

		const Point* positions = interpolatedPositions.Get<Point>();

		// Evaluate positions and derivatives
		#pragma omp parallel for
		for (int vertex = 0; vertex < tessCoords.size(); ++vertex) {
			// Init
			auto& coords = tessCoords[vertex];

			// Translate in ptex face
			int ptexFace = ptexIndices->GetFaceId(coords.face);

			//  Locate the patch corresponding to the face ptex idx and (s,t)
			//  and evaluate:
			Far::PatchTable::PatchHandle const * handle =
				patchMap->FindPatch(ptexFace, coords.x, coords.y);
			assert(handle);

			float pWeights[20];
			float duWeights[20];
			float dvWeights[20];
			patchTable->EvaluateBasis(
				*handle, coords.x, coords.y,
				pWeights, duWeights, dvWeights
			);

			//  Identify the patch cvs and combine with the evaluated weights --
			//  remember to distinguish cvs in the base level:
			Far::ConstIndexArray cvIndices = patchTable->GetPatchVertices(*handle);

			// Evaluate position and derivatives
			Point& pos = tessPositions[vertex];
			pos.Clear();

			Vector du;
			du.Clear();

			Vector dv;
			dv.Clear();

			for (int cv = 0; cv < cvIndices.size(); ++cv) {
				int cvIndex = cvIndices[cv];

				const Point& position = positions[cvIndex];

				pos.AddWithWeight(position, pWeights[cv]);
				du.AddWithWeight(position, duWeights[cv]);
				dv.AddWithWeight(position, dvWeights[cv]);
			}

			// Update normal
			tessNormals[vertex] = Normal(Cross(du, dv));

			// Check validity of normal and normalize if ok
			auto& t = tessNormals[vertex];
			float length = t.Length();

			if (length != 0.f) {
				t /= length;
			} else {
				SDL_LOG(
					"Subdivision (enhanced) - Vertex #" << vertex << ", "
					<< "uv = (" << coords.x << ", " << coords.y << "), "
					<< "du = (" << du[0] << ", " << du[1] << ", " << du[2] << "), "
					<< "dv = (" << dv[0] << ", " << dv[1] << ", " << dv[2] << ")"
				);
				SDL_LOG("Subdivision (enhanced) - WARNING - "
					<< "Null normal (vect(du, dv) == 0)"
				);
			}


		}  // ~for vertex

		return std::make_tuple(std::move(tessPositions), std::move(tessNormals));
	}

	///
	/// Evaluate primvar data at tessellated mesh coordinates
	///
	/// @param interpolatedValues Values of the data to be evaluated at the vertices
	///							  of the control surface
	///
	///	@param tessCoords Coordinates on the tessellated mesh where to evaluate data
	///
	///	@return A smart pointer to a buffer containing the evaluated data
	///
	template<typename T>
	std::unique_ptr<T[]> Evaluate(
		const InterpolatedValues& interpolatedValues,
		const CoordVector& tessCoords
	) const {

		int numCoords = tessCoords.size();

		// Allocate output structure
		auto tessValues = std::unique_ptr<T[]>(new T[numCoords]);

		const auto& topology = refiner->GetLevel(0);

		auto inputValues = interpolatedValues.Get<T>();

		// Evaluate
		#pragma omp parallel for
		for (int vertex = 0; vertex < tessCoords.size(); ++vertex) {
			// Init
			auto& coords = tessCoords[vertex];

			// Translate in ptex face
			int ptexFace = ptexIndices->GetFaceId(coords.face);

			//  Locate the patch corresponding to the face ptex idx and (s,t)
			//  and evaluate:
			Far::PatchTable::PatchHandle const * handle =
				patchMap->FindPatch(ptexFace, coords.x, coords.y);
			assert(handle);

			float pWeights[20];
			patchTable->EvaluateBasis(*handle, coords.x, coords.y, pWeights);

			//  Identify the patch cvs and combine with the evaluated weights --
			//  remember to distinguish cvs in the base level:
			Far::ConstIndexArray cvIndices = patchTable->GetPatchVertices(*handle);

			// Evaluate
			T& val = tessValues[vertex];

			val.Clear();

			for (int cv = 0; cv < cvIndices.size(); ++cv) {
				int cvIndex = cvIndices[cv];

				val.AddWithWeight(inputValues[cvIndex], pWeights[cv]);
			}

		}  // ~for vertex

		return tessValues;
	}


	// Property members
	// "We're all consenting adults here", so those members will be left public
	TopologyRefinerPtr refiner;
	PtexIndicesPtr ptexIndices;
	Far::PatchTableFactory::Options patchTableOptions;
	PatchTablePtr patchTable;
	PatchMapPtr patchMap;

};

// Helper to evaluate multi-layers data, like UV, Colors, Gamma, AOV.
// EXTRACTOR is a callable type providing the coarse data for a given layer.
struct MultiLayerDataEvaluator{

	// Constructor
	MultiLayerDataEvaluator(
		const Surface& p_surface,
		u_int p_layerSize,
		const CoordVector& p_coords
	):
		surface(p_surface),
		layerSize(p_layerSize),
		coords(p_coords)
	{}

	// Return type for evaluation
	template <typename DATA_BUFFER, typename DATA_OUT>
	struct EvaluatedLayers: std::vector<DATA_BUFFER> {

		using DATA_IN = DATA_BUFFER::element_type;

		// Return-object constructor
		EvaluatedLayers(size_t num_layers):
			std::vector<DATA_BUFFER>(num_layers)
		{}

		// Return-object releaser
		ExtMeshProp<DATA_OUT> releaseLayers() {
			for (size_t i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
				auto& layer = (*this)[i];
				auto layerPtr = std::shared_ptr<DATA_IN[]>(std::move(layer));
				outBuffers[i] = reinterpret_pointer_cast<DATA_OUT[]>(layerPtr);
			}
			return outBuffers;
		}
		// TODO
		//std::array<DATA_OUT *, EXTMESH_MAX_DATA_COUNT>* releaseLayers() {
			//for (size_t layer = 0; layer < EXTMESH_MAX_DATA_COUNT; ++layer) {
				//outBuffers[layer] = reinterpret_cast<DATA_OUT *>((*this)[layer].release());
			//}
			//return &outBuffers;
		//}

		ExtMeshProp<DATA_OUT> outBuffers;
		// TODO
		//std::array<DATA_OUT *, EXTMESH_MAX_DATA_COUNT> outBuffers;
	};


	// Evaluator
	template < typename EXTRACTOR >
	auto evaluate(EXTRACTOR getLayerData) {

		// Evaluated data are stored as smart pointers in a vector (one row per
		// layer), so that they are automatically freed if anything goes wrong
		// before the mesh is built. Each element of the vector is a buffer for a
		// layer. Eventually, the buffers have to be passed to ExtTriangleMesh
		// constructor, which takes ownership. In that way, they will be released.

		using DATA_IN = std::remove_pointer_t<std::invoke_result_t<EXTRACTOR, u_int>>;
		using DATA_BUFFER = std::unique_ptr<DATA_IN[]>;

		// Return float* if DATA is FloatAdapter
		// (for Alpha and AOV)
		using DATA_OUT = std::conditional_t<
			std::is_same_v<DATA_IN, FloatAdapter>, float, DATA_IN
		>;

		// Return object
		EvaluatedLayers<DATA_BUFFER, DATA_OUT> res(EXTMESH_MAX_DATA_COUNT);

		// Treatment
		for (size_t layer = 0; layer < EXTMESH_MAX_DATA_COUNT; ++layer) {
			auto* layerDataPtr = getLayerData(layer);
			if (layerDataPtr) {
				std::span<DATA_IN> layerDataSpan(layerDataPtr, layerSize);
				auto interpolatedData = surface.Interpolate(layerDataSpan);
				res[layer] = surface.Evaluate<DATA_IN>(interpolatedData, coords);
			}
		}
		return res;
	}

	const Surface& surface;
	const size_t layerSize;
	const CoordVector& coords;

};


// Entry point for enhanced subdivision
ExtTriangleMeshUPtr ApplySubdiv(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel,
	const float sharpnessThresholdRadians,
	const float creaseWeight
) {
	// Remarks:
	// All buffers (triangles, points, normals...) are enclosed in smart pointers
	// in order to guarantee that memory is released in case of exception arising
	// during whole process: please keep it so.
	//
	// Outputs values are returned as tuples, instead of using byref arguments.
	// This is for readability: arguments are inputs only, return values are output

	using std::tie;

	// Create limit surface from base geometry
	Surface surface(srcMesh, sharpnessThresholdRadians, creaseWeight);

	// Subdivide
	surface.Subdivide(maxLevel);

	// Compute tesselletion rate, from maxLevel
	size_t tessellationRate = 1 << maxLevel;

	// Tessellate
	CoordVector tessCoords;
	TriangleBuffer tessTriangles;
	int numPoints, numTriangles;

	tie(tessCoords, tessTriangles, numPoints, numTriangles) =
		surface.Tessellate(tessellationRate);

	// Record initial vertex count
	u_int numMeshVertex = srcMesh.GetTotalVertexCount();

	// Evaluate positions and normals
	VertexBuffer tessPoints;
	NormalBuffer tessNormals;
	{
		// Interpolate positions on subdivided surface
		auto interpolatedPositions =
			surface.Interpolate(srcMesh.GetVertices());

		// Evaluate refined (interpolated) positions
		SDL_LOG("Subdivision (enhanced) - Evaluating positions and normals");
		tie(tessPoints, tessNormals) =
			surface.EvaluatePositions(interpolatedPositions, tessCoords);
	}

	// Evaluate other primvars
	MultiLayerDataEvaluator evaluator(surface, numMeshVertex, tessCoords);

	// Evaluate UV
	auto extractorUV = [&srcMesh](u_int layer) { return srcMesh.GetUVs(layer).get(); };
	auto tessUVs = evaluator.evaluate(extractorUV);

	// Evaluate Colors
	auto extractorCols = [&srcMesh](u_int layer) { return srcMesh.GetColors(layer).get(); };
	auto tessCols = evaluator.evaluate(extractorCols);

	// Evaluate Alphas
	auto extractorAlphas = [&srcMesh](u_int layer)
		{ return (FloatAdapter*) srcMesh.GetAlphas(layer).get(); };
	auto tessAlphas = evaluator.evaluate(extractorAlphas);

	// Evaluate AOVs
	auto extractorAOVs = [&srcMesh](u_int layer)
		{ return (FloatAdapter*) srcMesh.GetVertexAOVs(layer).get(); };
	auto tessAOVs = evaluator.evaluate(extractorAOVs);

	// Allocate the new mesh and release buffers (transfer ownership to new
	// mesh)
	SDL_LOG(
		"Subdivision (enhanced) - Building new mesh: "
		<< numPoints << " points / "
		<< numTriangles << " triangles"
	);

	auto newMesh =  std::make_unique<ExtTriangleMesh>(
		std::move(tessPoints),
		std::move(tessTriangles),
		std::move(tessNormals),
		tessUVs.releaseLayers(),
		tessCols.releaseLayers(),
		tessAlphas.releaseLayers()
	);

	// Handle AOVs
	auto tessAOVs_released = tessAOVs.releaseLayers();
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		newMesh->SetVertexAOV(i, tessAOVs_released.GetLayerSpan(i));
	}

	return newMesh;

}

}  // ~namespace enhanced

}  // ~namespace (local)


//////////////////////////
//        API           //
//////////////////////////


SubdivShape::SubdivShape(
	CameraConstPtr camera,
	ExtTriangleMeshRef rawSrcMesh,
	const u_int maxLevel,
	const float maxEdgeScreenSize,
	const bool enhanced,
	const bool merge_before,
	const float sharpnessThresholdRadians,
	const float creaseWeight
) {
	const double startTime = WallClockTime();

	if ((maxEdgeScreenSize > 0.f) && !camera) {
		throw std::runtime_error(
			"The scene.GetCamera() must be defined in order to enable "
			"subdiv maxedgescreensize option"
		);
	}

	if (maxLevel > 0) {

		// Merge vertices on distance
		auto srcMeshPtr = merge_before ?
			MergeOnDistanceShape::ApplyMergeOnDistance(
				rawSrcMesh, std::numeric_limits<float>::min(), true
			) : nullptr;
		auto& srcMesh = merge_before ? *srcMeshPtr : rawSrcMesh;

		if (camera && (maxEdgeScreenSize > 0.f)) {
			SDL_LOG("Subdividing shape " << srcMesh.GetName()
					<< " - Adaptive subdivision");

			mesh = srcMesh.Copy();

			for (u_int i = 0; i < maxLevel; ++i) {
				SDL_LOG("Subdividing - Adaptive - Computing max edge size");
				// Check the size of the longest mesh edge on film image plane
				const float edgeScreenSize = MaxEdgeScreenSize(*camera, *mesh);


				if (edgeScreenSize <= maxEdgeScreenSize)
					break;

				SDL_LOG(
					"Subdivision - Adaptive - Step #" << i << " - "
					<< "Current maximum edge size on screen = "
					<< edgeScreenSize
					<< ", target = "
					<< maxEdgeScreenSize
				);


				int optimizedLevel =
					static_cast<int>(std::ceil(std::log2f(edgeScreenSize / maxEdgeScreenSize)));
				SDL_LOG("Subdividing adapted level = " << optimizedLevel);

				// Subdivide and re-try
				auto newMesh = ApplySubdiv(
					*mesh,
					optimizedLevel,
					enhanced,
					merge_before,
					sharpnessThresholdRadians,
					creaseWeight
				);
				SDL_LOG(
					"Done step #" << i
					<< " from " << mesh->GetTotalTriangleCount()
					<< " to " << newMesh->GetTotalTriangleCount() << " faces "
					<< "(level=" << optimizedLevel << ")"
				);

				// Replace old mesh with new one
				std::swap(mesh, newMesh);
			}
		} else {
			SDL_LOG("Subdividing shape " << srcMesh.GetName() << " at level: " << maxLevel);

			mesh = ApplySubdiv(srcMesh, maxLevel, enhanced, merge_before, sharpnessThresholdRadians, creaseWeight);
		}
	} else {
		// Nothing to do, just make a copy
		SDL_LOG("Subdivision: level=0 - Skipping");
		mesh = rawSrcMesh.Copy();
	}

	if (maxLevel > 0) {
		SDL_LOG("Subdivided shape from " << rawSrcMesh.GetTotalTriangleCount() << " to " << mesh->GetTotalTriangleCount() << " faces");
	}

	// For some debugging
	//mesh->Save("debug.ply");

	const double endTime = WallClockTime();
	SDL_LOG("Subdividing time: " << endTime - startTime << " secs");
}



float SubdivShape::MaxEdgeScreenSize(CameraConstRef camera, ExtTriangleMeshRef srcMesh) {
	const u_int triCount = srcMesh.GetTotalTriangleCount();
	const Points verts = srcMesh.GetVertices();
	const TriangleBuffer tris(srcMesh.GetTriangles());

	// Note VisualStudio doesn't support:
	//#pragma omp parallel for reduction(max:maxEdgeSize)

	const u_int threadCount = omp_get_max_threads();

	const Transform worldToScreen = Inverse(camera.GetScreenToWorld());

	std::vector<float> maxEdgeSizes(threadCount, 0.f);

	std::vector<Point> projectedPoints(srcMesh.GetTotalVertexCount());
	#pragma omp parallel for
	for(int i = 0; i < srcMesh.GetTotalVertexCount(); ++i) {
		projectedPoints[i] = worldToScreen * verts[i];
		projectedPoints[i] /= projectedPoints[i].z;
	}

	// Visual C++ 2013 supports only OpenMP 2.5, with int loop indices
	// At any rate, opensubdiv indices are of type 'int', so no regret...
	//
	// We use LengthSquared for max search, sparing one operation per iteration (sqrt)
	#pragma omp parallel for
	for(int i = 0; i < triCount; ++i) {
		const int tid = omp_get_thread_num();
		//const int tid = 0;

		const Triangle &tri = tris[i];
		const Point p0 = projectedPoints[tri.v[0]];
		const Point p1 = projectedPoints[tri.v[1]];
		const Point p2 = projectedPoints[tri.v[2]];

		std::array<float, 4> triMaxEdgeSize;
		triMaxEdgeSize[0] = (p1 - p0).LengthSquared();
		triMaxEdgeSize[1] = (p2 - p1).LengthSquared();
		triMaxEdgeSize[2] = (p0 - p2).LengthSquared();
		triMaxEdgeSize[3] = maxEdgeSizes[tid];

		maxEdgeSizes[tid] =
			*std::max_element(triMaxEdgeSize.begin(), triMaxEdgeSize.end());

	}

	float maxEdgeSize = *std::max_element(maxEdgeSizes.begin(), maxEdgeSizes.end());

	return sqrtf(maxEdgeSize);
}


ExtTriangleMeshUPtr SubdivShape::ApplySubdiv(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel,
	const bool enhanced,
	const bool merge_before,
	const float sharpnessThresholdRadians,
	const float creaseWeight
) {
	//--------------------------------------------------------------------------
	// Check data
	//--------------------------------------------------------------------------

	// OSD topology is indexed by int whereas Lux's one is indexed by uint...
	assert(srcMesh.GetTotalVertexCount() <= std::numeric_limits<int>::max());
	assert(srcMesh.GetTotalTriangleCount() <= std::numeric_limits<int>::max());

	if (enhanced) {
		SDL_LOG(
			"Subdivision (enhanced) - Starting - "
			<< srcMesh.GetTotalVertexCount() << " points / "
			<< srcMesh.GetTotalTriangleCount() << " triangles"
		);

		return enhanced::ApplySubdiv(
			srcMesh, maxLevel, sharpnessThresholdRadians, creaseWeight
		);
	}
	else {
		SDL_LOG("Subdivision - Starting - "
			<< srcMesh.GetTotalVertexCount() << " points / "
			<< srcMesh.GetTotalTriangleCount() << " triangles"
		);

		return simple::ApplySubdiv(srcMesh, maxLevel);
	}
}


SubdivShape::~SubdivShape() {
}

ExtTriangleMeshUPtr SubdivShape::RefineImpl(SceneConstRef scene) {
	return std::move(mesh);
}


// vim: autoindent noexpandtab tabstop=4 shiftwidth=4 foldmethod=syntax
