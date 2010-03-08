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

#ifndef _LUXRAYS_H
#define	_LUXRAYS_H

#include "luxrays/cfg.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/geometry/vector_normal.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/ray.h"
#include "luxrays/core/geometry/raybuffer.h"
#include "luxrays/core/geometry/bbox.h"
#include "luxrays/core/geometry/triangle.h"

namespace luxrays
{
class Accelerator;
class Context;
class DataSet;
class IntersectionDevice;
class Ray;
class RayBuffer;
class RayHit;
class TriangleMesh;
class VirtualM2OIntersectionDevice;
class VirtualM2OHardwareIntersectionDevice;
class VirtualO2MIntersectionDevice;
}

#endif	/* _LUXRAYS_H */
