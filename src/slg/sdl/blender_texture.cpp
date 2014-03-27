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

#include "slg/sdl/sdl.h"
#include "slg/sdl/bsdf.h"
#include "slg/sdl/blender_texture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Blender wood texture
//------------------------------------------------------------------------------

BlenderWoodTexture::BlenderWoodTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoise,
		const float noisesize, float turb, bool hard, float bright, float contrast) : 
		mapping(mp), type(BANDS), noisebasis2(TEX_SIN),  noisesize(noisesize),
		turbulence(turb), hard(hard), bright(bright), contrast(contrast) {
	if(ptype == "bands") {
		type = BANDS;
	} else if(ptype == "rings") {
		type = RINGS;
	} else if(ptype == "bandnoise") {
		type = BANDNOISE;
	} else if(ptype == "ringnoise") {
		type = RINGNOISE;
	};

	if(pnoise == "sin") {
		noisebasis2 = TEX_SIN;
	} else if(pnoise == "saw") {
		noisebasis2 = TEX_SAW;
	} else if(pnoise == "tri") {
		noisebasis2 = TEX_TRI;
	};
}

float BlenderWoodTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);

    float (*waveform[3])(float); /* create array of pointers to waveform functions */
    waveform[0] = tex_sin; /* assign address of tex_sin() function to pointer array */
    waveform[1] = tex_saw;
    waveform[2] = tex_tri;

	u_int wf = 0;
	if(noisebasis2 == TEX_SAW) { 
		wf = 1;
	} else if(noisebasis2 == TEX_TRI) 
		wf = 2;

	float wood = 0.f;
	switch(type) {
		case BANDS:
			wood = waveform[wf]((P.x + P.y + P.z) * 10.f);
			break;
		case RINGS:
			wood = waveform[wf](sqrtf(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f);
			break;
		case BANDNOISE:
			if(hard) wood = turbulence * fabs(2.f * Noise(scale*P) - 1.f);			
			else wood = turbulence * Noise(scale*P);
			wood = waveform[wf]((P.x + P.y + P.z) * 10.f + wood);
			break;
		case RINGNOISE:
			if(hard) wood = turbulence * fabs(2.f * Noise(scale*P) - 1.f);			
			else wood = turbulence * Noise(scale*P);
			wood = waveform[wf](sqrtf(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			break;
	}
    
	wood = (wood - 0.5f) * contrast + bright - 0.5f;
    if(wood < 0.f) wood = 0.f; 
	else if(wood > 1.f) wood = 1.f;
	
    return wood;
}

Spectrum BlenderWoodTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderWoodTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	std::string noise;
	switch(noisebasis2) {
		default:
		case TEX_SIN:
			noise = "sin";
			break;
		case TEX_SAW:
			noise = "saw";
			break;
		case TEX_TRI:
			noise = "tri";
			break;
	}
	std::string woodtype;
	switch(type) {
		default:
		case BANDS:
			woodtype = "bands";
			break;
		case RINGS:
			woodtype = "rings";
			break;
		case BANDNOISE:
			woodtype = "bandnoise";
			break;
		case RINGNOISE:
			woodtype = "ringnoise";
			break;
	}
	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_wood"));
	props.Set(Property("scene.textures." + name + ".woodtype")(woodtype));
	props.Set(Property("scene.textures." + name + ".noisebasis2")(noise));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".noisetype")(noisetype));
	props.Set(Property("scene.textures." + name + ".turbulence")(turbulence));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

BlenderCloudsTexture::BlenderCloudsTexture(const TextureMapping3D *mp, const float noisesize, const int noisedepth, 
		bool hard, float bright, float contrast) : 
		mapping(mp), noisedepth(noisedepth), noisesize(noisesize), hard(hard), bright(bright), contrast(contrast) {
}

float BlenderCloudsTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);

	float clouds = Turbulence(scale*P, noisesize, noisedepth);

	clouds = (clouds - 0.5f) * contrast + bright - 0.5f;
    if(clouds < 0.f) clouds = 0.f; 
	else if(clouds > 1.f) clouds = 1.f;
	
    return clouds;
}

Spectrum BlenderCloudsTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderCloudsTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_clouds"));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".noisedepth")(noisedepth));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender voronoi texture
//------------------------------------------------------------------------------

BlenderVoronoiTexture::BlenderVoronoiTexture(const TextureMapping3D *mp, const float intensity, const float exponent,
        const float fw1, const float fw2, const float fw3, const float fw4, DistanceMetric distmetric, float noisesize,  float bright, float contrast) :
		mapping(mp), intensity(intensity), feature_weight1(fw1), feature_weight2(fw2), feature_weight3(fw3), feature_weight4(fw4),
		distancemetric(distmetric), exponent(exponent),
		noisesize(noisesize), bright(bright), contrast(contrast) {
}

float BlenderVoronoiTexture::GetFloatValue(const HitPoint &hitPoint) const {
    float da[4], pa[12]; /* distance and point coordinate arrays of 4 nearest neighbours */
	luxrays::Point P(mapping->Map(hitPoint));

    const float aw1 = fabs(feature_weight1);
    const float aw2 = fabs(feature_weight2);
    const float aw3 = fabs(feature_weight3);
    const float aw4 = fabs(feature_weight4);

    float sc = (aw1 + aw2 + aw3 + aw4);

    if (sc > 1e-3f) sc = noisesize / sc;

    float result = 1.f;

	voronoi(P, da, pa, exponent, distancemetric);
    result = sc * fabs(feature_weight1 * da[0] + feature_weight2 * da[1] + feature_weight3 * da[2] + feature_weight4 * da[3]);

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f; 
	else if(result > 1.f) result = 1.f;

    return result;
}

Spectrum BlenderVoronoiTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderVoronoiTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_voronoi"));
	props.Set(Property("scene.textures." + name + ".intentity")(intensity));
	props.Set(Property("scene.textures." + name + ".exponent")(exponent));
	props.Set(Property("scene.textures." + name + ".w1")(feature_weight1));
	props.Set(Property("scene.textures." + name + ".w2")(feature_weight2));
	props.Set(Property("scene.textures." + name + ".w3")(feature_weight3));
	props.Set(Property("scene.textures." + name + ".w4")(feature_weight4));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
