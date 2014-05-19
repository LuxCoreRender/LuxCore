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

float BlenderWoodTexture_Evaluate(__global HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderWoodNoiseBase noisebasis2,
		const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global TextureMapping3D *mapping) {
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);
	float scale = 1.f;
	if (fabs(noisesize) > 0.00001f) scale = (1.f / noisesize);

	float wood = 0.f;
	switch(type) {
		default:
		case BANDS:
			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f);
			}
			break;
		case RINGS:
			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			}
			break;
		case BANDNOISE:			
			if(hard)	
				wood = turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = turbulence * Noise3(scale*P);

			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin((P.x + P.y + P.z) * 10.f + wood);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw((P.x + P.y + P.z) * 10.f + wood);
			} else {
				wood = tex_tri((P.x + P.y + P.z) * 10.f + wood);
			}
			break;
		case RINGNOISE:
			if(hard)	
				wood = turbulence * fabs(2.f * Noise3(scale*P) - 1.f);
			else
				wood = turbulence * Noise3(scale*P);

			if(noisebasis2 == TEX_SIN) {
				wood = tex_sin(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else if(noisebasis2 == TEX_SAW) {
				wood = tex_saw(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			} else {
				wood = tex_tri(sqrt(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			}
			break;
	}
	wood = (wood - 0.5f) * contrast + bright - 0.5f;
	wood = clamp(wood, 0.f, 1.f);

	return wood;
}

float BlenderWoodTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderWoodNoiseBase noisebasis2,
		const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global TextureMapping3D *mapping) {
	return BlenderWoodTexture_Evaluate(hitPoint, type, noisebasis2,
		noisesize, turbulence, contrast, bright, hard, mapping);
}

float3 BlenderWoodTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const BlenderWoodType type, const BlenderWoodNoiseBase noisebasis2,
		const float noisesize, const float turbulence,
		const float contrast, const float bright, const bool hard,
		__global TextureMapping3D *mapping) {
    return BlenderWoodTexture_Evaluate(hitPoint, type, noisebasis2,
		noisesize, turbulence, contrast, bright, hard, mapping);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BlenderWoodTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderWoodTexture_ConstEvaluateFloat(hitPoint,
			texture->blenderWood.type, texture->blenderWood.noisebasis2,
			texture->blenderWood.noisesize, texture->blenderWood.turbulence, 
			texture->blenderWood.contrast, texture->blenderWood.bright,
			texture->blenderWood.hard, &texture->blenderWood.mapping);
}

void BlenderWoodTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
	float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
    texValues[(*texValuesSize)++] = BlenderWoodTexture_ConstEvaluateSpectrum(hitPoint,
			texture->blenderWood.type, texture->blenderWood.noisebasis2,
			texture->blenderWood.noisesize, texture->blenderWood.turbulence, 
			texture->blenderWood.contrast, texture->blenderWood.bright,
			texture->blenderWood.hard, &texture->blenderWood.mapping);
}
#endif

#endif

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

#if defined(PARAM_ENABLE_BLENDER_CLOUDS)

float BlenderCloudsTexture_Evaluate(__global HitPoint *hitPoint,
		const float noisesize, const int noisedepth, const float contrast,
		const float bright, __global TextureMapping3D *mapping) {
	const float3 P = TextureMapping3D_Map(mapping, hitPoint);

	float scale = 1.f;
	if (fabs(noisesize) > 0.00001f) scale = (1.f / noisesize);

	float clouds = Turbulence(scale*P, noisesize, noisedepth);

	clouds = (clouds - 0.5f) * contrast + bright - 0.5f;
	clouds = clamp(clouds, 0.f, 1.f);

	return clouds;
}

float BlenderCloudsTexture_ConstEvaluateFloat(__global HitPoint *hitPoint,
		const float noisesize, const int noisedepth, const float contrast,
		const float bright, __global TextureMapping3D *mapping) {
	return BlenderCloudsTexture_Evaluate(hitPoint, noisesize, noisedepth,
			contrast, bright, mapping);
}

float3 BlenderCloudsTexture_ConstEvaluateSpectrum(__global HitPoint *hitPoint,
		const float noisesize, const int noisedepth, const float contrast,
		const float bright, __global TextureMapping3D *mapping) {
	return BlenderCloudsTexture_Evaluate(hitPoint, noisesize, noisedepth,
			contrast, bright, mapping);
}

#if defined(PARAM_DIASBLE_TEX_DYNAMIC_EVALUATION)
void BlenderCloudsTexture_EvaluateFloat(__global Texture *texture, __global HitPoint *hitPoint,
		float texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderCloudsTexture_ConstEvaluateFloat(hitPoint,
			texture->blenderClouds.noisesize, texture->blenderClouds.noisedepth,
			texture->blenderClouds.contrast, texture->blenderClouds.bright,
			&texture->blenderClouds.mapping);
}

void BlenderCloudsTexture_EvaluateSpectrum(__global Texture *texture, __global HitPoint *hitPoint,
		float3 texValues[TEXTURE_STACK_SIZE], uint *texValuesSize) {
	texValues[(*texValuesSize)++] = BlenderCloudsTexture_ConstEvaluateSpectrum(hitPoint,
			texture->blenderClouds.noisesize, texture->blenderClouds.noisedepth,
			texture->blenderClouds.contrast, texture->blenderClouds.bright,
			&texture->blenderClouds.mapping);
}
#endif

#endif