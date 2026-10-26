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

// A SampleResult channel is read only when the film has a channel using it
// (see Film_AddSampleResultColor() and VarianceClamping_Clamp()): skip the
// memory writes of the other channels
#define SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_REFLECT(film) ((film)->hasChannelDirectDiffuse || (film)->hasChannelDirectDiffuseReflect)
#define SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_TRANSMIT(film) ((film)->hasChannelDirectDiffuse || (film)->hasChannelDirectDiffuseTransmit)
#define SAMPLERESULT_NEEDS_DIRECT_GLOSSY_REFLECT(film) ((film)->hasChannelDirectGlossy || (film)->hasChannelDirectGlossyReflect)
#define SAMPLERESULT_NEEDS_DIRECT_GLOSSY_TRANSMIT(film) ((film)->hasChannelDirectGlossy || (film)->hasChannelDirectGlossyTransmit)
#define SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_REFLECT(film) ((film)->hasChannelIndirectDiffuse || (film)->hasChannelIndirectDiffuseReflect)
#define SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_TRANSMIT(film) ((film)->hasChannelIndirectDiffuse || (film)->hasChannelIndirectDiffuseTransmit)
#define SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_REFLECT(film) ((film)->hasChannelIndirectGlossy || (film)->hasChannelIndirectGlossyReflect)
#define SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_TRANSMIT(film) ((film)->hasChannelIndirectGlossy || (film)->hasChannelIndirectGlossyTransmit)
#define SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_REFLECT(film) ((film)->hasChannelIndirectSpecular || (film)->hasChannelIndirectSpecularReflect)
#define SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_TRANSMIT(film) ((film)->hasChannelIndirectSpecular || (film)->hasChannelIndirectSpecularTransmit)

OPENCL_FORCE_INLINE void SampleResult_ClearRadiance(__constant const Film* restrict film,
		__global SampleResult *sampleResult) {
	// Only the film radiance groups are read (light IDs are clamped to
	// radianceGroupCount - 1 by SampleResult_AddEmission/AddDirectLight)
	for (uint i = 0; i < film->radianceGroupCount; ++i)
		VSTORE3F(BLACK, sampleResult->radiancePerPixelNormalized[i].c);
}

