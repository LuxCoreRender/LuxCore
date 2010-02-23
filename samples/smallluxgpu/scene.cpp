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

#include <stdlib.h>

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
	// Read objects .ply file
	//--------------------------------------------------------------------------

	string plyFileName = scnProp.GetString("scene.objects.matte.all", "scene.scn");
	cerr << "PLY objects file name: " << plyFileName << endl;

	ExtTriangleMesh *meshObjects = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName);
	// Scale vertex colors
	Spectrum *objCols = meshObjects->GetColors();
	for (unsigned int i = 0; i < meshObjects->GetTotalVertexCount(); ++i)
		objCols[i] *= 0.75f;

	//--------------------------------------------------------------------------
	// Read lights .ply file
	//--------------------------------------------------------------------------

	plyFileName = scnProp.GetString("scene.objects.light.all", "scene.scn");
	cerr << "PLY lights file name: " << plyFileName << endl;

	ExtTriangleMesh *meshLights = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName);
	// Scale lights intensity
	Spectrum *lightCols = meshLights->GetColors();
	lightGain *= 0.75f;
	for (unsigned int i = 0; i < meshLights->GetTotalVertexCount(); ++i)
		lightCols[i] *= lightGain;

	//--------------------------------------------------------------------------
	// Join the ply objects
	//--------------------------------------------------------------------------

	meshLightOffset = meshObjects->GetTotalTriangleCount();
	std::deque<ExtTriangleMesh *> objList;
	objList.push_back(meshObjects);
	objList.push_back(meshLights);
	mesh = ExtTriangleMesh::Merge(objList);

	// Free temporary objects
	meshObjects->Delete();
	delete meshObjects;
	meshLights->Delete();
	delete meshLights;

	cerr << "Vertex count: " << mesh->GetTotalVertexCount() << " (" << (mesh->GetTotalVertexCount() * sizeof(Point) / 1024) << "Kb)" << endl;
	cerr << "Triangle count: " << mesh->GetTotalTriangleCount() << " (" << (mesh->GetTotalTriangleCount() * sizeof(Triangle) / 1024) << "Kb)" << endl;

	//--------------------------------------------------------------------------
	// Create light sources list
	//--------------------------------------------------------------------------

	nLights = mesh->GetTotalTriangleCount() - meshLightOffset;
	lights = new TriangleLight[nLights];
	for (size_t i = 0; i < nLights; ++i)
		new (&lights[i]) TriangleLight(i + meshLightOffset, mesh);

	//--------------------------------------------------------------------------
	// Create the DataSet
	//--------------------------------------------------------------------------

	dataSet = new DataSet(ctx);
	dataSet->Add(mesh);
	dataSet->Preprocess();
}
