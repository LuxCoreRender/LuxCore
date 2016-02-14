#line 2 "film_types.cl"

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

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

typedef struct {
	// Used only by PATHOCL
#if defined(PARAM_USE_FAST_PIXEL_FILTER)
	uint pixelX, pixelY;
#endif
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
	// Note: MATERIAL_ID_MASK and BY_MATERIAL_ID are calculated starting from materialID field
	uint materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
	// Note: OBJECT_ID_MASK and BY_OBJECT_ID are calculated starting from objectID field
	uint objectID;
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
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	float rayCount;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	Spectrum irradiance, irradiancePathThroughput;
#endif

	BSDFEvent firstPathVertexEvent;
	int firstPathVertex, lastPathVertex, passThroughPath;
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
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
#define FILM_RAYCOUNT_PARAM_DECL , __global float *filmRayCount
#define FILM_RAYCOUNT_PARAM , filmRayCount
#else
#define FILM_RAYCOUNT_PARAM_DECL
#define FILM_RAYCOUNT_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_MATERIAL_ID)
#define FILM_BY_MATERIAL_ID_PARAM_DECL , __global float *filmByMaterialID
#define FILM_BY_MATERIAL_ID_PARAM , filmByMaterialID
#else
#define FILM_BY_MATERIAL_ID_PARAM_DECL
#define FILM_BY_MATERIAL_ID_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
#define FILM_IRRADIANCE_PARAM_DECL , __global float *filmIrradiance
#define FILM_IRRADIANCE_PARAM , filmIrradiance
#else
#define FILM_IRRADIANCE_PARAM_DECL
#define FILM_IRRADIANCE_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
#define FILM_OBJECT_ID_PARAM_DECL , __global uint *filmObjectID
#define FILM_OBJECT_ID_PARAM , filmObjectID
#else
#define FILM_OBJECT_ID_PARAM_DECL
#define FILM_OBJECT_ID_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID_MASK)
#define FILM_OBJECT_ID_MASK_PARAM_DECL , __global float *filmObjectIDMask
#define FILM_OBJECT_ID_MASK_PARAM , filmObjectIDMask
#else
#define FILM_OBJECT_ID_MASK_PARAM_DECL
#define FILM_OBJECT_ID_MASK_PARAM
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_BY_OBJECT_ID)
#define FILM_BY_OBJECT_ID_PARAM_DECL , __global float *filmByObjectID
#define FILM_BY_OBJECT_ID_PARAM , filmByObjectID
#else
#define FILM_BY_OBJECT_ID_PARAM_DECL
#define FILM_BY_OBJECT_ID_PARAM
#endif

#define FILM_PARAM_DECL , const uint filmWidth, const uint filmHeight FILM_RADIANCE_GROUP_PARAM_DECL FILM_ALPHA_PARAM_DECL FILM_DEPTH_PARAM_DECL FILM_POSITION_PARAM_DECL FILM_GEOMETRY_NORMAL_PARAM_DECL FILM_SHADING_NORMAL_PARAM_DECL FILM_MATERIAL_ID_PARAM_DECL FILM_DIRECT_DIFFUSE_PARAM_DECL FILM_DIRECT_GLOSSY_PARAM_DECL FILM_EMISSION_PARAM_DECL FILM_INDIRECT_DIFFUSE_PARAM_DECL FILM_INDIRECT_GLOSSY_PARAM_DECL FILM_INDIRECT_SPECULAR_PARAM_DECL FILM_MATERIAL_ID_MASK_PARAM_DECL FILM_DIRECT_SHADOW_MASK_PARAM_DECL FILM_INDIRECT_SHADOW_MASK_PARAM_DECL FILM_UV_PARAM_DECL FILM_RAYCOUNT_PARAM_DECL FILM_BY_MATERIAL_ID_PARAM_DECL FILM_IRRADIANCE_PARAM_DECL FILM_OBJECT_ID_PARAM_DECL FILM_OBJECT_ID_MASK_PARAM_DECL FILM_BY_OBJECT_ID_PARAM_DECL
#define FILM_PARAM , filmWidth, filmHeight FILM_RADIANCE_GROUP_PARAM FILM_ALPHA_PARAM FILM_DEPTH_PARAM FILM_POSITION_PARAM FILM_GEOMETRY_NORMAL_PARAM FILM_SHADING_NORMAL_PARAM FILM_MATERIAL_ID_PARAM FILM_DIRECT_DIFFUSE_PARAM FILM_DIRECT_GLOSSY_PARAM FILM_EMISSION_PARAM FILM_INDIRECT_DIFFUSE_PARAM FILM_INDIRECT_GLOSSY_PARAM FILM_INDIRECT_SPECULAR_PARAM FILM_MATERIAL_ID_MASK_PARAM FILM_DIRECT_SHADOW_MASK_PARAM FILM_INDIRECT_SHADOW_MASK_PARAM FILM_UV_PARAM FILM_RAYCOUNT_PARAM FILM_BY_MATERIAL_ID_PARAM FILM_IRRADIANCE_PARAM FILM_OBJECT_ID_PARAM FILM_OBJECT_ID_MASK_PARAM FILM_BY_OBJECT_ID_PARAM

#endif
