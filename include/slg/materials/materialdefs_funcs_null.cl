#line 2 "materialdefs_funcs_null.cl"

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

//------------------------------------------------------------------------------
// NULL material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_NULL)

BSDFEvent NullMaterial_GetEventTypes() {
	return SPECULAR | TRANSMIT;
}

bool NullMaterial_IsDelta() {
	return true;
}

#if defined(PARAM_HAS_PASSTHROUGH)
float3 NullMaterial_GetPassThroughTransparency(__global const Material *material,
		__global HitPoint *hitPoint, const float3 localFixedDir, const float passThroughEvent
		TEXTURES_PARAM_DECL) {
	const uint transpTexIndex = material->transpTexIndex;

	if (transpTexIndex != NULL_INDEX) {
		const float3 blendColor = clamp(
			Texture_GetSpectrumValue(transpTexIndex, hitPoint
				TEXTURES_PARAM),
			0.f, 1.f);

		if (Spectrum_IsBlack(blendColor)) {
			// It doesn't make any sense to have a solid NULL material
			return .0001f;
		} else
			return blendColor;
	} else
		return WHITE;
}
#endif

float3 NullMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW) {
	return BLACK;
}

float3 NullMaterial_Sample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	*sampledDir = -fixedDir;
	*cosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return WHITE;
}

#endif
