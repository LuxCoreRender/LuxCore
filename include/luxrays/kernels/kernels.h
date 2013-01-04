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

extern std::string KernelSource_luxrays_types;
	
// Intersection kernels
extern std::string KernelSource_bvh;
extern std::string KernelSource_qbvh;
extern std::string KernelSource_mqbvh;

extern std::string KernelSource_sampler_types;
extern std::string KernelSource_filter_types;
extern std::string KernelSource_camera_types;
extern std::string KernelSource_trianglemesh_types;
extern std::string KernelSource_trianglemesh_funcs;
extern std::string KernelSource_randomgen_types;
extern std::string KernelSource_randomgen_funcs;
extern std::string KernelSource_matrix4x4_types;
extern std::string KernelSource_transform_types;
extern std::string KernelSource_transform_funcs;
extern std::string KernelSource_mc_funcs;
extern std::string KernelSource_frame_types;
extern std::string KernelSource_frame_funcs;
extern std::string KernelSource_bsdf_types;
extern std::string KernelSource_bsdf_funcs;
extern std::string KernelSource_epsilon_types;
extern std::string KernelSource_epsilon_funcs;
extern std::string KernelSource_spectrum_types;
extern std::string KernelSource_spectrum_funcs;
extern std::string KernelSource_material_types;
extern std::string KernelSource_material_funcs;
extern std::string KernelSource_texture_types;
extern std::string KernelSource_texture_funcs;
extern std::string KernelSource_ray_types;
extern std::string KernelSource_ray_funcs;
extern std::string KernelSource_light_types;
extern std::string KernelSource_light_funcs;
extern std::string KernelSource_vector_types;
extern std::string KernelSource_vector_funcs;
extern std::string KernelSource_triangle_types;
extern std::string KernelSource_triangle_funcs;
extern std::string KernelSource_uv_types;
extern std::string KernelSource_scene_funcs;

} }

#endif	/* _LUXRAYS_KERNELS_H */
