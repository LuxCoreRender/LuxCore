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

//------------------------------------------------------------------------------
// Configure unicode support (requires Boost version 1.50 or better)

#if (BOOST_VERSION / 100000 >= 1) && (BOOST_VERSION / 100 % 1000 >= 50)
#define ENABLE_UNICODE_SUPPORT 1
#else
#undef ENABLE_UNICODE_SUPPORT
#endif

//------------------------------------------------------------------------------

#if defined(ENABLE_UNICODE_SUPPORT)
#include <boost/locale.hpp>
#include <boost/filesystem/fstream.hpp>

#define BOOST_IFSTREAM boost::filesystem::ifstream
#define BOOST_OFSTREAM boost::filesystem::ofstream

#else

#define BOOST_IFSTREAM std::ifstream
#define BOOST_OFSTREAM std::ofstream

#endif

//------------------------------------------------------------------------------

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
class IntersectionDevice;
class Mesh;
class Matrix4x4;
class Normal;
class Point;
class Ray;
class RayBuffer;
class RayBufferQueue;
class RayBufferQueueO2O;
class RayHit;
class RGBColor;
class Triangle;
class TriangleMesh;
class UV;
class Vector;
class VirtualIntersectionDevice;
}

#endif	/* _LUXRAYS_H */
