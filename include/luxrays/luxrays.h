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

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;


/*! \namespace luxrays
 *
 * \brief The LuxRays core classes are defined within this namespace.
 */
namespace luxrays {
class Accelerator;
class BBox;
class Context;
class DataSet;
class IntersectionDevice;
class Mesh;
class Normal;
class PixelDevice;
class Point;
class Ray;
class RayBuffer;
class RayBufferQueue;
class RayBufferQueueO2O;
class RayHit;
class SampleBuffer;
class Spectrum;
class Triangle;
class TriangleMesh;
class UV;
class Vector;
class VirtualM2MHardwareIntersectionDevice;
class VirtualM2OHardwareIntersectionDevice;
}

#endif	/* _LUXRAYS_H */
