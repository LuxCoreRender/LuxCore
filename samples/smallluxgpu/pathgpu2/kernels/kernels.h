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

#ifndef _SLG_KERNELS_H
#define	_SLG_KERNELS_H

#include <string>

namespace luxrays {

// Intersection kernels
extern std::string KernelSource_PathGPU2_kernel_core;
extern std::string KernelSource_PathGPU2_kernel_datatypes;
extern std::string KernelSource_PathGPU2_kernel_filters;
extern std::string KernelSource_PathGPU2_kernel_samplers;
extern std::string KernelSource_PathGPU2_kernel_scene;
extern std::string KernelSource_PathGPU2_kernels;


}

#endif	/* _SLG_KERNELS_H */
