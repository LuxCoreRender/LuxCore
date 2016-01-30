#line 2 "biaspathocl_sampleresult_funcs.cl"

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

void SR_Accumulate(__global SampleResult *src, SampleResult *dst) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].c[0] += src->radiancePerPixelNormalized[0].c[0];
	dst->radiancePerPixelNormalized[0].c[1] += src->radiancePerPixelNormalized[0].c[1];
	dst->radiancePerPixelNormalized[0].c[2] += src->radiancePerPixelNormalized[0].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].c[0] += src->radiancePerPixelNormalized[1].c[0];
	dst->radiancePerPixelNormalized[1].c[1] += src->radiancePerPixelNormalized[1].c[1];
	dst->radiancePerPixelNormalized[1].c[2] += src->radiancePerPixelNormalized[1].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].c[0] += src->radiancePerPixelNormalized[2].c[0];
	dst->radiancePerPixelNormalized[2].c[1] += src->radiancePerPixelNormalized[2].c[1];
	dst->radiancePerPixelNormalized[2].c[2] += src->radiancePerPixelNormalized[2].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].c[0] += src->radiancePerPixelNormalized[3].c[0];
	dst->radiancePerPixelNormalized[3].c[1] += src->radiancePerPixelNormalized[3].c[1];
	dst->radiancePerPixelNormalized[3].c[2] += src->radiancePerPixelNormalized[3].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].c[0] += src->radiancePerPixelNormalized[4].c[0];
	dst->radiancePerPixelNormalized[4].c[1] += src->radiancePerPixelNormalized[4].c[1];
	dst->radiancePerPixelNormalized[4].c[2] += src->radiancePerPixelNormalized[4].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].c[0] += src->radiancePerPixelNormalized[5].c[0];
	dst->radiancePerPixelNormalized[5].c[1] += src->radiancePerPixelNormalized[5].c[1];
	dst->radiancePerPixelNormalized[5].c[2] += src->radiancePerPixelNormalized[5].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].c[0] += src->radiancePerPixelNormalized[6].c[0];
	dst->radiancePerPixelNormalized[6].c[1] += src->radiancePerPixelNormalized[6].c[1];
	dst->radiancePerPixelNormalized[6].c[2] += src->radiancePerPixelNormalized[6].c[2];
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].c[0] += src->radiancePerPixelNormalized[7].c[0];
	dst->radiancePerPixelNormalized[7].c[1] += src->radiancePerPixelNormalized[7].c[1];
	dst->radiancePerPixelNormalized[7].c[2] += src->radiancePerPixelNormalized[7].c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha += src->alpha;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.c[0] += src->directDiffuse.c[0];
	dst->directDiffuse.c[1] += src->directDiffuse.c[1];
	dst->directDiffuse.c[2] += src->directDiffuse.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.c[0] += src->directGlossy.c[0];
	dst->directGlossy.c[1] += src->directGlossy.c[1];
	dst->directGlossy.c[2] += src->directGlossy.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.c[0] += src->emission.c[0];
	dst->emission.c[1] += src->emission.c[1];
	dst->emission.c[2] += src->emission.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.c[0] += src->indirectDiffuse.c[0];
	dst->indirectDiffuse.c[1] += src->indirectDiffuse.c[1];
	dst->indirectDiffuse.c[2] += src->indirectDiffuse.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.c[0] += src->indirectGlossy.c[0];
	dst->indirectGlossy.c[1] += src->indirectGlossy.c[1];
	dst->indirectGlossy.c[2] += src->indirectGlossy.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.c[0] += src->indirectSpecular.c[0];
	dst->indirectSpecular.c[1] += src->indirectSpecular.c[1];
	dst->indirectSpecular.c[2] += src->indirectSpecular.c[2];
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask += src->directShadowMask;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask += src->indirectShadowMask;
#endif

	bool depthWrite = true;
