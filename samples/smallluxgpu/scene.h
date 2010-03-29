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

#ifndef _SCENE_H
#define	_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "smalllux.h"
#include "camera.h"
#include "light.h"
#include "material.h"

#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "texmap.h"
#include "volume.h"

using namespace std;

typedef enum {
	ONE_UNIFORM, ALL_UNIFORM
} DirectLightStrategy;

class Scene {
public:
	Scene(Context *ctx, const bool lowLatency, const string &fileName, Film *film);
	~Scene();

	unsigned int SampleLights(const float u) const {
		// One Uniform light strategy
		const unsigned int lightIndex = Min<unsigned int>(Floor2UInt(lights.size() * u), lights.size() - 1);

		return lightIndex;
	}

	// Siggned because of the delta parameter
	int maxPathDepth;
	DirectLightStrategy lightStrategy;
	unsigned int shadowRayCount;
	int rrDepth;
	float rrProb;

	PerspectiveCamera *camera;

	vector<Material *> materials; // All materials
	TextureMapCache texMapCache; // Texture maps
	map<string, size_t> materialIndices; // All materials indices

	vector<ExtTriangleMesh *> objects; // All objects
	vector<Material *> triangleMaterials; // One for each triangle
	vector<TextureMap *> triangleTexMaps; // One for each triangle

	vector<LightSource *> lights; // One for each light source

	DataSet *dataSet;

	InfiniteLight *infiniteLight;
	bool useInfiniteLightBruteForce;

	VolumeIntegrator *volumeIntegrator;
};

#endif	/* _SCENE_H */
