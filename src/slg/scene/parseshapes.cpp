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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "slg/scene/scene.h"

#include "slg/shapes/meshshape.h"
#include "slg/shapes/pointiness.h"
#include "slg/shapes/strands.h"
#include "slg/shapes/opensubdiv.h"
#include "slg/shapes/harlequin.h"

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

		ExtMesh *mesh = CreateShape(shapeName, props);
		if (extMeshCache.IsExtMeshDefined(shapeName)) {
			// A replacement for an existing mesh
			const ExtMesh *oldMesh = extMeshCache.GetExtMesh(shapeName);

			// Replace old mesh direct references with new one and get the list
			// of scene objects referencing the old mesh
			boost::unordered_set<SceneObject *> modifiedObjsList;
			objDefs.UpdateMeshReferences(oldMesh, mesh, modifiedObjsList);

			// For each scene object
			BOOST_FOREACH(SceneObject *o, modifiedObjsList) {
				// Check if is a light source
				if (o->GetMaterial()->IsLightSource()) {
					const string objName = o->GetName();

					// Delete all old triangle lights
					lightDefs.DeleteLightSourceStartWith(objName + TRIANGLE_LIGHT_POSTFIX);

					// Add all new triangle lights
					SDL_LOG("The " << objName << " object is a light sources with " << mesh->GetTotalTriangleCount() << " triangles");

					for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i) {
						TriangleLight *tl = new TriangleLight();
						tl->lightMaterial = o->GetMaterial();
						tl->mesh = mesh;
						tl->triangleIndex = i;
						tl->Preprocess();

						lightDefs.DefineLightSource(objName + TRIANGLE_LIGHT_POSTFIX + ToString(i), tl);
					}

					editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
				}
			}
		}

		extMeshCache.DefineExtMesh(shapeName, mesh);

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

