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

using namespace std;

Scene::Scene(Context *ctx, const bool lowLatency, const string &fileName, Film *film) {
	maxPathDepth = 3;
	shadowRayCount = 1;

	cerr << "Reading scene: " << fileName << endl;

	ifstream file;
	file.exceptions(ifstream::eofbit | ifstream::failbit | ifstream::badbit);
	file.open(fileName.c_str(), ios::in);

	//--------------------------------------------------------------------------
	// Read light position and radius
	//--------------------------------------------------------------------------

	Spectrum lightGain;
	file >> lightGain.r;
	file >> lightGain.g;
	file >> lightGain.b;

	cerr << "Light gain: (" << lightGain.r << ", " << lightGain.g << ", " << lightGain.b << ")" << endl;

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	Point o;
	file >> o.x;
	file >> o.y;
	file >> o.z;

	Point t;
	file >> t.x;
	file >> t.y;
	file >> t.z;

	cerr << "Camera postion: " << o << endl;
	cerr << "Camera target: " << t << endl;

	camera = new PerspectiveCamera(lowLatency, o, t, film);

	//--------------------------------------------------------------------------
	// Read objects .ply file
	//--------------------------------------------------------------------------

	string plyFileName;
	file >> plyFileName;
	cerr << "PLY objects file name: " << plyFileName << endl;

	ExtTriangleMesh *meshObjects = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName);
	// Scale vertex colors
	Spectrum *objCols = meshObjects->GetColors();
	for (unsigned int i = 0; i < meshObjects->GetTotalVertexCount(); ++i)
		objCols[i] *= 0.75f;

	//--------------------------------------------------------------------------
	// Read lights .ply file
	//--------------------------------------------------------------------------

	file >> plyFileName;
	cerr << "PLY lights file name: " << plyFileName << endl;

	ExtTriangleMesh *meshLights = ExtTriangleMesh::LoadExtTriangleMesh(ctx, plyFileName);
	// Scale lights intensity
	Spectrum *lightCols = meshLights->GetColors();
	lightGain *= 0.75f;
	for (unsigned int i = 0; i < meshLights->GetTotalVertexCount(); ++i)
		lightCols[i] *=  lightGain;

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
