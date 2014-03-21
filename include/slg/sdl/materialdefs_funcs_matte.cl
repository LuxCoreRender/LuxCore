#line 2 "materialdefs_funcs_matte.cl"

/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
// Matte material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_MATTE)

float3 MatteMaterial_Evaluate(__global Material *material,
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW
		TEXTURES_PARAM_DECL) {
	if (directPdfW)
		*directPdfW = fabs(lightDir.z * M_1_PI_F);

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matte.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	return kd * fabs(lightDir.z * M_1_PI_F);
}

float3 MatteMaterial_Sample(__global Material *material,
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
		float *pdfW, float *cosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent
		TEXTURES_PARAM_DECL) {
	if (!(requestedEvent & (DIFFUSE | REFLECT)) ||
			(fabs(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*sampledDir = (signbit(fixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*cosSampledDir = fabs((*sampledDir).z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = DIFFUSE | REFLECT;

	const float3 kd = Spectrum_Clamp(Texture_GetSpectrumValue(&texs[material->matte.kdTexIndex], hitPoint
			TEXTURES_PARAM));
	return kd;
}

#endif