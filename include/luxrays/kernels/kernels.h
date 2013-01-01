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

#ifndef _LUXRAYS_KERNELS_H
#define	_LUXRAYS_KERNELS_H

#include <string>

namespace luxrays { namespace ocl {

// Intersection kernels
extern std::string KernelSource_BVH;
extern std::string KernelSource_QBVH;
extern std::string KernelSource_MQBVH;

extern std::string KernelSource_SamplerTypes;
extern std::string KernelSource_FilterTypes;
extern std::string KernelSource_CameraTypes;
extern std::string KernelSource_TriangleMeshTypes;
extern std::string KernelSource_TriangleMeshFuncs;
extern std::string KernelSource_RandomGenTypes;
extern std::string KernelSource_RandomGenFuncs;
extern std::string KernelSource_Matrix4x4Types;
extern std::string KernelSource_TransformTypes;
extern std::string KernelSource_TransformFuncs;
extern std::string KernelSource_McFuncs;
extern std::string KernelSource_FrameTypes;
extern std::string KernelSource_FrameFuncs;
extern std::string KernelSource_BSDFTypes;
extern std::string KernelSource_BSDFFuncs;

} }

#endif	/* _LUXRAYS_KERNELS_H */