ExtMesh *Scene::CreateInlinedMesh(const string &shapeName, const string &propName, const Properties &props) {
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

ExtMesh *Scene::CreateShape(const string &shapeName, const Properties &props) {
	const string propName = "scene.shapes." + shapeName;

	// Get the shape type
	const string shapeType = props.Get(Property(propName + ".type")("mesh")).Get<string>();

	// Define the shape
	Shape *shape;
	if (shapeType == "mesh") {
		const string meshName = props.Get(Property(propName + ".ply")("")).Get<string>();

		shape = new MeshShape(meshName);
	} else if (shapeType == "inlinedmesh")
		shape = new MeshShape(CreateInlinedMesh(shapeName, propName, props));
	else if (shapeType == "pointiness") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in a pointiness shape: " + shapeName);
		
		shape = new PointinessShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName));
	} else if (shapeType == "strands") {
		const string fileName = props.Get(Property(propName + ".file")("strands.hair")).Get<string>();

		const string tessellationTypeStr = props.Get(Property(propName + ".tesselation.type")("ribbon")).Get<string>();
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

		const u_int adaptiveMaxDepth = Max(0, props.Get(Property(propName + ".tesselation.adaptive.maxdepth")(8)).Get<int>());
		const float adaptiveError = props.Get(Property(propName + ".tesselation.adaptive.error")(.1f)).Get<float>();

		const u_int solidSideCount = Max(0, props.Get(Property(propName + ".tesselation.solid.sidecount")(3)).Get<int>());
		const bool solidCapBottom = props.Get(Property(propName + ".tesselation.solid.capbottom")(false)).Get<bool>();
		const bool solidCapTop = props.Get(Property(propName + ".tesselation.solid.captop")(false)).Get<bool>();
		
		const bool useCameraPosition = props.Get(Property(propName + ".tesselation.usecameraposition")(false)).Get<bool>();

		cyHairFile strandsFile;
		const int strandsCount = strandsFile.LoadFromFile(fileName.c_str());
		if (strandsCount <= 0)
			throw runtime_error("Unable to read strands file: " + fileName);

		shape = new StrendsShape(this,
				&strandsFile, tessellationType,
				adaptiveMaxDepth, adaptiveError,
				solidSideCount, solidCapBottom, solidCapTop,
				useCameraPosition);
	} else if (shapeType == "opensubdiv") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in an opensubdiv shape: " + shapeName);

		auto_ptr<OpenSubdivShape> sbdvShape(new OpenSubdivShape(
				(ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName)));

		// Read OpenSubdiv options
		const string vtxBoundaryOptions = props.Get(Property(propName + ".options.vtxboundary")("edge_and_corner")).Get<string>();
		if (vtxBoundaryOptions == "none")
			sbdvShape->options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_NONE);
		else if (vtxBoundaryOptions == "edge_only")
			sbdvShape->options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_ONLY);
		else if (vtxBoundaryOptions == "edge_and_corner")
			sbdvShape->options.SetVtxBoundaryInterpolation(OpenSubdiv::Sdc::Options::VTX_BOUNDARY_EDGE_AND_CORNER);
		else
			throw runtime_error("OpenSubdiv unknown vertex boundary option: " + vtxBoundaryOptions);

		const string fvarInterpolationRules = props.Get(Property(propName + ".options.fvarlinearinterp")("boundaries")).Get<string>();
		if (fvarInterpolationRules == "none")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_NONE);
		else if (fvarInterpolationRules == "corners_only")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_ONLY);
		else if (fvarInterpolationRules == "corners_plus1")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_PLUS1);
		else if (fvarInterpolationRules == "corners_plus2")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_CORNERS_PLUS2);
		else if (fvarInterpolationRules == "boundaries")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_BOUNDARIES);
		else if (fvarInterpolationRules == "all")
			sbdvShape->options.SetFVarLinearInterpolation(OpenSubdiv::Sdc::Options::FVAR_LINEAR_ALL);
		else
			throw runtime_error("OpenSubdiv unknown fvar boundary option: " + fvarInterpolationRules);

		const string creasingMethod = props.Get(Property(propName + ".options.creasingmethod")("chaikin")).Get<string>();
		if (creasingMethod == "uniform")
			sbdvShape->options.SetCreasingMethod(OpenSubdiv::Sdc::Options::CREASE_UNIFORM);
		else if (creasingMethod == "chaikin")
			sbdvShape->options.SetCreasingMethod(OpenSubdiv::Sdc::Options::CREASE_CHAIKIN);
		else
			throw runtime_error("OpenSubdiv unknown creasing method: " + creasingMethod);

		const string triangleSundiv = props.Get(Property(propName + ".options.trianglesubdiv")("catmark")).Get<string>();
		if (triangleSundiv == "catmark")
			sbdvShape->options.SetTriangleSubdivision(OpenSubdiv::Sdc::Options::TRI_SUB_CATMARK);
		else if (triangleSundiv == "smooth")
			sbdvShape->options.SetTriangleSubdivision(OpenSubdiv::Sdc::Options::TRI_SUB_SMOOTH);
		else
			throw runtime_error("OpenSubdiv unknown creasing method: " + creasingMethod);

		// Read OpenSubdiv scheme type
		const string schemeTypeStr = props.Get(Property(propName + ".scheme.type")("catmark")).Get<string>();
		if (schemeTypeStr == "bilinear")
			sbdvShape->schemeType = OpenSubdiv::Sdc::SCHEME_BILINEAR;
		else if (schemeTypeStr == "catmark")
			sbdvShape->schemeType = OpenSubdiv::Sdc::SCHEME_CATMARK;
		else if (schemeTypeStr == "loop")
			sbdvShape->schemeType = OpenSubdiv::Sdc::SCHEME_LOOP;
		else
			throw runtime_error("OpenSubdiv unknown scheme type: " + schemeTypeStr);

		// Read OpenSubdiv refine type
		const string refineTypeStr = props.Get(Property(propName + ".refine.type")("uniform")).Get<string>();
		if (refineTypeStr == "uniform")
			sbdvShape->refineType = OpenSubdivShape::REFINE_UNIFORM;
		else if (refineTypeStr == "adaptive")
			sbdvShape->refineType = OpenSubdivShape::REFINE_ADAPTIVE;
		else
			throw runtime_error("OpenSubdiv unknown refine type: " + refineTypeStr);

		if (sbdvShape->refineType == OpenSubdivShape::REFINE_UNIFORM)
			sbdvShape->refineMaxLevel = props.Get(Property(propName + ".refine.uniform.maxlevel")(1)).Get<u_int>();
		else if (sbdvShape->refineType == OpenSubdivShape::REFINE_ADAPTIVE)
			sbdvShape->refineMaxLevel = props.Get(Property(propName + ".refine.adaptive.maxlevel")(1)).Get<u_int>();

		shape = sbdvShape.release();
	} else if (shapeType == "harlequin") {
		const string sourceMeshName = props.Get(Property(propName + ".source")("")).Get<string>();
		if (!extMeshCache.IsExtMeshDefined(sourceMeshName))
			throw runtime_error("Unknown shape name in an harlequin shape: " + shapeName);

		shape = new HarlequinShape((ExtTriangleMesh *)extMeshCache.GetExtMesh(sourceMeshName));
	} else
		throw runtime_error("Unknown shape type: " + shapeType);

	ExtMesh *mesh = shape->Refine(this);
	delete shape;

	return mesh;
}
