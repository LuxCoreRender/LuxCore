#line 2 "texture_irregulardata_funcs.cl"

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
// IrregularData texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_TEX_IRREGULARDATA)

OPENCL_FORCE_INLINE float IrregularDataTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float3 rgb) {
	return Spectrum_Y(rgb);
}

OPENCL_FORCE_INLINE float3 IrregularDataTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float3 rgb) {
	return rgb;
}

#endif
