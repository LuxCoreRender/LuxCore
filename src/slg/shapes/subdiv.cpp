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
#include <opensubdiv/far/primvarRefiner.h>
#include <opensubdiv/osd/cpuPatchTable.h>
#include <opensubdiv/osd/cpuVertexBuffer.h>
#include <opensubdiv/far/ptexIndices.h>
#include <opensubdiv/osd/tbbEvaluator.h>
#define OSD_EVALUATOR Osd::TbbEvaluator

#include <omp.h>

#include "luxrays/core/exttrianglemesh.h"
#include "slg/shapes/subdiv.h"
#include "slg/shapes/tessellate.h"
#include "slg/shapes/subdiv_envelope.h"
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
                             float sharpnessThresholdRadians);

// Build the edge map (with sharpness / crease weight) according to the
// crease mode:
// - NONE: empty map (no creases)
// - COMPUTED: dihedral angles vs sharpnessThresholdRadians, uniform creaseWeight
// - IMPORTED: crease pairs stored on the mesh (PLY edge element or
//   DefineMeshExt crease_pairs/crease_weights), with their own sharpness
//   weights; falls back to NONE when the mesh carries no crease data
static void buildCreaseEdgeMap(
	const ExtTriangleMesh &mesh,
	const SubdivCreaseMode creaseMode,
	const float sharpnessThresholdRadians,
	const float creaseWeight,
	std::map<std::pair<int, int>, EdgeInfo> &edgeMap
) {
	switch (creaseMode) {
		case SubdivCreaseMode::NONE:
			edgeMap.clear();
			break;
		case SubdivCreaseMode::COMPUTED:
			processEdgesForSharpness(mesh, edgeMap, sharpnessThresholdRadians);
			// Apply the uniform crease weight to the sharp edges
			for (auto &entry : edgeMap) {
				if (entry.second.sharpness > 0.5f)
					entry.second.creaseWeight = creaseWeight;
			}
			break;
		case SubdivCreaseMode::IMPORTED: {
			edgeMap.clear();
			const auto &pairs = mesh.GetCreasePairs();
			const auto &weights = mesh.GetCreaseWeights();
			for (size_t i = 0; i < weights.size(); ++i) {
				const int v0 = static_cast<int>(pairs[2 * i]);
				const int v1 = static_cast<int>(pairs[2 * i + 1]);
				EdgeInfo edgeInfo(v0, v1);
				edgeInfo.sharpness = 1.0f;
				edgeInfo.creaseWeight = weights[i];
				edgeMap.emplace(std::make_pair(std::min(v0, v1), std::max(v0, v1)), edgeInfo);
			}
			break;
		}
	}
}

