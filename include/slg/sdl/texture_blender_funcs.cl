#line 2 "texture_blender_funcs.cl"

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

#ifndef TEXTURE_STACK_SIZE
#define TEXTURE_STACK_SIZE 16
#endif

//------------------------------------------------------------------------------
// Wood texture
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_WOOD)

float WoodTexture_Evaluate(__global Texture *texture, __global HitPoint *hitPoint){
	const float3 P = TextureMapping3D_Map(&texture->wood.mapping, hitPoint);
	float scale = 1.f;
	if(fabs(texture->wood.noisesize) > 0.00001f) scale = (1.f/texture->wood.noisesize);	

	const NoiseBase noise = texture->wood.noisebasis2;
	float wood = 0.0f;

	switch(texture->wood.type) {
		default:
		case BANDS:
			if(noise == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f);
			} else if(noise == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f);
			}
			break;
		case RINGS:
			if(noise == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else if(noise == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			}
			break;
		case BANDNOISE:			
			if(texture->wood.hard)	
				wood = texture->wood.turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = texture->wood.turbulence * Noise3(scale*P);

			if(noise == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f + wood);
			} else if(noise == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f + wood);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f + wood);
			}
			break;
		case RINGNOISE:
			if(texture->wood.hard)	
				wood = texture->wood.turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = texture->wood.turbulence * Noise3(scale*P);

			if(noise == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else if(noise == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			}
			break;
	}
	wood = (wood - 0.5f) * texture->wood.contrast + texture->wood.bright - 0.5f;
	if(wood < 0.f) wood = 0.f;
	else if(wood > 1.f) wood = 1.f;

	return wood;
}

void WoodTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = WoodTexture_Evaluate(texture, hitPoint);
}

void WoodTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
		float wood = WoodTexture_Evaluate(texture, hitPoint);
	texValues[(*texValuesSize)++] = (float3)(wood, wood, wood);
}

void WoodTexture_EvaluateDuDv(__global Texture *texture, __global HitPoint *hitPoint,
		float2 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = (float2)(DUDV_VALUE, DUDV_VALUE);
}

#endif