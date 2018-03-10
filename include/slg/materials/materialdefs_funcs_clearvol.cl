#line 2 "materialdefs_funcs_clearvol.cl"

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
// ClearVol material
//
// ClearVol hasn't scattering so none of the below functions is really used.
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_CLEAR_VOL)

OPENCL_FORCE_INLINE BSDFEvent ClearVolMaterial_GetEventTypes() {
	return DIFFUSE | REFLECT;
}

OPENCL_FORCE_INLINE float3 ClearVolMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir,
		BSDFEvent *event, float *directPdfW) {
	return BLACK;
}

OPENCL_FORCE_INLINE float3 ClearVolMaterial_Sample(
		__global HitPoint *hitPoint, const float3 fixedDir, float3 *sampledDir,
		const float u0, const float u1, 
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *cosSampledDir, BSDFEvent *event) {
	return BLACK;
}

#endif
