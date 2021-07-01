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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "slg/scene/scene.h"
#include "slg/utils/filenameresolver.h"

#include "slg/shapes/meshshape.h"
#include "slg/shapes/pointiness.h"
#include "slg/shapes/strands.h"
#include "slg/shapes/groupshape.h"
#include "slg/shapes/subdiv.h"
#include "slg/shapes/displacement.h"
#include "slg/shapes/harlequinshape.h"
#include "slg/shapes/simplify.h"
#include "slg/shapes/islandaovshape.h"
#include "slg/shapes/randomtriangleaovshape.h"
#include "slg/shapes/edgedetectoraov.h"
#include "slg/shapes/bevelshape.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void Scene::ParseShapes(const Properties &props) {
	vector<string> shapeKeys = props.GetAllUniqueSubNames("scene.shapes");
	if (shapeKeys.size() == 0) {
		// There are not shape definitions
		return;
	}

	double lastPrint = WallClockTime();
	u_int shapeCount = 0;
	BOOST_FOREACH(const string &key, shapeKeys) {
		// Extract the shape name
		const string shapeName = Property::ExtractField(key, 2);
		if (shapeName == "")
			throw runtime_error("Syntax error in shape definition: " + shapeName);

		ExtTriangleMesh *mesh = CreateShape(shapeName, props);
		DefineMesh(mesh);
		++shapeCount;

		const double now = WallClockTime();
		if (now - lastPrint > 2.0) {
			SDL_LOG("Shape count: " << shapeCount);
			lastPrint = now;
		}
	}
	SDL_LOG("Shape count: " << shapeCount);

	editActions.AddActions(GEOMETRY_EDIT);
}

ExtTriangleMesh *Scene::CreateInlinedMesh(const string &shapeName, const string &propName, const Properties &props) {
	// The mesh definition is in-lined
	u_int pointsSize;
	Point *points;
	if (props.IsDefined(propName + ".vertices")) {
		Property prop = props.Get(propName + ".vertices");
		if ((prop.GetSize() == 0) || (prop.GetSize() % 3 != 0))
			throw runtime_error("Wrong shape vertex list length: " + shapeName);

		pointsSize = prop.GetSize() / 3;
		points = TriangleMesh::AllocVerticesBuffer(pointsSize);
		for (u_int i = 0; i < pointsSize; ++i) {
			const u_int index = i * 3;
			points[i] = Point(prop.Get<float>(index), prop.Get<float>(index + 1), prop.Get<float>(index + 2));
		}
	} else
		throw runtime_error("Missing shape vertex list: " + shapeName);

	u_int trisSize;
	Triangle *tris;
	if (props.IsDefined(propName + ".faces")) {
		Property prop = props.Get(propName + ".faces");
		if ((prop.GetSize() == 0) || (prop.GetSize() % 3 != 0))
			throw runtime_error("Wrong shape face list length: " + shapeName);

		trisSize = prop.GetSize() / 3;
		tris = TriangleMesh::AllocTrianglesBuffer(trisSize);
		for (u_int i = 0; i < trisSize; ++i) {
			const u_int index = i * 3;
			tris[i] = Triangle(prop.Get<u_int>(index), prop.Get<u_int>(index + 1), prop.Get<u_int>(index + 2));
		}
	} else {
		delete[] points;
		throw runtime_error("Missing shape face list: " + shapeName);
	}

	Normal *normals = NULL;
	if (props.IsDefined(propName + ".normals")) {
		Property prop = props.Get(propName + ".normals");
		if ((prop.GetSize() == 0) || (prop.GetSize() / 3 != pointsSize))
			throw runtime_error("Wrong shape normal list length: " + shapeName);

		normals = new Normal[pointsSize];
		for (u_int i = 0; i < pointsSize; ++i) {
			const u_int index = i * 3;
			normals[i] = Normal(prop.Get<float>(index), prop.Get<float>(index + 1), prop.Get<float>(index + 2));
		}
	}

	UV *uvs = NULL;
	if (props.IsDefined(propName + ".uvs")) {
		Property prop = props.Get(propName + ".uvs");
		if ((prop.GetSize() == 0) || (prop.GetSize() / 2 != pointsSize))
			throw runtime_error("Wrong shape uv list length: " + shapeName);

		uvs = new UV[pointsSize];
		for (u_int i = 0; i < pointsSize; ++i) {
			const u_int index = i * 2;
			uvs[i] = UV(prop.Get<float>(index), prop.Get<float>(index + 1));
		}
	}
	
	return new ExtTriangleMesh(pointsSize, trisSize, points, tris, normals, uvs);
}

