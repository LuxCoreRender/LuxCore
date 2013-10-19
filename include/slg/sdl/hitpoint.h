/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_HITPOINT_H
#define	_SLG_HITPOINT_H

#include "luxrays/luxrays.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/hitpoint_types.cl"
}

typedef struct {
	// The incoming direction. It is the eyeDir when fromLight = false and
	// lightDir when fromLight = true
	luxrays::Vector fixedDir;
	luxrays::Point p;
	luxrays::UV uv;
	luxrays::Normal geometryN;
	luxrays::Normal shadeN;
	luxrays::Spectrum color;
	float alpha;
	float passThroughEvent;
	bool fromLight;
} HitPoint;

}

#endif	/* _SLG_HITPOINT_H */
