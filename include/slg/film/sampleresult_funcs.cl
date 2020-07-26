#line 2 "sampleresult_funcs.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
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

OPENCL_FORCE_INLINE void SampleResult_Init(__constant const Film* restrict film,
		__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[0].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[1].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[2].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[3].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[4].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[5].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[6].c);
	VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[7].c);

	VSTORE3F(BLACK, sampleResult->directDiffuse.c);
	VSTORE3F(BLACK, sampleResult->directGlossy.c);
	VSTORE3F(BLACK, sampleResult->emission.c);
	VSTORE3F(BLACK, sampleResult->indirectDiffuse.c);
	VSTORE3F(BLACK, sampleResult->indirectGlossy.c);
	VSTORE3F(BLACK, sampleResult->indirectSpecular.c);
	sampleResult->rayCount = 0.f;
	VSTORE3F(BLACK, sampleResult->irradiance.c);
	VSTORE3F(BLACK, sampleResult->albedo.c);

	sampleResult->firstPathVertexEvent = NONE;
	sampleResult->firstPathVertex = true;
	// sampleResult->lastPathVertex can not be really initialized here without knowing
	// the max. path depth.
	sampleResult->lastPathVertex = true;
}

OPENCL_FORCE_INLINE void SampleResult_AddEmission(__constant const Film* restrict film,
		__global SampleResult *sampleResult, const uint lightID,
		const float3 pathThroughput, const float3 incomingRadiance) {
	const float3 radiance = pathThroughput * incomingRadiance;

	// Avoid out of bound access if the light group doesn't exist. This can happen
	// with RT modes.
	const uint id = min(lightID, film->radianceGroupCount - 1u);
	VADD3F(sampleResult->radiancePerPixelNormalized[id].c, radiance);

	if (sampleResult->firstPathVertex) {
		VADD3F(sampleResult->emission.c, radiance);
	} else {
		sampleResult->indirectShadowMask = 0.f;

		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if (firstPathVertexEvent & DIFFUSE) {
			VADD3F(sampleResult->indirectDiffuse.c, radiance);
		} else if (firstPathVertexEvent & GLOSSY) {
			VADD3F(sampleResult->indirectGlossy.c, radiance);
		} else if (firstPathVertexEvent & SPECULAR) {
			VADD3F(sampleResult->indirectSpecular.c, radiance);
		}
	}
}

OPENCL_FORCE_INLINE void SampleResult_AddDirectLight(__constant const Film* restrict film,
		__global SampleResult *sampleResult, const uint lightID,
		const BSDFEvent bsdfEvent, const float3 pathThroughput, const float3 incomingRadiance,
		const float lightScale) {
	const float3 radiance = pathThroughput * incomingRadiance;

	// Avoid out of bound access if the light group doesn't exist. This can happen
	// with RT modes.
	const uint id = min(lightID, film->radianceGroupCount - 1u);
	VADD3F(sampleResult->radiancePerPixelNormalized[id].c, radiance);

	if (sampleResult->firstPathVertex) {
		sampleResult->directShadowMask = fmax(0.f, sampleResult->directShadowMask - lightScale);

		if (bsdfEvent & DIFFUSE) {
			VADD3F(sampleResult->directDiffuse.c, radiance);
		} else {
			VADD3F(sampleResult->directGlossy.c, radiance);
		}
	} else {
		sampleResult->indirectShadowMask = fmax(0.f, sampleResult->indirectShadowMask - lightScale);

		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if (firstPathVertexEvent & DIFFUSE) {
			VADD3F(sampleResult->indirectDiffuse.c, radiance);
		} else if (firstPathVertexEvent & GLOSSY) {
			VADD3F(sampleResult->indirectGlossy.c, radiance);
		} else if (firstPathVertexEvent & SPECULAR) {
			VADD3F(sampleResult->indirectSpecular.c, radiance);
		}

		VADD3F(sampleResult->irradiance.c, VLOAD3F(sampleResult->irradiancePathThroughput.c) * incomingRadiance);
	}
}

OPENCL_FORCE_INLINE float3 SampleResult_GetSpectrum(__constant const Film* restrict film,
		__global SampleResult *sampleResult,
		float3 filmRadianceGroupScale[FILM_MAX_RADIANCE_GROUP_COUNT]) {
	float3 c = BLACK;
	for (uint i = 0; i < film->radianceGroupCount; ++i)
		c += VLOAD3F(sampleResult->radiancePerPixelNormalized[i].c) * filmRadianceGroupScale[i];

	return c;
}

OPENCL_FORCE_INLINE float SampleResult_GetRadianceY(__constant const Film* restrict film,
		__global SampleResult *sampleResult) {
	float y = 0.f;
	
	for (uint i = 0; i < film->radianceGroupCount; ++i)
		y += Spectrum_Y(VLOAD3F(sampleResult->radiancePerPixelNormalized[i].c));

	return y;
}

OPENCL_FORCE_INLINE void SampleResult_ClampRadiance(__constant const Film* restrict film,
		__global SampleResult *sampleResult, const uint index,
		const float minRadiance, const float maxRadiance) {
	VSTORE3F(
			Spectrum_ScaledClamp(VLOAD3F(sampleResult->radiancePerPixelNormalized[index].c), minRadiance, maxRadiance),
			sampleResult->radiancePerPixelNormalized[index].c);
}