#if defined(PARAM_FILM_CHANNELS_HAS_DEPTH)
	const float srcDepthValue = src->depth;
	if (srcDepthValue <= dst->depth)
		dst->depth = srcDepthValue;
	else
		depthWrite = false;
#endif
	if (depthWrite) {
#if defined(PARAM_FILM_CHANNELS_HAS_POSITION)
		dst->position = src->position;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_GEOMETRY_NORMAL)
		dst->geometryNormal = src->geometryNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_SHADING_NORMAL)
		dst->shadingNormal = src->shadingNormal;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_MATERIAL_ID)
		// Note: MATERIAL_ID_MASK and BY_MATERIAL_ID are calculated starting from materialID field
		dst->materialID = src->materialID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_OBJECT_ID)
		// Note: OBJECT_ID_MASK and BY_OBJECT_ID are calculated starting from objectID field
		dst->objectID = src->objectID;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_UV)
		dst->uv = src->uv;
#endif
	}

#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	dst->rayCount += src->rayCount;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	dst->irradiance.c[0] += src->irradiance.c[0];
	dst->irradiance.c[1] += src->irradiance.c[1];
	dst->irradiance.c[2] += src->irradiance.c[2];
#endif
}

void SR_Normalize(SampleResult *dst, const float k) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	dst->radiancePerPixelNormalized[0].c[0] *= k;
	dst->radiancePerPixelNormalized[0].c[1] *= k;
	dst->radiancePerPixelNormalized[0].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	dst->radiancePerPixelNormalized[1].c[0] *= k;
	dst->radiancePerPixelNormalized[1].c[1] *= k;
	dst->radiancePerPixelNormalized[1].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	dst->radiancePerPixelNormalized[2].c[0] *= k;
	dst->radiancePerPixelNormalized[2].c[1] *= k;
	dst->radiancePerPixelNormalized[2].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	dst->radiancePerPixelNormalized[3].c[0] *= k;
	dst->radiancePerPixelNormalized[3].c[1] *= k;
	dst->radiancePerPixelNormalized[3].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	dst->radiancePerPixelNormalized[4].c[0] *= k;
	dst->radiancePerPixelNormalized[4].c[1] *= k;
	dst->radiancePerPixelNormalized[4].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	dst->radiancePerPixelNormalized[5].c[0] *= k;
	dst->radiancePerPixelNormalized[5].c[1] *= k;
	dst->radiancePerPixelNormalized[5].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	dst->radiancePerPixelNormalized[6].c[0] *= k;
	dst->radiancePerPixelNormalized[6].c[1] *= k;
	dst->radiancePerPixelNormalized[6].c[2] *= k;
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	dst->radiancePerPixelNormalized[7].c[0] *= k;
	dst->radiancePerPixelNormalized[7].c[1] *= k;
	dst->radiancePerPixelNormalized[7].c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_ALPHA)
	dst->alpha *= k;
#endif

#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	dst->directDiffuse.c[0] *= k;
	dst->directDiffuse.c[1] *= k;
	dst->directDiffuse.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	dst->directGlossy.c[0] *= k;
	dst->directGlossy.c[1] *= k;
	dst->directGlossy.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	dst->emission.c[0] *= k;
	dst->emission.c[1] *= k;
	dst->emission.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	dst->indirectDiffuse.c[0] *= k;
	dst->indirectDiffuse.c[1] *= k;
	dst->indirectDiffuse.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	dst->indirectGlossy.c[0] *= k;
	dst->indirectGlossy.c[1] *= k;
	dst->indirectGlossy.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	dst->indirectSpecular.c[0] *= k;
	dst->indirectSpecular.c[1] *= k;
	dst->indirectSpecular.c[2] *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
	dst->directShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
	dst->indirectShadowMask *= k;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	dst->irradiance.c[0] *= k;
	dst->irradiance.c[1] *= k;
	dst->irradiance.c[2] *= k;
#endif
}
