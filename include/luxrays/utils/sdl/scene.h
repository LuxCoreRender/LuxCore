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

#include "luxrays/utils/sdl/sdl.h"
#include "luxrays/utils/sdl/camera.h"
#include "luxrays/utils/sdl/light.h"
#include "luxrays/utils/sdl/material.h"
#include "luxrays/utils/sdl/texmap.h"
#include "luxrays/utils/sdl/extmeshcache.h"

#include "luxrays/utils/properties.h"

namespace luxrays { namespace sdl {

class Scene {
public:
	Scene(const std::string &fileName, const int accType = -1);
	~Scene();

	int GetAccelType() const { return accelType; }

	unsigned int GetLightCount() const {
		return static_cast<unsigned int>(lights.size());
	}

	LightSource *GetLight(unsigned int index) const {
		return lights[index];
	}

	LightSource *GetLight(const LightSourceType lightType) const {
		// Look for the SunLight
		for (unsigned int i = 0; i < static_cast<unsigned int>(lights.size()); ++i) {
			LightSource *ls = lights[i];
			if (ls->GetType() == lightType)
				return ls;
		}

		return NULL;
	}

	LightSource *SampleAllLights(const float u, float *pdf, bool skipInfiniteLight = false) const {
		const unsigned int lightsSize = static_cast<unsigned int>(lights.size());

		// One Uniform light strategy
		const unsigned int lightIndex = Min(Floor2UInt(lightsSize * u), lightsSize - 1);

		*pdf = 1.f / lightsSize;

		return lights[lightIndex];
	}

	void UpdateDataSet(Context *ctx);

	static Material *CreateMaterial(const std::string &propName, const Properties &prop);

	PerspectiveCamera *camera;

	ExtMeshCache *extMeshCache; // Mesh objects
	TextureMapCache *texMapCache; // Texture maps

	std::vector<LightSource *> lights; // One for each light source
	std::vector<Material *> materials; // All materials (one for each light source)
	std::map<std::string, size_t> materialIndices; // All materials indices
	std::vector<ExtMesh *> objects; // All objects
	std::map<std::string, size_t> objectIndices; // All object indices

	std::vector<Material *> objectMaterials; // One for each object
	std::vector<TexMapInstance *> objectTexMaps; // One for each object
	std::vector<BumpMapInstance *> objectBumpMaps; // One for each object
	std::vector<NormalMapInstance *> objectNormalMaps; // One for each object

	std::vector<LightSource *> triangleLightSource; // One for each triangle

	DataSet *dataSet;

protected:
	static std::vector<float> GetParameters(const Properties &prop,
		const std::string &paramName, const unsigned int paramCount,
		const std::string &defaultValue);

	int accelType;
	Properties *scnProp;
};

} }

#endif	/* _LUXRAYS_SDL_SCENE_H */
