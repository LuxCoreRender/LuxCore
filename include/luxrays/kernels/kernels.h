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

namespace luxrays {

// Intersection kernels
extern std::string KernelSource_BVH;
extern std::string KernelSource_QBVH;
extern std::string KernelSource_MQBVH;

// Pixel kernels
extern std::string KernelSource_Pixel_ClearFB;
extern std::string KernelSource_Pixel_ClearSampleFB;
extern std::string KernelSource_Pixel_UpdateFrameBuffer;
extern std::string KernelSource_Pixel_AddSampleBuffer;
extern std::string KernelSource_Pixel_AddSampleBufferPreview;
extern std::string KernelSource_Pixel_AddSampleBufferGaussian2x2;

}

#endif	/* _LUXRAYS_KERNELS_H */
