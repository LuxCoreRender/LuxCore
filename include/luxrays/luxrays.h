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

#ifndef _LUXRAYS_H
#define	_LUXRAYS_H

#include <boost/version.hpp>

#include "luxrays/cfg.h"

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

// According Masol Lee's test FreeImage unicode support works only on Windows. FreeImage
// documentation seems to confirm that unicode functions are supported only on Windows.
#if defined(ENABLE_UNICODE_SUPPORT) && defined(WIN32)

#define FREEIMAGE_CONVFILENAME(a)  boost::locale::conv::utf_to_utf<wchar_t>(a)
#define FREEIMAGE_GETFILETYPE  FreeImage_GetFileTypeU
#define FREEIMAGE_GETFIFFROMFILENAME FreeImage_GetFIFFromFilenameU
#define FREEIMAGE_LOAD FreeImage_LoadU
#define FREEIMAGE_SAVE FreeImage_SaveU

#else

#define FREEIMAGE_CONVFILENAME(a)  (a)
#define FREEIMAGE_GETFILETYPE  FreeImage_GetFileType
#define FREEIMAGE_GETFIFFROMFILENAME FreeImage_GetFIFFromFilename
#define FREEIMAGE_LOAD FreeImage_Load
#define FREEIMAGE_SAVE FreeImage_Save

#endif

//------------------------------------------------------------------------------

typedef unsigned char u_char;
typedef unsigned short u_short;
typedef unsigned int u_int;
typedef unsigned long u_long;
typedef unsigned long u_longlong;

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
class Normal;
class Point;
class Ray;
class RayBuffer;
class RayBufferQueue;
class RayBufferQueueO2O;
class RayHit;
class SampleBuffer;
class Triangle;
class TriangleMesh;
class UV;
class Vector;
class VirtualIntersectionDevice;
}

#endif	/* _LUXRAYS_H */
