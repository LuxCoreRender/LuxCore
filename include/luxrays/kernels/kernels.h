/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _LUXRAYS_KERNELS_H
#define	_LUXRAYS_KERNELS_H

#include <string>

namespace luxrays { namespace ocl {

extern std::string KernelSource_luxrays_types;
	
// Intersection kernels
extern std::string KernelSource_bvh_types;
extern std::string KernelSource_bvh;
extern std::string KernelSource_mbvh;
extern std::string KernelSource_qbvh_types;
extern std::string KernelSource_qbvh;
extern std::string KernelSource_mqbvh;

extern std::string KernelSource_trianglemesh_types;
extern std::string KernelSource_trianglemesh_funcs;
extern std::string KernelSource_randomgen_types;
extern std::string KernelSource_randomgen_funcs;
extern std::string KernelSource_matrix4x4_types;
extern std::string KernelSource_matrix4x4_funcs;
extern std::string KernelSource_transform_types;
extern std::string KernelSource_transform_funcs;
extern std::string KernelSource_frame_types;
extern std::string KernelSource_frame_funcs;
extern std::string KernelSource_epsilon_types;
extern std::string KernelSource_epsilon_funcs;
extern std::string KernelSource_ray_types;
extern std::string KernelSource_ray_funcs;
extern std::string KernelSource_point_types;
extern std::string KernelSource_vector_types;
extern std::string KernelSource_vector_funcs;
extern std::string KernelSource_normal_types;
extern std::string KernelSource_quaternion_types;
extern std::string KernelSource_quaternion_funcs;
extern std::string KernelSource_bbox_types;
extern std::string KernelSource_bbox_funcs;
extern std::string KernelSource_motionsystem_types;
extern std::string KernelSource_motionsystem_funcs;
extern std::string KernelSource_triangle_types;
extern std::string KernelSource_triangle_funcs;
extern std::string KernelSource_uv_types;
extern std::string KernelSource_utils_funcs;
extern std::string KernelSource_color_types;
extern std::string KernelSource_color_funcs;

// Util kernel functions
extern std::string KernelSource_mc_funcs;

} }

#endif	/* _LUXRAYS_KERNELS_H */