OPENCL_FORCE_INLINE void SampleResult_Init(__constant const Film* restrict film,
		__global SampleResult *sampleResult) {
	// Initialize only Spectrum fields

	SampleResult_ClearRadiance(film, sampleResult);

	// directDiffuse, directGlossy, indirectDiffuse, indirectGlossy and
	// indirectSpecular are never read: the film sums the Reflect and Transmit
	// parts instead
	if (SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_REFLECT(film))
		VSTORE3F(BLACK, sampleResult->directDiffuseReflect.c);
	if (SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_TRANSMIT(film))
		VSTORE3F(BLACK, sampleResult->directDiffuseTransmit.c);
	if (SAMPLERESULT_NEEDS_DIRECT_GLOSSY_REFLECT(film))
		VSTORE3F(BLACK, sampleResult->directGlossyReflect.c);
	if (SAMPLERESULT_NEEDS_DIRECT_GLOSSY_TRANSMIT(film))
		VSTORE3F(BLACK, sampleResult->directGlossyTransmit.c);
	if (film->hasChannelEmission)
		VSTORE3F(BLACK, sampleResult->emission.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_REFLECT(film))
		VSTORE3F(BLACK, sampleResult->indirectDiffuseReflect.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_TRANSMIT(film))
		VSTORE3F(BLACK, sampleResult->indirectDiffuseTransmit.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_REFLECT(film))
		VSTORE3F(BLACK, sampleResult->indirectGlossyReflect.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_TRANSMIT(film))
		VSTORE3F(BLACK, sampleResult->indirectGlossyTransmit.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_REFLECT(film))
		VSTORE3F(BLACK, sampleResult->indirectSpecularReflect.c);
	if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_TRANSMIT(film))
		VSTORE3F(BLACK, sampleResult->indirectSpecularTransmit.c);
	sampleResult->rayCount = 0.f;
	if (film->hasChannelIrradiance)
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
		if (film->hasChannelEmission)
			VADD3F(sampleResult->emission.c, radiance);
	} else {
		sampleResult->indirectShadowMask = 0.f;

		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if ((firstPathVertexEvent & (DIFFUSE | REFLECT)) == (DIFFUSE | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_REFLECT(film))
				VADD3F(sampleResult->indirectDiffuseReflect.c, radiance);
		} else if ((firstPathVertexEvent & (DIFFUSE | TRANSMIT)) == (DIFFUSE | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_TRANSMIT(film))
				VADD3F(sampleResult->indirectDiffuseTransmit.c, radiance);
		} else if ((firstPathVertexEvent & (GLOSSY | REFLECT)) == (GLOSSY | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_REFLECT(film))
				VADD3F(sampleResult->indirectGlossyReflect.c, radiance);
		} else if ((firstPathVertexEvent & (GLOSSY | TRANSMIT)) == (GLOSSY | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_TRANSMIT(film))
				VADD3F(sampleResult->indirectGlossyTransmit.c, radiance);
		} else if ((firstPathVertexEvent & (SPECULAR | REFLECT)) == (SPECULAR | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_REFLECT(film))
				VADD3F(sampleResult->indirectSpecularReflect.c, radiance);
		} else if ((firstPathVertexEvent & (SPECULAR | TRANSMIT)) == (SPECULAR | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_TRANSMIT(film))
				VADD3F(sampleResult->indirectSpecularTransmit.c, radiance);
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

		if ((bsdfEvent & (DIFFUSE | REFLECT)) == (DIFFUSE | REFLECT)) {
			if (SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_REFLECT(film))
				VADD3F(sampleResult->directDiffuseReflect.c, radiance);
		} else if ((bsdfEvent & (DIFFUSE | TRANSMIT)) == (DIFFUSE | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_DIRECT_DIFFUSE_TRANSMIT(film))
				VADD3F(sampleResult->directDiffuseTransmit.c, radiance);
		} else if ((bsdfEvent & (GLOSSY | REFLECT)) == (GLOSSY | REFLECT)) {
			if (SAMPLERESULT_NEEDS_DIRECT_GLOSSY_REFLECT(film))
				VADD3F(sampleResult->directGlossyReflect.c, radiance);
		} else if ((bsdfEvent & (GLOSSY | TRANSMIT)) == (GLOSSY | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_DIRECT_GLOSSY_TRANSMIT(film))
				VADD3F(sampleResult->directGlossyTransmit.c, radiance);
		}
	} else {
		sampleResult->indirectShadowMask = fmax(0.f, sampleResult->indirectShadowMask - lightScale);

		const BSDFEvent firstPathVertexEvent = sampleResult->firstPathVertexEvent;
		if ((firstPathVertexEvent & (DIFFUSE | REFLECT)) == (DIFFUSE | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_REFLECT(film))
				VADD3F(sampleResult->indirectDiffuseReflect.c, radiance);
		} else if ((firstPathVertexEvent & (DIFFUSE | TRANSMIT)) == (DIFFUSE | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_DIFFUSE_TRANSMIT(film))
				VADD3F(sampleResult->indirectDiffuseTransmit.c, radiance);
		} else if ((firstPathVertexEvent & (GLOSSY | REFLECT)) == (GLOSSY | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_REFLECT(film))
				VADD3F(sampleResult->indirectGlossyReflect.c, radiance);
		} else if ((firstPathVertexEvent & (GLOSSY | TRANSMIT)) == (GLOSSY | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_GLOSSY_TRANSMIT(film))
				VADD3F(sampleResult->indirectGlossyTransmit.c, radiance);
		} else if ((firstPathVertexEvent & (SPECULAR | REFLECT)) == (SPECULAR | REFLECT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_REFLECT(film))
				VADD3F(sampleResult->indirectSpecularReflect.c, radiance);
		} else if ((firstPathVertexEvent & (SPECULAR | TRANSMIT)) == (SPECULAR | TRANSMIT)) {
			if (SAMPLERESULT_NEEDS_INDIRECT_SPECULAR_TRANSMIT(film))
				VADD3F(sampleResult->indirectSpecularTransmit.c, radiance);
		}

		if (film->hasChannelIrradiance)
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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
