#line 2 "texture_fresnelconst.cl"

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
// FresnelConst texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_FRESNELCONST)

// The following functions are never really used as Metal material has special
// code to evaluate Fresnel texture

float FresnelConstTexture_ConstEvaluateFloat(__global const Texture *tex) {
	return 0.f;
}

float3 FresnelConstTexture_ConstEvaluateSpectrum(__global const Texture *tex) {
	return 0.f;
}

// Note: FresnelConstTexture_Bump() is defined in texture_bump_funcs.cl

#endif
