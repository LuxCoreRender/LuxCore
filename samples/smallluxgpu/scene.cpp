/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>

#include <boost/detail/container_fwd.hpp>

#include "scene.h"

#include "luxrays/core/dataset.h"
#include "luxrays/utils/properties.h"
#include "trianglemat.h"

using namespace std;

Scene::Scene(Context *ctx, const bool lowLatency, const string &fileName, Film *film) {
	maxPathDepth = 3;
	shadowRayCount = 1;

	cerr << "Reading scene: " << fileName << endl;

	Properties scnProp(fileName);

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	vector<float> vf = scnProp.GetFloatVector("scene.camera.lookat", "10.0 0.0 0.0  0.0 0.0 0.0");
	if (vf.size() != 6)
		throw runtime_error("Syntax error in scene.camera.lookat parameter");
	Point o(vf.at(0), vf.at(1), vf.at(2));
	Point t(vf.at(3), vf.at(4), vf.at(5));

	cerr << "Camera postion: " << o << endl;
	cerr << "Camera target: " << t << endl;

	camera = new PerspectiveCamera(lowLatency, o, t, film);

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	std::vector<std::string> matKeys = scnProp.GetAllKeys("scene.materials.");
	if (matKeys.size() == 0)
		throw runtime_error("No material definition found");

	for (std::vector<std::string>::const_iterator matKey = matKeys.begin(); matKey != matKeys.end(); ++matKey) {
		const string &key = *matKey;
		const string matType = Properties::ExtractField(key, 2);
		if (matType == "")
			throw runtime_error("Syntax error in " + key);
		const string matName = Properties::ExtractField(key, 3);
		if (matName == "")
			throw runtime_error("Syntax error in " + key);

		cerr << "Material definition: " << matName << " [" << matType << "]" << endl;

		if (matType == "matte") {
			vf = scnProp.GetFloatVector("scene.materials." + matType + "." + matName, "");
			if (vf.size() != 3)
				throw runtime_error("Syntax error in scene.materials." + matType + "." + matName);
			const Spectrum col(vf.at(0), vf.at(1), vf.at(2));

			MatteMaterial *mat = new MatteMaterial(col);
			materialIndices[matName] = materials.size();
			materials.push_back(mat);
		} else if (matType == "light") {
			vf = scnProp.GetFloatVector("scene.materials." + matType + "." + matName, "");
			if (vf.size() != 3)
				throw runtime_error("Syntax error in scene.materials." + matType + "." + matName);
			const Spectrum gain(vf.at(0), vf.at(1), vf.at(2));

			AreaLightMaterial *mat = new AreaLightMaterial(gain);
			materialIndices[matName] = materials.size();
			materials.push_back(mat);
		} else if (matType == "mirror") {
			vf = scnProp.GetFloatVector("scene.materials." + matType + "." + matName, "");
			if (vf.size() != 3)
				throw runtime_error("Syntax error in scene.materials." + matType + "." + matName);
			const Spectrum col(vf.at(0), vf.at(1), vf.at(2));

			MirrorMaterial *mat = new MirrorMaterial(col);
			materialIndices[matName] = materials.size();
			materials.push_back(mat);
		}  else if (matType == "mattemirror") {
			vf = scnProp.GetFloatVector("scene.materials." + matType + "." + matName, "");
			if (vf.size() != 6)
				throw runtime_error("Syntax error in scene.materials." + matType + "." + matName);
			const Spectrum Kd(vf.at(0), vf.at(1), vf.at(2));
			const Spectrum Kr(vf.at(3), vf.at(4), vf.at(5));

			MatteMirrorMaterial *mat = new MatteMirrorMaterial(Kd, Kr);
			materialIndices[matName] = materials.size();
			materials.push_back(mat);
		} else
			throw runtime_error("Unknown material type " + matType);
	}

	//--------------------------------------------------------------------------
	// Read all objects .ply file
	//--------------------------------------------------------------------------

	std::vector<std::string> objKeys = scnProp.GetAllKeys("scene.objects.");
	if (objKeys.size() == 0)
		objKeys.push_back("scene.scn");

	for (std::vector<std::string>::const_iterator objKey = objKeys.begin(); objKey != objKeys.end(); ++objKey) {
		const string &key = *objKey;
		const string plyFileName = scnProp.GetString(key, "scene.scn");
		cerr << "PLY objects file name: " << plyFileName << endl;

		ExtTriangleMesh *meshObject = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName);
		objects.push_back(meshObject);

		// Get the material
		const string matName = Properties::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material name: " + matName);
		if (materialIndices.count(matName) < 1)
			throw runtime_error("Unknown material: " + matName);
		Material *mat = materials[materialIndices[matName]];

		const string objName = Properties::ExtractField(key, 3);
		// Check if it is a light sources
		if (mat->IsLightSource()) {
			cerr << "The " << objName << " objects is a light sources with " << meshObject->GetTotalTriangleCount() << " triangles" << endl;

			AreaLightMaterial *light = (AreaLightMaterial *)mat;
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i) {
				TriangleLight *tl = new TriangleLight(light, objects.size() - 1, i, objects);
				lights.push_back(tl);
				triangleMatirials.push_back(tl);
			}
		} else {
			SurfaceMaterial *surfMat = (SurfaceMaterial *)mat;
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
				triangleMatirials.push_back(surfMat);
		}
	}

	//--------------------------------------------------------------------------
	// Create the DataSet
	//--------------------------------------------------------------------------

	dataSet = new DataSet(ctx);

	// Add all objects
	for (std::vector<ExtTriangleMesh *>::const_iterator obj = objects.begin(); obj != objects.end(); ++obj)
		dataSet->Add(*obj);

	dataSet->Preprocess();
}
