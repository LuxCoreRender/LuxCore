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

#ifndef _LUXRAYS_H
#define	_LUXRAYS_H

#include <boost/version.hpp>

#include "luxrays/cfg.h"
#include "luxrays/utils/utils.h"

/*!
 * \namespace luxrays
 *
 * \brief The LuxRays core classes are defined within this namespace.
 */
namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/luxrays_types.cl"
}

class Accelerator;
class BBox;
class Context;
class DataSet;
class Device;
class DeviceDescription;
class HardwareDevice;
class IntersectionDevice;
class Mesh;
class Matrix4x4;
class Normal;
class Point;
class Ray;
class RayHit;
class RGBColor;
class Triangle;
class TriangleMesh;
class UV;
class Vector;

extern void Init();

extern bool isCudaAvilable;

}

#endif	/* _LUXRAYS_H */
