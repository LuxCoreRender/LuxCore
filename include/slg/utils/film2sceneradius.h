/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _SLG_FILM2SCENERADIUS_H
#define	_SLG_FILM2SCENERADIUS_H

#include "slg/slg.h"
#include "slg/bsdf/bsdf.h"

namespace slg {

class Film2SceneRadiusValidator {
public:
	Film2SceneRadiusValidator() { }
	virtual ~Film2SceneRadiusValidator() { }
	
	virtual bool IsValid(const BSDF &bsdf) const = 0;
};
	
// This function estimates the value of a scene radius starting from film plane
// radius (without material ray differential support)
extern float Film2SceneRadius(const Scene *scene, 
		const float imagePlaneRadius, const float defaultRadius,
		const u_int maxPathDepth, const float timeStart, const float timeEnd,
		const Film2SceneRadiusValidator *validator = nullptr);

}

#endif	/* _SLG_FILM2SCENERADIUS_H */
