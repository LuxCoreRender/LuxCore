#line 2 "sampleresult_funcs.cl"

/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

OPENCL_FORCE_INLINE void SampleResult_Init(__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[1].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[2].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[3].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[4].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[5].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[6].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[7].c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
	VSTORE3F(BLACK, sampleResult->directDiffuse.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
	VSTORE3F(BLACK, sampleResult->directGlossy.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
	VSTORE3F(BLACK, sampleResult->emission.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
	VSTORE3F(BLACK, sampleResult->indirectDiffuse.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
	VSTORE3F(BLACK, sampleResult->indirectGlossy.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
	VSTORE3F(BLACK, sampleResult->indirectSpecular.c);
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_RAYCOUNT)
	sampleResult->rayCount = 0.f;
#endif
#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
	VSTORE3F(BLACK, sampleResult->irradiance.c);
#endif

	sampleResult->firstPathVertexEvent = NONE;
	sampleResult->firstPathVertex = true;
	// sampleResult->lastPathVertex can not be really initialized here without knowing
	// the max. path depth.
	sampleResult->lastPathVertex = true;
	sampleResult->passThroughPath = true;
}

OPENCL_FORCE_INLINE void SampleResult_AddEmission(__global SampleResult *sampleResult, const uint lightID,
		const float3 pathThroughput, const float3 incomingRadiance) {
	const float3 radiance = pathThroughput * incomingRadiance;

	// Avoid out of bound access if the light group doesn't exist. This can happen
	// with RT modes.
	const uint id = min(lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
	VADD3F(sampleResult->radiancePerPixelNormalized[id].c, radiance);

	if (sampleResult->firstPathVertex) {
#if defined(PARAM_FILM_CHANNELS_HAS_EMISSION)
		VADD3F(sampleResult->emission.c, radiance);
#endif
	} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = 0.f;
#endif
		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if (firstPathVertexEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			VADD3F(sampleResult->indirectDiffuse.c, radiance);
#endif
		} else if (firstPathVertexEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			VADD3F(sampleResult->indirectGlossy.c, radiance);
#endif
		} else if (firstPathVertexEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			VADD3F(sampleResult->indirectSpecular.c, radiance);
#endif
		}
	}
}

OPENCL_FORCE_INLINE void SampleResult_AddDirectLight(__global SampleResult *sampleResult, const uint lightID,
		const BSDFEvent bsdfEvent, const float3 pathThroughput, const float3 incomingRadiance,
		const float lightScale) {
	const float3 radiance = pathThroughput * incomingRadiance;

	// Avoid out of bound access if the light group doesn't exist. This can happen
	// with RT modes.
	const uint id = min(lightID, PARAM_FILM_RADIANCE_GROUP_COUNT - 1u);
	VADD3F(sampleResult->radiancePerPixelNormalized[id].c, radiance);

	if (sampleResult->firstPathVertex) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_SHADOW_MASK)
		sampleResult->directShadowMask = fmax(0.f, sampleResult->directShadowMask - lightScale);
#endif

		if (bsdfEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_DIFFUSE)
			VADD3F(sampleResult->directDiffuse.c, radiance);
#endif
		} else {
#if defined(PARAM_FILM_CHANNELS_HAS_DIRECT_GLOSSY)
			VADD3F(sampleResult->directGlossy.c, radiance);
#endif
		}
	} else {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SHADOW_MASK)
		sampleResult->indirectShadowMask = fmax(0.f, sampleResult->indirectShadowMask - lightScale);
#endif

		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if (firstPathVertexEvent & DIFFUSE) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_DIFFUSE)
			VADD3F(sampleResult->indirectDiffuse.c, radiance);
#endif
		} else if (firstPathVertexEvent & GLOSSY) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_GLOSSY)
			VADD3F(sampleResult->indirectGlossy.c, radiance);
#endif
		} else if (firstPathVertexEvent & SPECULAR) {
#if defined(PARAM_FILM_CHANNELS_HAS_INDIRECT_SPECULAR)
			VADD3F(sampleResult->indirectSpecular.c, radiance);
#endif
		}

#if defined(PARAM_FILM_CHANNELS_HAS_IRRADIANCE)
		VADD3F(sampleResult->irradiance.c, VLOAD3F(sampleResult->irradiancePathThroughput.c) * incomingRadiance);
#endif
	}
}

OPENCL_FORCE_INLINE float SampleResult_Radiance_Y(__global SampleResult *sampleResult) {
	float y = 0.f;
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[0].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[1].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[2].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[3].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[4].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[5].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[6].c));
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[7].c));
#endif

	return y;
}

OPENCL_FORCE_INLINE void SampleResult_ClampRadiance(__global SampleResult *sampleResult,
		const float minRadiance, const float maxRadiance) {
#if defined(PARAM_FILM_RADIANCE_GROUP_0)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[0].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_1)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[1].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_2)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[2].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_3)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[3].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_4)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[4].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_5)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[5].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_6)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[6].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
#if defined(PARAM_FILM_RADIANCE_GROUP_7)
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[7].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[0].c);
#endif
}
