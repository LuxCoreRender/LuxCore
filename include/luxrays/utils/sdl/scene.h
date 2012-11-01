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

	LightSource *GetLightByType(const LightSourceType lightType) const;
	LightSource *SampleAllLights(const float u, float *pdf) const;
	float PickLightPdf() const;
	Spectrum GetEnvLightsRadiance(const Vector &dir,
			const Point &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

	void UpdateDataSet(Context *ctx);

	static Material *CreateMaterial(const std::string &propName, const Properties &prop);

	PerspectiveCamera *camera;

	ExtMeshCache *extMeshCache; // Mesh objects
	TextureMapCache *texMapCache; // Texture maps

	LightSource *infiniteLight; // A SLG scene can have only one infinite light
	LightSource *sunLight;
	std::vector<LightSource *> lights; // One for each light source (doesn't include light/infinite light)

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
