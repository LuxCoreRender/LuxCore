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

using namespace std;

Scene::Scene(Context *ctx, const bool lowLatency, const string &fileName, Film *film) {
	maxPathDepth = 3;
	shadowRayCount = 1;

	cerr << "Reading scene: " << fileName << endl;

	Properties scnProp(fileName);

	//--------------------------------------------------------------------------
	// Read light position and radius
	//--------------------------------------------------------------------------

	vector<float> vf = scnProp.GetFloatVector("scene.lights.globalscale", "1.0 1.0 1.0");
	if (vf.size() != 3)
		throw runtime_error("Syntax error in scene.lights.globalscale parameter");
	Spectrum lightGain(vf.at(0), vf.at(1), vf.at(2));

	cerr << "Light gain: " << lightGain << endl;

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	vf = scnProp.GetFloatVector("scene.camera.lookat", "10.0 0.0 0.0  0.0 0.0 0.0");
	if (vf.size() != 6)
		throw runtime_error("Syntax error in scene.camera.lookat parameter");
	Point o(vf.at(0), vf.at(1), vf.at(2));
	Point t(vf.at(3), vf.at(4), vf.at(5));

	cerr << "Camera postion: " << o << endl;
	cerr << "Camera target: " << t << endl;

	camera = new PerspectiveCamera(lowLatency, o, t, film);

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
		// Scale vertex colors
		Spectrum *objCols = meshObject->GetColors();
		for (unsigned int i = 0; i < meshObject->GetTotalVertexCount(); ++i)
			objCols[i] *= 0.75f;

		objects.push_back(meshObject);

		// Check if it is a light sources
		if (key.find("scene.objects.light.") == 0) {
			cerr << "The objects is a light sources with " << meshObject->GetTotalTriangleCount() << " triangles" << endl;

			// Scale lights intensity
			Spectrum *lightCols = meshObject->GetColors();
			for (unsigned int i = 0; i < meshObject->GetTotalVertexCount(); ++i)
				lightCols[i] *= lightGain;

			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i) {
				TriangleLight *tl = new TriangleLight(objects.size() - 1, i, objects);
				lights.push_back(tl);
				triangleMatirials.push_back(tl);
			}
		} else {
			for (unsigned int i = 0; i < meshObject->GetTotalTriangleCount(); ++i)
				triangleMatirials.push_back(NULL);
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
