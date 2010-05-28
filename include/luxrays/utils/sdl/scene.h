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

#ifndef _LUXRAYS_SDL_SCENE_H
#define	_LUXRAYS_SDL_SCENE_H

#include <string>
#include <iostream>
#include <fstream>

#include "luxrays/utils/sdl/camera.h"
#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/material.h"
#include "luxrays/utils/sdl/texmap.h"

#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/properties.h"

namespace luxrays { namespace sdl {

class Scene {
public:
	Scene(Context *ctx, const std::string &fileName, const int accelType);
	~Scene();

	unsigned int SampleLights(const float u) const {
		// One Uniform light strategy
		const unsigned int lightIndex = Min<unsigned int>(Floor2UInt(lights.size() * u), lights.size() - 1);

		return lightIndex;
	}

	PerspectiveCamera *camera;

	std::vector<Material *> materials; // All materials
	TextureMapCache *texMapCache; // Texture maps
	std::map<std::string, size_t> materialIndices; // All materials indices

	std::vector<ExtTriangleMesh *> objects; // All objects
	std::vector<Material *> triangleMaterials; // One for each triangle
	std::vector<TexMapInstance *> triangleTexMaps; // One for each triangle
	std::vector<BumpMapInstance *> triangleBumpMaps; // One for each triangle
	std::vector<NormalMapInstance *> triangleNormalMaps; // One for each triangle

	std::vector<LightSource *> lights; // One for each light source
	InfiniteLight *infiniteLight;
	bool useInfiniteLightBruteForce;

	DataSet *dataSet;

protected:
	std::vector<float> GetParameters(const std::string &paramName,
			const unsigned int paramCount, const std::string &defaultValue) const;

	Properties *scnProp;
};

} }

#endif	/* _LUXRAYS_SDL_SCENE_H */