// Process edges to determine sharpness based on dihedral angles
void processEdgesForSharpness(const ExtTriangleMesh &mesh,
                             std::map<std::pair<int, int>, EdgeInfo> &edgeMap,
                             float sharpnessThresholdRadians) {

	auto triangles = mesh.GetTriangles();
	auto vertices = mesh.GetVertices();

    // Clear any existing edge information
    edgeMap.clear();

    // Build edge-to-triangle adjacency map in a single pass over triangles.
    // Key: canonical edge (v0 < v1). Value: indices of adjacent triangles.
    // This is O(T) instead of the previous O(E*T) approach which scanned all
    // triangles for each edge.
    std::map<std::pair<int, int>, std::vector<size_t>> edgeToTris;

    for (size_t triIndex = 0; triIndex < triangles.size(); ++triIndex) {
        const Triangle &tri = triangles[triIndex];
        for (int i = 0; i < 3; ++i) {
            int v0 = tri.v[i];
            int v1 = tri.v[(i + 1) % 3];
            auto edgeKey = std::make_pair(std::min(v0, v1), std::max(v0, v1));
            edgeToTris[edgeKey].push_back(triIndex);
        }
    }

    // Calculate sharpness for each edge using the adjacency map (O(E))
    for (const auto &entry : edgeToTris) {
        const auto &edgeKey = entry.first;
        const auto &adjacentTris = entry.second;

        int v0 = edgeKey.first;
        int v1 = edgeKey.second;

        EdgeInfo edgeInfo(v0, v1);

        // If edge is on boundary (only one adjacent triangle), mark as sharp
        if (adjacentTris.size() == 1) {
            edgeInfo.sharpness = 1.0f;
            edgeInfo.creaseWeight = 1.0f;
        }
        // For internal edges, calculate dihedral angle
        else if (adjacentTris.size() >= 2) {
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

        edgeMap.emplace(edgeKey, edgeInfo);
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
		if (edge.sharpness > 0.5f) {
			creases.push_back(edge.v0);
			creases.push_back(edge.v1);
			creaseWeights.push_back(edge.creaseWeight);
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
		const float creaseWeight,
		const SubdivCreaseMode creaseMode
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
		buildCreaseEdgeMap(srcMesh, creaseMode, sharpnessThresholdRadians, creaseWeight, edgeMap);

		// Construct refiner
		refiner = std::move(
			createTopologyAdaptiveRefiner(
				reinterpret_cast<const int *>(srcMesh.GetTrianglesAsInts().data()),
				srcMesh.GetTotalVertexCount(),
				srcMesh.GetTotalTriangleCount(),
				edgeMap,
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
	/// Tessellate input mesh: delegates to the standalone tessellation
	/// (TessellateShape API), called directly on this refiner
	std::tuple<CoordVector, TriangleBuffer, int, int>
	Tessellate (const size_t N) {
		return TessellateTopology(*refiner, N);
	}


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
		auto refinedValues = std::make_unique<T[]>(numAllRefinedValues);

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
		return GetTessellationPosCount(refiner->GetLevel(0), N);
	}

	// Get number of triangles after N-tessellation
	int getTesselTriangleCount(int N) const {
		return GetTessellationTriangleCount(refiner->GetLevel(0), N);
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
		#pragma omp parallel
		{
			// Per-thread weight buffers, resized to the patch CV count as needed
			std::vector<float> pWeights, duWeights, dvWeights;

			#pragma omp for
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

				//  Identify the patch cvs and combine with the evaluated weights --
				//  remember to distinguish cvs in the base level:
				Far::ConstIndexArray cvIndices = patchTable->GetPatchVertices(*handle);
				const int numCVs = cvIndices.size();

				pWeights.resize(numCVs);
				duWeights.resize(numCVs);
				dvWeights.resize(numCVs);
				patchTable->EvaluateBasis(
					*handle, coords.x, coords.y,
					pWeights.data(), duWeights.data(), dvWeights.data()
				);

				// Evaluate position and derivatives
				Point& pos = tessPositions[vertex];
				pos.Clear();

				Vector du;
				du.Clear();

				Vector dv;
				dv.Clear();

				for (int cv = 0; cv < numCVs; ++cv) {
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
		}  // ~omp parallel

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
		auto tessValues = std::make_unique<T[]>(numCoords);

		const auto& topology = refiner->GetLevel(0);

		auto inputValues = interpolatedValues.Get<T>();

		// Evaluate
		#pragma omp parallel
		{
			// Per-thread weight buffer, resized to the patch CV count as needed
			std::vector<float> pWeights;

			#pragma omp for
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

				//  Identify the patch cvs and combine with the evaluated weights --
				//  remember to distinguish cvs in the base level:
				Far::ConstIndexArray cvIndices = patchTable->GetPatchVertices(*handle);
				const int numCVs = cvIndices.size();

				pWeights.resize(numCVs);
				patchTable->EvaluateBasis(*handle, coords.x, coords.y, pWeights.data());

				// Evaluate
				T& val = tessValues[vertex];

				val.Clear();

				for (int cv = 0; cv < numCVs; ++cv) {
					int cvIndex = cvIndices[cv];

					val.AddWithWeight(inputValues[cvIndex], pWeights[cv]);
				}

			}  // ~for vertex
		}  // ~omp parallel

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
		EvaluatedLayers(size_t num_layers, size_t data_size):
			std::vector<DATA_BUFFER>(num_layers),
			dataSize(data_size)
		{}

		// Return-object releaser
		ExtMeshProp<DATA_OUT> releaseLayers() {
			for (size_t i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
				auto& layer = (*this)[i];
				if (!layer)
					continue;
				if constexpr (std::is_same_v<DATA_IN, FloatAdapter>) {
					// Convert FloatAdapter[] to float[] by copy
					auto floatLayer = std::make_shared<float[]>(dataSize);
					for (size_t j = 0; j < dataSize; ++j)
						floatLayer[j] = layer[j].x;
					outBuffers.SetLayer(i, std::move(floatLayer), dataSize);
				} else {
					auto layerPtr = std::shared_ptr<DATA_IN[]>(std::move(layer));
					outBuffers.SetLayer(i, std::move(layerPtr), dataSize);
				}
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
		size_t dataSize;
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

		using RAW_DATA = std::remove_pointer_t<std::invoke_result_t<EXTRACTOR, u_int>>;

		// Float data (alphas, AOVs) need FloatAdapter for the subdivision
		// evaluator, which requires Clear()/AddWithWeight()
		using DATA_IN = std::conditional_t<
			std::is_same_v<RAW_DATA, float>, FloatAdapter, RAW_DATA
		>;
		using DATA_BUFFER = std::unique_ptr<DATA_IN[]>;
		using DATA_OUT = RAW_DATA;

		// Return object
		EvaluatedLayers<DATA_BUFFER, DATA_OUT> res(EXTMESH_MAX_DATA_COUNT, coords.size());

		// Treatment
		for (size_t layer = 0; layer < EXTMESH_MAX_DATA_COUNT; ++layer) {
			auto* rawDataPtr = getLayerData(layer);
			if (rawDataPtr) {
				if constexpr (std::is_same_v<RAW_DATA, float>) {
					// Copy float data into FloatAdapter array
					auto adapterData = std::make_unique<FloatAdapter[]>(layerSize);
					for (size_t j = 0; j < layerSize; ++j)
						adapterData[j] = FloatAdapter(rawDataPtr[j]);
					std::span<FloatAdapter> layerDataSpan(adapterData.get(), layerSize);
					auto interpolatedData = surface.Interpolate(layerDataSpan);
					res[layer] = surface.Evaluate<FloatAdapter>(interpolatedData, coords);
				} else {
					std::span<RAW_DATA> layerDataSpan(rawDataPtr, layerSize);
					auto interpolatedData = surface.Interpolate(layerDataSpan);
					res[layer] = surface.Evaluate<RAW_DATA>(interpolatedData, coords);
				}
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
	const float creaseWeight,
	const SubdivCreaseMode creaseMode,
	std::vector<u_int> *triBaseFace
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
	Surface surface(srcMesh, sharpnessThresholdRadians, creaseWeight, creaseMode);

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

	// Optionally record, for each tessellated triangle, the face of the BASE
	// (level 0) topology it comes from (walking the refiner parent links).
	// Used to propagate per-face data (e.g. material slots) through subdivision.
	if (triBaseFace) {
		// TessellateTopology works on level 0 of the refiner, so the local
		// coords' face is already a BASE face index
		triBaseFace->resize(numTriangles);
		for (int t = 0; t < numTriangles; ++t)
			(*triBaseFace)[t] = tessCoords[tessTriangles[t].v[0]].face;
	}

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
	auto extractorAlphas = [&srcMesh](u_int layer) { return srcMesh.GetAlphas(layer).get(); };
	auto tessAlphas = evaluator.evaluate(extractorAlphas);

	// Evaluate AOVs
	auto extractorAOVs = [&srcMesh](u_int layer) { return srcMesh.GetVertexAOVs(layer).get(); };
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

	// Handle AOVs: only the layers evaluated from a source layer having values
	// (GetLayerSpan can not be used here, the evaluated buffers have their own
	// size)
	auto tessAOVs_released = tessAOVs.releaseLayers();
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		const auto layer = tessAOVs_released.GetLayer(i);
		if (layer)
			newMesh->SetVertexAOV(i, layer, size_t(numPoints));
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
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel,
	const float maxEdgeScreenSize,
	const float sharpnessThresholdRadians,
	const float creaseWeight,
	const SubdivCreaseMode creaseMode,
	const int matSlot
) :
	camera(camera),
	srcMesh(srcMesh),
	maxLevel(maxLevel),
	maxEdgeScreenSize(maxEdgeScreenSize),
	sharpnessThresholdRadians(sharpnessThresholdRadians),
	creaseWeight(creaseWeight),
	creaseMode(creaseMode),
	matSlot(matSlot)
{
	if ((maxEdgeScreenSize > 0.f) && !camera) {
		throw std::runtime_error(
			"The scene.GetCamera() must be defined in order to enable "
			"subdiv maxedgescreensize option"
		);
	}
}

ExtTriangleMeshUPtr SubdivShape::ApplySubdiv(
	ExtTriangleMeshRef srcMesh,
	const u_int maxLevel,
	const float sharpnessThresholdRadians,
	const float creaseWeight,
	const SubdivCreaseMode creaseMode,
	std::vector<u_int> *triBaseFace
) {
	//--------------------------------------------------------------------------
	// Check data
	//--------------------------------------------------------------------------

	// OSD topology is indexed by int whereas Lux's one is indexed by uint...
	assert(srcMesh.GetTotalVertexCount() <= std::numeric_limits<int>::max());
	assert(srcMesh.GetTotalTriangleCount() <= std::numeric_limits<int>::max());

	SDL_LOG(
		"Subdivision (enhanced) - Starting - "
		<< srcMesh.GetTotalVertexCount() << " points / "
		<< srcMesh.GetTotalTriangleCount() << " triangles"
	);

	return enhanced::ApplySubdiv(
		srcMesh, maxLevel, sharpnessThresholdRadians, creaseWeight, creaseMode,
		triBaseFace
	);
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


SubdivShape::~SubdivShape() {
}



ExtTriangleMeshUPtr SubdivShape::RefineImpl(SceneConstRef scene) {
	const double startTime = WallClockTime();

	// Watertight envelope over all submeshes sharing the source point buffer,
	// so shared edges stay interior during subdivision (no cracks/warping)
	auto envelope = BuildSubdivEnvelope(scene, srcMesh);
	ExtTriangleMesh *src = envelope ? envelope.get() : &srcMesh;
	if (envelope) {
		SDL_LOG("Subdividing envelope " << envelope->GetName()
			<< " - " << envelope->GetTotalTriangleCount() << " triangles over "
			<< envelope->GetTotalVertexCount() << " shared points");
	}

	// Per-face material slots are propagated through subdivision only on the
	// uniform path (the refiner parent links are only valid for a single
	// subdivision of the envelope)
	std::vector<u_int> triBaseFace;
	std::vector<u_int> *triBaseFacePtr = nullptr;
	if (envelope && !(camera && (maxEdgeScreenSize > 0.f)))
		triBaseFacePtr = &triBaseFace;

	if (maxLevel > 0) {
		if (camera && (maxEdgeScreenSize > 0.f)) {
			SDL_LOG("Subdividing shape " << src->GetName()
					<< " - Adaptive subdivision");

			ExtTriangleMeshUPtr work = src->Copy();

			for (u_int i = 0; i < maxLevel; ++i) {
				SDL_LOG("Subdividing - Adaptive - Computing max edge size");
				const float edgeScreenSize = MaxEdgeScreenSize(*camera, *work);

				if (edgeScreenSize <= maxEdgeScreenSize)
					break;

				SDL_LOG(
					"Subdivision - Adaptive - Step #" << i << " - "
					<< "Current maximum edge size on screen = "
					<< edgeScreenSize
					<< ", target = " << maxEdgeScreenSize
				);

				int optimizedLevel =
					static_cast<int>(std::ceil(std::log2f(edgeScreenSize / maxEdgeScreenSize)));

				// Cap optimizedLevel to avoid excessive tessellation.
				const u_int faceCount = work->GetTotalTriangleCount();
				const double maxNInt = std::sqrt(
					static_cast<double>(std::numeric_limits<int>::max()) / faceCount
				);
				const int maxIntLevel = static_cast<int>(std::log2(maxNInt));
				const double maxNPractical = std::sqrt(50.0e6 / faceCount);
				const int maxPracticalLevel = static_cast<int>(std::log2(maxNPractical));
				const int maxSafeLevel = std::min(maxIntLevel, maxPracticalLevel);
				if (optimizedLevel > maxSafeLevel) {
					SDL_LOG(
						"Subdividing adapted level capped from " << optimizedLevel
						<< " to " << maxSafeLevel
						<< " (face count = " << faceCount << ")"
					);
					optimizedLevel = maxSafeLevel;
				}
				if (optimizedLevel <= 0) {
					SDL_LOG("Subdivision - cannot subdivide further "
						"(face count limit reached)");
					break;
				}
				SDL_LOG("Subdividing adapted level = " << optimizedLevel);

				auto newMesh = ApplySubdiv(
					*work,
					optimizedLevel,
					sharpnessThresholdRadians,
					creaseWeight,
					creaseMode
				);
				SDL_LOG(
					"Done step #" << i
					<< " from " << work->GetTotalTriangleCount()
					<< " to " << newMesh->GetTotalTriangleCount() << " faces "
					<< "(level=" << optimizedLevel << ")"
				);

				std::swap(work, newMesh);
			}
			mesh = std::move(work);
		} else {
			SDL_LOG("Subdividing shape " << src->GetName() << " at level: " << maxLevel);

			mesh = ApplySubdiv(
				*src, maxLevel, sharpnessThresholdRadians, creaseWeight, creaseMode,
				triBaseFacePtr
			);
		}
	} else {
		// Nothing to do, just make a copy
		SDL_LOG("Subdivision: level=0 - Skipping");
		mesh = src->Copy();
	}

	// Split the subdivided envelope back per material slot
	if (envelope) {
		if (triBaseFacePtr)
			PropagateMaterialSlots(*envelope, triBaseFace, *mesh);
		else if (matSlot >= 0)
			SDL_LOG("Warning: per-material split (matslot) is only supported on "
				"the uniform subdivision path (no maxedgescreensize)");
		if (matSlot >= 0 && triBaseFacePtr)
			mesh = FilterByMaterialSlot(*mesh, matSlot);
	}

	SDL_LOG("Subdivided shape in " << WallClockTime() - startTime << " secs");

	return std::move(mesh);
}

// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
