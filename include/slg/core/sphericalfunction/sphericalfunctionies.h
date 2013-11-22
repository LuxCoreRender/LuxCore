/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_SPHERICALFUNCTIONIES_H
#define _SLG_SPHERICALFUNCTIONIES_H

#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/core/sphericalfunction/photometricdataies.h"

namespace slg {

/**
 * A spherical function based on measured IES data.
 */
class IESSphericalFunction : public ImageMapSphericalFunction {
public:
	IESSphericalFunction(const PhotometricDataIES &data, const bool flipZ);
	~IESSphericalFunction();
};

}

#endif //_SLG_SPHERICALFUNCTIONIES_H
