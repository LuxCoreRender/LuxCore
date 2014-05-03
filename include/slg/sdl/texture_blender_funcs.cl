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
// Blender wood texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_WOOD)

float BlenderWoodTexture_Evaluate(__global Texture *texture, __global HitPoint *hitPoint){
	const float3 P = TextureMapping3D_Map(&texture->blenderWood.mapping, hitPoint);
	float scale = 1.f;
	if(fabs(texture->blenderWood.noisesize) > 0.00001f) scale = (1.f/texture->blenderWood.noisesize);

	const BlenderWoodNoiseBase noise = texture->blenderWood.noisebasis2;
	float wood = 0.f;

	switch(texture->blenderWood.type) {
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
			if(texture->blenderWood.hard)	
				wood = texture->blenderWood.turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = texture->blenderWood.turbulence * Noise3(scale*P);

			if(noise == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f + wood);
			} else if(noise == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f + wood);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f + wood);
			}
			break;
		case RINGNOISE:
			if(texture->blenderWood.hard)	
				wood = texture->blenderWood.turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = texture->blenderWood.turbulence * Noise3(scale*P);

			if(noise == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else if(noise == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			}
			break;
	}
	wood = (wood - 0.5f) * texture->blenderWood.contrast + texture->blenderWood.bright - 0.5f;
	wood = clamp(wood, 0.f, 1.f);

	return wood;
}

float BlenderWoodTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return BlenderWoodTexture_Evaluate(texture, hitPoint);
}

float3 BlenderWoodTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
    const float wood = BlenderWoodTexture_Evaluate(texture, hitPoint);

    return (float3)(wood, wood, wood);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BlenderWoodTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderWoodTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void BlenderWoodTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
	float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
    texValues[(*texValuesSize)++] = BlenderWoodTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_CLOUDS)

float BlenderCloudsTexture_Evaluate(__global Texture *texture, __global HitPoint *hitPoint) {
	const float3 P = TextureMapping3D_Map(&texture->blenderClouds.mapping, hitPoint);

	float scale = 1.f;
	if(fabs(texture->blenderClouds.noisesize) > 0.00001f) scale = (1.f/texture->blenderClouds.noisesize);

	float clouds = Turbulence(scale*P, texture->blenderClouds.noisesize, texture->blenderClouds.noisedepth);

	clouds = (clouds - 0.5f) * texture->blenderClouds.contrast + texture->blenderClouds.bright - 0.5f;
	clouds = clamp(clouds, 0.f, 1.f);

	return clouds;
}

float BlenderCloudsTexture_DynamicEvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint) {
	return BlenderCloudsTexture_Evaluate(texture, hitPoint);
}

float3 BlenderCloudsTexture_DynamicEvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint) {
	const float clouds = BlenderCloudsTexture_Evaluate(texture, hitPoint);

	return (float3)(clouds, clouds, clouds);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BlenderCloudsTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderCloudsTexture_DynamicEvaluateFloat(texture, hitPoint);
}

void BlenderCloudsTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderCloudsTexture_DynamicEvaluateSpectrum(texture, hitPoint);
}
#endif

#endif