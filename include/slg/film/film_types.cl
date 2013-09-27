#line 2 "film_types.cl"

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

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	float filmX, filmY;
	Spectrum radiancePerPixelNormalized[PARAM_FILM_RADIANCE_GROUP_COUNT];
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	float alpha;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	float depth;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
	Point position;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
	Normal geometryNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
	Normal shadingNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
	// Note: MATERIAL_ID_MASK is calculated starting from materialID field
	uint materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	Spectrum directDiffuse;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	Spectrum directGlossy;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	Spectrum emission;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	Spectrum indirectDiffuse;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	Spectrum indirectGlossy;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	Spectrum indirectSpecular;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	float directShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	float indirectShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
	UV uv;
#endif
} SampleResult;

#endif

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define FILM_RADIANCE_GROUP_PARAM_DECL , __global float **filmRadianceGroup
#define FILM_RADIANCE_GROUP_PARAM , filmRadianceGroup

#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
#define FILM_ALPHA_PARAM_DECL , __global float *filmAlpha
#define FILM_ALPHA_PARAM , filmAlpha
#else
#define FILM_ALPHA_PARAM_DECL
#define FILM_ALPHA_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
#define FILM_DEPTH_PARAM_DECL , __global float *filmDepth
#define FILM_DEPTH_PARAM , filmDepth
#else
#define FILM_DEPTH_PARAM_DECL
#define FILM_DEPTH_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
#define FILM_POSITION_PARAM_DECL , __global float *filmPosition
#define FILM_POSITION_PARAM , filmPosition
#else
#define FILM_POSITION_PARAM_DECL
#define FILM_POSITION_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
#define FILM_GEOMETRY_NORMAL_PARAM_DECL , __global float *filmGeometryNormal
#define FILM_GEOMETRY_NORMAL_PARAM , filmGeometryNormal
#else
#define FILM_GEOMETRY_NORMAL_PARAM_DECL
#define FILM_GEOMETRY_NORMAL_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
#define FILM_SHADING_NORMAL_PARAM_DECL , __global float *filmShadingNormal
#define FILM_SHADING_NORMAL_PARAM , filmShadingNormal
#else
#define FILM_SHADING_NORMAL_PARAM_DECL
#define FILM_SHADING_NORMAL_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
#define FILM_MATERIAL_ID_PARAM_DECL , __global uint *filmMaterialID
#define FILM_MATERIAL_ID_PARAM , filmMaterialID
#else
#define FILM_MATERIAL_ID_PARAM_DECL
#define FILM_MATERIAL_ID_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
#define FILM_DIRECT_DIFFUSE_PARAM_DECL , __global float *filmDirectDiffuse
#define FILM_DIRECT_DIFFUSE_PARAM , filmDirectDiffuse
#else
#define FILM_DIRECT_DIFFUSE_PARAM_DECL
#define FILM_DIRECT_DIFFUSE_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
#define FILM_DIRECT_GLOSSY_PARAM_DECL , __global float *filmDirectGlossy
#define FILM_DIRECT_GLOSSY_PARAM , filmDirectGlossy
#else
#define FILM_DIRECT_GLOSSY_PARAM_DECL
#define FILM_DIRECT_GLOSSY_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
#define FILM_EMISSION_PARAM_DECL , __global float *filmEmission
#define FILM_EMISSION_PARAM , filmEmission
#else
#define FILM_EMISSION_PARAM_DECL
#define FILM_EMISSION_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
#define FILM_INDIRECT_DIFFUSE_PARAM_DECL , __global float *filmIndirectDiffuse
#define FILM_INDIRECT_DIFFUSE_PARAM , filmIndirectDiffuse
#else
#define FILM_INDIRECT_DIFFUSE_PARAM_DECL
#define FILM_INDIRECT_DIFFUSE_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
#define FILM_INDIRECT_GLOSSY_PARAM_DECL , __global float *filmIndirectGlossy
#define FILM_INDIRECT_GLOSSY_PARAM , filmIndirectGlossy
#else
#define FILM_INDIRECT_GLOSSY_PARAM_DECL
#define FILM_INDIRECT_GLOSSY_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
#define FILM_INDIRECT_SPECULAR_PARAM_DECL , __global float *filmIndirectSpecular
#define FILM_INDIRECT_SPECULAR_PARAM , filmIndirectSpecular
#else
#define FILM_INDIRECT_SPECULAR_PARAM_DECL
#define FILM_INDIRECT_SPECULAR_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID_MASK)
#define FILM_MATERIAL_ID_MASK_PARAM_DECL , __global float *filmMaterialIDMask
#define FILM_MATERIAL_ID_MASK_PARAM , filmMaterialIDMask
#else
#define FILM_MATERIAL_ID_MASK_PARAM_DECL
#define FILM_MATERIAL_ID_MASK_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
#define FILM_DIRECT_SHADOW_MASK_PARAM_DECL , __global float *filmDirectShadowMask
#define FILM_DIRECT_SHADOW_MASK_PARAM , filmDirectShadowMask
#else
#define FILM_DIRECT_SHADOW_MASK_PARAM_DECL
#define FILM_DIRECT_SHADOW_MASK_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
#define FILM_INDIRECT_SHADOW_MASK_PARAM_DECL , __global float *filmIndirectShadowMask
#define FILM_INDIRECT_SHADOW_MASK_PARAM , filmIndirectShadowMask
#else
#define FILM_INDIRECT_SHADOW_MASK_PARAM_DECL
#define FILM_INDIRECT_SHADOW_MASK_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
#define FILM_UV_PARAM_DECL , __global float *filmUV
#define FILM_UV_PARAM , filmUV
#else
#define FILM_UV_PARAM_DECL
#define FILM_UV_PARAM
#endif

#define FILM_PARAM_DECL , const uint filmWidth, const uint filmHeight FILM_RADIANCE_GROUP_PARAM_DECL FILM_ALPHA_PARAM_DECL FILM_DEPTH_PARAM_DECL FILM_POSITION_PARAM_DECL FILM_GEOMETRY_NORMAL_PARAM_DECL FILM_SHADING_NORMAL_PARAM_DECL FILM_MATERIAL_ID_PARAM_DECL FILM_DIRECT_DIFFUSE_PARAM_DECL FILM_DIRECT_GLOSSY_PARAM_DECL FILM_EMISSION_PARAM_DECL FILM_INDIRECT_DIFFUSE_PARAM_DECL FILM_INDIRECT_GLOSSY_PARAM_DECL FILM_INDIRECT_SPECULAR_PARAM_DECL FILM_MATERIAL_ID_MASK_PARAM_DECL FILM_DIRECT_SHADOW_MASK_PARAM_DECL FILM_INDIRECT_SHADOW_MASK_PARAM_DECL FILM_UV_PARAM_DECL
#define FILM_PARAM , filmWidth, filmHeight FILM_RADIANCE_GROUP_PARAM FILM_ALPHA_PARAM FILM_DEPTH_PARAM FILM_POSITION_PARAM FILM_GEOMETRY_NORMAL_PARAM FILM_SHADING_NORMAL_PARAM FILM_MATERIAL_ID_PARAM FILM_DIRECT_DIFFUSE_PARAM FILM_DIRECT_GLOSSY_PARAM FILM_EMISSION_PARAM FILM_INDIRECT_DIFFUSE_PARAM FILM_INDIRECT_GLOSSY_PARAM FILM_INDIRECT_SPECULAR_PARAM FILM_MATERIAL_ID_MASK_PARAM FILM_DIRECT_SHADOW_MASK_PARAM FILM_INDIRECT_SHADOW_MASK_PARAM FILM_UV_PARAM

#endif
