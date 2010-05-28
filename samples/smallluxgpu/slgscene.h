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
#include "volume.h"
#include "film.h"

#include "luxrays/core/context.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/sdl/scene.h"

using namespace std;
using namespace luxrays::sdl;

typedef enum {
	ONE_UNIFORM, ALL_UNIFORM
} DirectLightStrategy;

typedef enum {
	PROBABILITY, IMPORTANCE
} RussianRouletteStrategy;

class SLGScene : public Scene {
public:
	SLGScene(Context *ctx, const string &fileName, Film *film, const int accelType);
	~SLGScene();

	unsigned int SampleLights(const float u) const {
		// One Uniform light strategy
		const unsigned int lightIndex = Min<unsigned int>(Floor2UInt(lights.size() * u), lights.size() - 1);

		return lightIndex;
	}

	// Signed because of the delta parameter
	int maxPathDepth;
	bool onlySampleSpecular;
	DirectLightStrategy lightStrategy;
	unsigned int shadowRayCount;

	RussianRouletteStrategy rrStrategy;
	int rrDepth;
	float rrProb; // Used by PROBABILITY strategy
	float rrImportanceCap; // Used by IMPORTANCE strategy

	VolumeIntegrator *volumeIntegrator;
};

#endif	/* _SCENE_H */