ExtTriangleMesh *Scene::CreateShape(const string &shapeName, const Properties &props) {
	const string propName = "scene.shapes." + shapeName;

	// Get the shape type
	const string shapeType = props.Get(Property(propName + ".type")("mesh")).Get<string>();

	// Define the shape
	Shape *shape;
	if (shapeType == "mesh") {
		const string meshFileName = SLG_FileNameResolver.ResolveFile(props.Get(Property(propName + ".ply")("")).Get<string>());

		MeshShape *meshShape = new MeshShape(meshFileName);

		if (props.IsDefined(propName + ".transformation")) {
			// Apply the transformation
			const Matrix4x4 mat = props.Get(Property(propName +
					".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			meshShape->ApplyTransform(Transform(mat));
		} else {
			const Matrix4x4 mat = props.Get(Property(propName +
				".appliedtransformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			meshShape->SetLocal2World(Transform(mat));
		}

		shape = meshShape;
	} else if (shapeType == "inlinedmesh") {
		MeshShape *meshShape = new MeshShape(CreateInlinedMesh(shapeName, propName, props));
		
		if (props.IsDefined(propName + ".transformation")) {
			// Apply the transformation
			const Matrix4x4 mat = props.Get(Property(propName +
					".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			meshShape->ApplyTransform(Transform(mat));
		} else {
			const Matrix4x4 mat = props.Get(Property(propName +
				".appliedtransformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
			meshShape->SetLocal2World(Transform(mat));
		}

		shape = meshShape;
	} else if (shapeType == "pointiness") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a pointiness shape: " + shapeName);

		const u_int aovIndex = props.Get(Property(propName + ".aovindex")(NULL_INDEX)).Get<u_int>();
		if ((aovIndex != NULL_INDEX) && (aovIndex >= EXTMESH_MAX_DATA_COUNT))
			throw runtime_error("Pointiness aov index must be less than: " + ToString(EXTMESH_MAX_DATA_COUNT));
		
		shape = new PointinessShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName), aovIndex);
	} else if (shapeType == "strands") {
		const string fileName = SLG_FileNameResolver.ResolveFile(props.Get(Property(propName + ".file")("strands.hair")).Get<string>());

		// For compatibility with the past
		string tessellationTypeStr = props.Get(Property(propName + ".tesselation.type")("ribbon")).Get<string>();
		tessellationTypeStr = props.Get(Property(propName + ".tessellation.type")(tessellationTypeStr)).Get<string>();

		StrendsShape::TessellationType tessellationType;
		if (tessellationTypeStr == "ribbon")
			tessellationType = StrendsShape::TESSEL_RIBBON;
		else if (tessellationTypeStr == "ribbonadaptive")
			tessellationType = StrendsShape::TESSEL_RIBBON_ADAPTIVE;
		else if (tessellationTypeStr == "solid")
			tessellationType = StrendsShape::TESSEL_SOLID;
		else if (tessellationTypeStr == "solidadaptive")
			tessellationType = StrendsShape::TESSEL_SOLID_ADAPTIVE;
		else
			throw runtime_error("Tessellation type unknown: " + tessellationTypeStr);

		// For compatibility with the past
		u_int adaptiveMaxDepth = Max(0, props.Get(Property(propName + ".tesselation.adaptive.maxdepth")(8)).Get<int>());
		adaptiveMaxDepth = Max(0, props.Get(Property(propName + ".tessellation.adaptive.maxdepth")(adaptiveMaxDepth)).Get<int>());
		// For compatibility with the past
		float adaptiveError = props.Get(Property(propName + ".tesselation.adaptive.error")(.1f)).Get<float>();
		adaptiveError = props.Get(Property(propName + ".tessellation.adaptive.error")(adaptiveError)).Get<float>();

		// For compatibility with the past
		u_int solidSideCount = Max(0, props.Get(Property(propName + ".tesselation.solid.sidecount")(3)).Get<int>());
		solidSideCount = Max(0, props.Get(Property(propName + ".tessellation.solid.sidecount")(solidSideCount)).Get<int>());
		// For compatibility with the past
		bool solidCapBottom = props.Get(Property(propName + ".tesselation.solid.capbottom")(false)).Get<bool>();
		solidCapBottom = props.Get(Property(propName + ".tessellation.solid.capbottom")(solidCapBottom)).Get<bool>();
		// For compatibility with the past
		bool solidCapTop = props.Get(Property(propName + ".tesselation.solid.captop")(false)).Get<bool>();
		solidCapTop = props.Get(Property(propName + ".tessellation.solid.captop")(solidCapTop)).Get<bool>();
		
		// For compatibility with the past
		bool useCameraPosition = props.Get(Property(propName + ".tesselation.usecameraposition")(false)).Get<bool>();
		useCameraPosition = props.Get(Property(propName + ".tessellation.usecameraposition")(useCameraPosition)).Get<bool>();

		cyHairFile strandsFile;
		const int strandsCount = strandsFile.LoadFromFile(fileName.c_str());
		if (strandsCount <= 0)
			throw runtime_error("Unable to read strands file: " + fileName);

		shape = new StrendsShape(this,
				&strandsFile, tessellationType,
				adaptiveMaxDepth, adaptiveError,
				solidSideCount, solidCapBottom, solidCapTop,
				useCameraPosition);
	} else if (shapeType == "group") {
		vector<string> shapeNamesKeys = props.GetAllUniqueSubNames(propName, true);
		if (shapeNamesKeys.size() > 0) {
			vector<const ExtTriangleMesh *> meshes;
			vector<Transform> trans;
			for (auto const &shapeNamesKey : shapeNamesKeys) {
				// Extract the index
				const string shapeNamesNumberStr = Property::ExtractField(shapeNamesKey, 3);
				if (shapeNamesNumberStr == "")
					throw runtime_error("Syntax error in group shape definition: " + shapeName);

				const string prefix = propName+ "." + shapeNamesNumberStr;

				const string name = props.Get(Property(prefix + ".shape")("shape")).Get<string>();
				if (!extMeshCache.IsExtMeshDefined(name))
					throw runtime_error("Unknown shape name in a group shape " + shapeName + ": " + name);

				meshes.push_back((const ExtTriangleMesh *)extMeshCache.GetExtMesh(name));

				if (props.IsDefined(prefix + ".transformation")) {
					const Matrix4x4 mat = props.Get(Property(prefix +
							".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
					trans.push_back(Transform(mat));
				} else
					trans.push_back(Transform(Matrix4x4::MAT_IDENTITY));
			}

			shape = new GroupShape(meshes, trans);
		} else
			throw runtime_error("A shape group can not be empty: " + shapeName);
	} else if (shapeType == "subdiv") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a subdiv shape: " + shapeName);
		
		const u_int maxLevel = props.Get(Property(propName + ".maxlevel")(2)).Get<u_int>();
		const float maxEdgeScreenSize = Max(props.Get(Property(propName + ".maxedgescreensize")(0.f)).Get<float>(), 0.f);

		shape = new SubdivShape(camera, (ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName),
				maxLevel, maxEdgeScreenSize);
	} else if (shapeType == "displacement") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a displacement shape: " + shapeName);
		
		const Texture *tex = GetTexture(props.Get(Property(propName + ".map")(0.f)));
		
		DisplacementShape::Params params;

		const string vectorSpaceStr = props.Get(Property(propName + ".map.type")("height")).Get<string>();
		if (vectorSpaceStr == "height")
			params.mapType = DisplacementShape::HIGHT_DISPLACEMENT;
		else if (vectorSpaceStr == "vector")
			params.mapType = DisplacementShape::VECTOR_DISPLACEMENT;
		else
			throw runtime_error("Unknown map type in displacement shape: " + shapeName);

		// Blender standard: R is an offset along
		// the tangent, G along the normal and B along the bitangent.
		// So map.channels = 2 0 1
		//
		// Mudbox standard: map.channels = 0 2 1
		const Property defaultPropChannels = Property(propName + ".map.channels")(2u, 0u, 1u);
		const Property propChannels = props.Get(defaultPropChannels);
		if (propChannels.GetSize() != 3)
			throw runtime_error("Wrong number of map channel indices in a displacement shape: " + shapeName);
		params.mapChannels[0] = Min(propChannels.Get<u_int>(0), 2u);
		params.mapChannels[1] = Min(propChannels.Get<u_int>(1), 2u);
		params.mapChannels[2] = Min(propChannels.Get<u_int>(2), 2u);

		params.scale = props.Get(Property(propName + ".scale")(1.f)).Get<float>();
		params.offset = props.Get(Property(propName + ".offset")(0.f)).Get<float>();
		params.uvIndex = props.Get(Property(propName + ".uvindex")(0)).Get<u_int>();
		params.normalSmooth = props.Get(Property(propName + ".normalsmooth")(true)).Get<bool>();

		shape = new DisplacementShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName),
				*tex, params);
	} else if (shapeType == "harlequin") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a harlequin shape: " + shapeName);
		
		shape = new HarlequinShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName));
	} else if (shapeType == "simplify") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a simplify shape: " + shapeName);
		
		const float target = props.Get(Property(propName + ".target")(.25f)).Get<float>();
		const float edgeScreenSize = Clamp(props.Get(Property(propName + ".edgescreensize")(0.f)).Get<float>(), 0.f, 1.f);
		const bool preserveBorder = props.Get(Property(propName + ".preserveborder")(false)).Get<bool>();
		
		shape = new SimplifyShape(camera, (ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName),
				target, edgeScreenSize, preserveBorder);
	} else if (shapeType == "islandaov") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a islandaov shape: " + shapeName);

		const u_int dataIndex = Clamp(props.Get(Property(propName + ".dataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		
		shape = new IslandAOVShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName), dataIndex);
	} else if (shapeType == "randomtriangleaov") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a randomtriangleaov shape: " + shapeName);

		const u_int srcDataIndex = Clamp(props.Get(Property(propName + ".srcdataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);
		const u_int dstDataIndex = Clamp(props.Get(Property(propName + ".dstdataindex")(0u)).Get<u_int>(), 0u, EXTMESH_MAX_DATA_COUNT);

		shape = new RandomTriangleAOVShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName),
				srcDataIndex, dstDataIndex);
	} else if (shapeType == "edgedetectoraov") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a edgedetectoraov shape: " + shapeName);

		const u_int aovIndex0 = props.Get(Property(propName + ".aovindex0")(0)).Get<u_int>();
		if (aovIndex0 >= EXTMESH_MAX_DATA_COUNT)
			throw runtime_error("Edgedetectoraov aov index 0 must be less than: " + ToString(EXTMESH_MAX_DATA_COUNT));

		const u_int aovIndex1 = props.Get(Property(propName + ".aovindex1")(1)).Get<u_int>();
		if (aovIndex1 >= EXTMESH_MAX_DATA_COUNT)
			throw runtime_error("Edgedetectoraov aov index 1 must be less than: " + ToString(EXTMESH_MAX_DATA_COUNT));

		const u_int aovIndex2 = props.Get(Property(propName + ".aovindex2")(2)).Get<u_int>();
		if (aovIndex2 >= EXTMESH_MAX_DATA_COUNT)
			throw runtime_error("Edgedetectoraov aov index 2 must be less than: " + ToString(EXTMESH_MAX_DATA_COUNT));

		shape = new EdgeDetectorAOVShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName),
				aovIndex0, aovIndex1, aovIndex2);
	} else if (shapeType == "bevel") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a bevel shape: " + shapeName);

		const float geometryBevel = props.Get(Property(propName + ".bevel.radius")(0.f)).Get<float>();

		shape = new BevelShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName), geometryBevel);
	} else
		throw runtime_error("Unknown shape type: " + shapeType);

	ExtTriangleMesh *mesh = shape->Refine(this);
	delete shape;
	mesh->SetName(shapeName);

	return mesh;
}
