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

#include <limits>

#include "slg/core/sdl.h"
#include "slg/bsdf/bsdf.h"
#include "slg/textures/blender_texture.h"

using namespace luxrays;
using namespace slg;
using namespace slg::blender;

//------------------------------------------------------------------------------
// Blender blend texture
//------------------------------------------------------------------------------

BlenderBlendTexture::BlenderBlendTexture(const TextureMapping3D *mp, const std::string ptype,
										 const bool direction, float bright, float contrast) :
		mapping(mp), type(TEX_LIN), direction(direction), bright(bright), contrast(contrast) {

		if (ptype == "linear") {type = TEX_LIN;}
		else if (ptype == "quadratic") {type = TEX_QUAD;}
		else if (ptype == "easing") {type = TEX_EASE;}
		else if (ptype == "diagonal") {type = TEX_DIAG;}
		else if (ptype == "spherical") {type = TEX_SPHERE;}
		else if (ptype == "halo") {type = TEX_HALO;}
		else if (ptype == "radial") {type = TEX_RAD;};
}

float BlenderBlendTexture::GetFloatValue(const HitPoint &hitPoint) const {
	float result = 0.f;
	Point P(mapping->Map(hitPoint));

	float x, y, t;

	if(!direction) {
		//horizontal
		x = P.x;
		y = P.y;
	} else {
		//vertical
		x = P.y;
		y = P.x;
	};

    if (type == TEX_LIN) { /* lin */
        result = (1.f + x) / 2.f;
    } else if (type == TEX_QUAD) { /* quad */
        result = (1.f + x) / 2.f;
        if (result < 0.f) result = 0.f;
        else result *= result;
    } else if (type == TEX_EASE) { /* ease */
        result = (1.f + x) / 2.f;
        if (result <= 0.f) result = 0.f;
        else if (result >= 1.f) result = 1.f;
        else {
            t = result * result;
            result = (3.f * t - 2.f * t * result);
        }
    } else if (type == TEX_DIAG) { /* diag */
        result = (2.f + x + y) / 4.f;
    } else if (type == TEX_RAD) { /* radial */
        result = (atan2f(y, x) / (2.f * M_PI) + 0.5f);
    } else { /* sphere TEX_SPHERE */
        result = 1.f - sqrt(x * x + y * y + P.z * P.z);
        if (result < 0.f) result = 0.f;
        if (type == TEX_HALO) result *= result; /* halo */
    }

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

	return result;
}

Spectrum BlenderBlendTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderBlendTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	std::string progressiontype;
	switch(type) {
		default:
		case TEX_LIN:
			progressiontype = "linear";
			break;
		case TEX_QUAD:
			progressiontype = "quadratic";
			break;
		case TEX_EASE:
			progressiontype = "easing";
			break;
		case TEX_DIAG:
			progressiontype = "diagonal";
			break;
		case TEX_SPHERE:
			progressiontype = "spherical";
			break;
		case TEX_HALO:
			progressiontype = "halo";
			break;
		case TEX_RAD:
			progressiontype = "radial";
			break;
	}
	std::string directiontype = "horizontal";
	if(direction) directiontype = "vertical";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_blend"));
	props.Set(Property("scene.textures." + name + ".progressiontype")(progressiontype));
	props.Set(Property("scene.textures." + name + ".direction")(directiontype));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender clouds texture
//------------------------------------------------------------------------------

BlenderCloudsTexture::BlenderCloudsTexture(const TextureMapping3D *mp, const std::string &pnoisebasis, const float noisesize, const int noisedepth,
		bool hard, float bright, float contrast) :
		mapping(mp), noisebasis(BLENDER_ORIGINAL), noisedepth(noisedepth), noisesize(noisesize),
		hard(hard), bright(bright), contrast(contrast) {

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};
}

float BlenderCloudsTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));

	float clouds = BLI_gTurbulence(noisesize, P.x, P.y, P.z, noisedepth, hard, noisebasis);

	clouds = (clouds - 0.5f) * contrast + bright - 0.5f;
	clouds = Clamp(clouds, 0.f, 1.f);

    return clouds;
}

Spectrum BlenderCloudsTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderCloudsTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	std::string nbas;
	switch(noisebasis) {
		default:
		case BLENDER_ORIGINAL:
			nbas = "blender_original";
			break;
		case ORIGINAL_PERLIN:
			nbas = "original_perlin";
			break;
		case IMPROVED_PERLIN:
			nbas = "improved_perlin";
			break;
		case VORONOI_F1:
			nbas = "voronoi_f1";
			break;
		case VORONOI_F2:
			nbas = "voronoi_f2";
			break;
		case VORONOI_F3:
			nbas = "voronoi_f3";
			break;
		case VORONOI_F4:
			nbas = "voronoi_f4";
			break;
		case VORONOI_F2_F1:
			nbas = "voronoi_f2_f1";
			break;
		case VORONOI_CRACKLE:
			nbas = "voronoi_crackle";
			break;
		case CELL_NOISE:
			nbas = "cell_noise";
			break;
	}

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_clouds"));
	props.Set(Property("scene.textures." + name + ".noisetype")(noisetype));
	props.Set(Property("scene.textures." + name + ".noisebasis")(nbas));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".noisedepth")(noisedepth));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender distorted noise texture
//------------------------------------------------------------------------------

BlenderDistortedNoiseTexture::BlenderDistortedNoiseTexture(const TextureMapping3D *mp, const std::string &pnoisedistortion,
		const std::string &pnoisebasis, float distortion, float noisesize, float bright, float contrast) :
		mapping(mp), noisedistortion(BLENDER_ORIGINAL), noisebasis(BLENDER_ORIGINAL), distortion(distortion), noisesize(noisesize),
		bright(bright), contrast(contrast) {

	if(pnoisedistortion == "blender_original") {
		noisedistortion = BLENDER_ORIGINAL;
	} else if(pnoisedistortion == "original_perlin") {
		noisedistortion = ORIGINAL_PERLIN;
	} else if(pnoisedistortion == "improved_perlin") {
		noisedistortion = IMPROVED_PERLIN;
	} else if(pnoisedistortion == "voronoi_f1") {
		noisedistortion = VORONOI_F1;
	} else if(pnoisedistortion == "voronoi_f2") {
		noisedistortion = VORONOI_F2;
	} else if(pnoisedistortion == "voronoi_f3") {
		noisedistortion = VORONOI_F3;
	} else if(pnoisedistortion == "voronoi_f4") {
		noisedistortion = VORONOI_F4;
	} else if(pnoisedistortion == "voronoi_f2_f1") {
		noisedistortion = VORONOI_F2_F1;
	} else if(pnoisedistortion == "voronoi_crackle") {
		noisedistortion = VORONOI_CRACKLE;
	} else if(pnoisedistortion == "cell_noise") {
		noisedistortion = CELL_NOISE;
	};

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};

}

float BlenderDistortedNoiseTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	float result = 0.f;
	float scale = 1.f;

	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	P *= scale;

	result = mg_VLNoise(P.x, P.y, P.z, distortion, noisebasis, noisedistortion);

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

	return result;
}

Spectrum BlenderDistortedNoiseTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderDistortedNoiseTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_distortednoise"));
	props.Set(Property("scene.textures." + name + ".noisebasis")(noisebasis));
	props.Set(Property("scene.textures." + name + ".noise_distortion")(noisedistortion));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".distortion")(distortion));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender magic texture
//------------------------------------------------------------------------------

BlenderMagicTexture::BlenderMagicTexture(const TextureMapping3D *mp, const int noisedepth,
										 const float turbulence, float bright, float contrast) :
		mapping(mp), noisedepth(noisedepth), turbulence(turbulence), bright(bright), contrast(contrast) {

}

float BlenderMagicTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BlenderMagicTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	Spectrum s;

    float x, y, z, turb = 1.f;
    float r, g, b;
    int n;

    n = noisedepth;
    turb = turbulence / 5.f;

    x = sin((P.x + P.y + P.z)*5.f);
    y = cos((-P.x + P.y - P.z)*5.f);
    z = -cos((-P.x - P.y + P.z)*5.f);
    if (n > 0) {
        x *= turb;
        y *= turb;
        z *= turb;
        y = -cos(x - y + z);
        y *= turb;
        if (n > 1) {
            x = cos(x - y - z);
            x *= turb;
            if (n > 2) {
                z = sin(-x - y - z);
                z *= turb;
                if (n > 3) {
                    x = -cos(-x + y - z);
                    x *= turb;
                    if (n > 4) {
                        y = -sin(-x + y + z);
                        y *= turb;
                        if (n > 5) {
                            y = -cos(-x + y + z);
                            y *= turb;
                            if (n > 6) {
                                x = cos(x + y + z);
                                x *= turb;
                                if (n > 7) {
                                    z = sin(x + y - z);
                                    z *= turb;
                                    if (n > 8) {
                                        x = -cos(-x - y + z);
                                        x *= turb;
                                        if (n > 9) {
                                            y = -sin(x - y + z);
                                            y *= turb;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    if (turb != 0.f) {
        turb *= 2.f;
        x /= turb;
        y /= turb;
        z /= turb;
    }
    r = 0.5f - x;
    g = 0.5f - y;
    b = 0.5f - z;

	r = (r - 0.5f) * contrast + bright - 0.5f;
    if(r < 0.f) r = 0.f;
	else if(r > 1.f) r = 1.f;

	g = (g - 0.5f) * contrast + bright - 0.5f;
    if(g < 0.f) g = 0.f;
	else if(g > 1.f) g = 1.f;

	b = (b - 0.5f) * contrast + bright - 0.5f;
    if(b < 0.f) b = 0.f;
	else if(b > 1.f) b = 1.f;

	return Spectrum(RGBColor(r,g,b));
}

float BlenderMagicTexture::Y() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	luxrays::Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += luxrays::Spectrum(c[i]);
	return cs.Y() / NC;
#undef NC
}

float BlenderMagicTexture::Filter() const {
	static float c[][3] = { { .58f, .58f, .6f }, { .58f, .58f, .6f }, { .58f, .58f, .6f },
		{ .5f, .5f, .5f }, { .6f, .59f, .58f }, { .58f, .58f, .6f },
		{ .58f, .58f, .6f }, {.2f, .2f, .33f }, { .58f, .58f, .6f }, };
	luxrays::Spectrum cs;
#define NC  sizeof(c) / sizeof(c[0])
	for (u_int i = 0; i < NC; ++i)
		cs += luxrays::Spectrum(c[i]);
	return cs.Filter() / NC;
#undef NC
}

Properties BlenderMagicTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;
	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_magic"));
	props.Set(Property("scene.textures." + name + ".noisedepth")(noisedepth));
	props.Set(Property("scene.textures." + name + ".turbulence")(turbulence));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}
//------------------------------------------------------------------------------
// Blender marble texture
//------------------------------------------------------------------------------

BlenderMarbleTexture::BlenderMarbleTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis,
		const std::string &pnoise, float noisesize, float turb, int noisedepth, bool hard, float bright, float contrast) :
		mapping(mp), type(TEX_SOFT), noisebasis(BLENDER_ORIGINAL), noisebasis2(TEX_SIN), noisesize(noisesize),
		turbulence(turb), noisedepth(noisedepth), hard(hard), bright(bright), contrast(contrast) {

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};

	if(ptype == "soft") {
		type = TEX_SOFT;
	} else if(ptype == "sharp") {
		type = TEX_SHARP;
	} else if(ptype == "sharper") {
		type = TEX_SHARPER;
	};

	if(pnoise == "sin") {
		noisebasis2 = TEX_SIN;
	} else if(pnoise == "saw") {
		noisebasis2 = TEX_SAW;
	} else if(pnoise == "tri") {
		noisebasis2 = TEX_TRI;
	};
}

float BlenderMarbleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));

    float (*waveform[3])(float); /* create array of pointers to waveform functions */
    waveform[0] = tex_sin; /* assign address of tex_sin() function to pointer array */
    waveform[1] = tex_saw;
    waveform[2] = tex_tri;

	u_int wf = 0;

	if(noisebasis2 == TEX_SAW) {
		wf = 1;
	} else if(noisebasis2 == TEX_TRI)
		wf = 2;

	float result = 0.f;

    const float n = 5.f * (P.x + P.y + P.z);

    result = n + turbulence * BLI_gTurbulence(noisesize, P.x, P.y, P.z, noisedepth, hard, noisebasis);
	result = waveform[wf](result);

	if (type == TEX_SHARP) {
		result = sqrtf(result);
	} else if (type == TEX_SHARPER) {
		result = sqrtf(sqrtf(result));
	}

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

    return result;
}

Spectrum BlenderMarbleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderMarbleTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	std::string nbas;
	switch(noisebasis) {
		default:
		case BLENDER_ORIGINAL:
			nbas = "blender_original";
			break;
		case ORIGINAL_PERLIN:
			nbas = "original_perlin";
			break;
		case IMPROVED_PERLIN:
			nbas = "improved_perlin";
			break;
		case VORONOI_F1:
			nbas = "voronoi_f1";
			break;
		case VORONOI_F2:
			nbas = "voronoi_f2";
			break;
		case VORONOI_F3:
			nbas = "voronoi_f3";
			break;
		case VORONOI_F4:
			nbas = "voronoi_f4";
			break;
		case VORONOI_F2_F1:
			nbas = "voronoi_f2_f1";
			break;
		case VORONOI_CRACKLE:
			nbas = "voronoi_crackle";
			break;
		case CELL_NOISE:
			nbas = "cell_noise";
			break;
	}

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

	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_marble"));
	props.Set(Property("scene.textures." + name + ".noisebasis")(nbas));
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
// Blender musgrave texture
//------------------------------------------------------------------------------

BlenderMusgraveTexture::BlenderMusgraveTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis,
		const float dimension, const float intensity, const float lacunarity, const float offset, const float gain,
		const float octaves, float noisesize, float bright, float contrast) :
		mapping(mp), type(TEX_MULTIFRACTAL), noisebasis(BLENDER_ORIGINAL), dimension(dimension), intensity(intensity),
		lacunarity(lacunarity), offset(offset), gain(gain), octaves(octaves),
		noisesize(noisesize), bright(bright), contrast(contrast) {

	if(ptype == "multifractal") {
		type = TEX_MULTIFRACTAL;
	} else if(ptype == "ridged_multifractal") {
		type = TEX_RIDGED_MULTIFRACTAL;
	} else if(ptype == "hybrid_multifractal") {
		type = TEX_HYBRID_MULTIFRACTAL;
	} else if(ptype == "fBM") {
		type = TEX_FBM;
	} else if(ptype == "hetero_terrain") {
		type = TEX_HETERO_TERRAIN;
	};

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};
}

float BlenderMusgraveTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));

	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	P *= scale;

	float result = 0.f;

	switch (type) {
		case TEX_MULTIFRACTAL:
			result = mg_MultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, noisebasis);
			break;
		case TEX_FBM:
			result = mg_fBm(P.x, P.y, P.z, dimension, lacunarity, octaves, noisebasis);
            break;
		case TEX_RIDGED_MULTIFRACTAL:
			result = mg_RidgedMultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, gain, noisebasis);
			break;
        case TEX_HYBRID_MULTIFRACTAL:
			result = mg_HybridMultiFractal(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, gain, noisebasis);
			break;
        case TEX_HETERO_TERRAIN:
			result = mg_HeteroTerrain(P.x, P.y, P.z, dimension, lacunarity, octaves, offset, noisebasis);
			break;
    };

	result *= intensity;

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

    return result;
}

Spectrum BlenderMusgraveTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderMusgraveTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	std::string nbas;
	switch(noisebasis) {
		default:
		case BLENDER_ORIGINAL:
			nbas = "blender_original";
			break;
		case ORIGINAL_PERLIN:
			nbas = "original_perlin";
			break;
		case IMPROVED_PERLIN:
			nbas = "improved_perlin";
			break;
		case VORONOI_F1:
			nbas = "voronoi_f1";
			break;
		case VORONOI_F2:
			nbas = "voronoi_f2";
			break;
		case VORONOI_F3:
			nbas = "voronoi_f3";
			break;
		case VORONOI_F4:
			nbas = "voronoi_f4";
			break;
		case VORONOI_F2_F1:
			nbas = "voronoi_f2_f1";
			break;
		case VORONOI_CRACKLE:
			nbas = "voronoi_crackle";
			break;
		case CELL_NOISE:
			nbas = "cell_noise";
			break;
	}

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_musgrave"));
	props.Set(Property("scene.textures." + name + ".musgravetype")(type));
	props.Set(Property("scene.textures." + name + ".noisebasis")(nbas));
	props.Set(Property("scene.textures." + name + ".dimension")(dimension));
	props.Set(Property("scene.textures." + name + ".intensity")(intensity));
	props.Set(Property("scene.textures." + name + ".lacunarity")(lacunarity));
	props.Set(Property("scene.textures." + name + ".offset")(offset));
	props.Set(Property("scene.textures." + name + ".gain")(gain));
	props.Set(Property("scene.textures." + name + ".octaves")(octaves));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender noise texture
//------------------------------------------------------------------------------

BlenderNoiseTexture::BlenderNoiseTexture(int noisedepth, float bright, float contrast) :
		noisedepth(noisedepth), bright(bright), contrast(contrast) {
}

float BlenderNoiseTexture::GetFloatValue(const HitPoint &hitPoint) const {
	/* The original Blender code was not thread-safe
	float result = 0.f;

	float div = 3.f;
    int val, ran, loop;

    ran = BLI_rand();
    val = (ran & 3);

    loop = noisedepth;
    while (loop--) {
        ran = (ran >> 2);
        val *= (ran & 3);
        div *= 3.f;
    }*/

	float result = 0.f;

	float div = 3.f;

	float ran = hitPoint.passThroughEvent;
	float val = Floor2UInt(ran * 4.f);
	
	int loop = noisedepth;
    while (loop--) {
		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		ran = fabsf(ran - .5f) * 2.f;

        val *= Floor2UInt(ran * 4.f);
        div *= 3.f;		
    }

	result = ((float) val) / div;
	
	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

	return result;
}

Spectrum BlenderNoiseTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderNoiseTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_noise"));
	props.Set(Property("scene.textures." + name + ".noisedepth")(noisedepth));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	return props;
}

//------------------------------------------------------------------------------
// Blender stucci texture
//------------------------------------------------------------------------------

BlenderStucciTexture::BlenderStucciTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoisebasis,
		const float noises, float turb, bool hard, float bright, float contrast) :
		mapping(mp), type(TEX_PLASTIC), noisebasis(BLENDER_ORIGINAL), noisesize(noises),
		turbulence(turb), hard(hard), bright(bright), contrast(contrast) {

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};

	if(ptype == "plastic") {
		type = TEX_PLASTIC;
	} else if(ptype == "wall_in") {
		type = TEX_WALL_IN;
	} else if(ptype == "wall_out") {
		type = TEX_WALL_OUT;
	};
}

float BlenderStucciTexture::GetFloatValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));

	float result = 0.f;
    float nor[3], b2, ofs;

    b2 = BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);

    ofs = turbulence / 200.f;

	if (type != TEX_PLASTIC) ofs *= (b2 * b2);
//	neo2068: only nor{2] is used
//    nor[0] = BLI_gNoise(noisesize, P.x + ofs, P.y, P.z, hard, noisebasis);
//    nor[1] = BLI_gNoise(noisesize, P.x, P.y + ofs, P.z, hard, noisebasis);
    nor[2] = BLI_gNoise(noisesize, P.x, P.y, P.z + ofs, hard, noisebasis);

    result = nor[2];

    if (type == TEX_WALL_OUT)
        result = 1.f - result;

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

    return result;
}

Spectrum BlenderStucciTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderStucciTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	std::string nbas;
	switch(noisebasis) {
		default:
		case BLENDER_ORIGINAL:
			nbas = "blender_original";
			break;
		case ORIGINAL_PERLIN:
			nbas = "original_perlin";
			break;
		case IMPROVED_PERLIN:
			nbas = "improved_perlin";
			break;
		case VORONOI_F1:
			nbas = "voronoi_f1";
			break;
		case VORONOI_F2:
			nbas = "voronoi_f2";
			break;
		case VORONOI_F3:
			nbas = "voronoi_f3";
			break;
		case VORONOI_F4:
			nbas = "voronoi_f4";
			break;
		case VORONOI_F2_F1:
			nbas = "voronoi_f2_f1";
			break;
		case VORONOI_CRACKLE:
			nbas = "voronoi_crackle";
			break;
		case CELL_NOISE:
			nbas = "cell_noise";
			break;
	}

	std::string stuccitype;
	switch(type) {
		default:
		case TEX_PLASTIC:
			stuccitype = "plastic";
			break;
		case TEX_WALL_IN:
			stuccitype = "wall_in";
			break;
		case TEX_WALL_OUT:
			stuccitype = "wall_out";
			break;
	}

	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_stucci"));
	props.Set(Property("scene.textures." + name + ".stuccitype")(stuccitype));
	props.Set(Property("scene.textures." + name + ".noisebasis")(nbas));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".noisetype")(noisetype));
	props.Set(Property("scene.textures." + name + ".turbulence")(turbulence));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Blender voronoi texture
//------------------------------------------------------------------------------

BlenderVoronoiTexture::BlenderVoronoiTexture(const TextureMapping3D *mp, const float intensity, const float exponent,
        const float fw1, const float fw2, const float fw3, const float fw4, const std::string distmetric, float noisesize,  float bright, float contrast) :
		mapping(mp), intensity(intensity), feature_weight1(fw1), feature_weight2(fw2), feature_weight3(fw3), feature_weight4(fw4),
		distancemetric(ACTUAL_DISTANCE), exponent(exponent), noisesize(noisesize), bright(bright), contrast(contrast) {

	if(distmetric == "actual_distance") {
		distancemetric = ACTUAL_DISTANCE;
	} else if(distmetric == "distance_squared") {
		distancemetric = DISTANCE_SQUARED;
	} else if(distmetric == "manhattan") {
		distancemetric = MANHATTAN;
	} else if(distmetric == "chebychev") {
		distancemetric = CHEBYCHEV;
	} else if(distmetric == "minkowski_half") {
		distancemetric = MINKOWSKI_HALF;
	} else if(distmetric == "minkowski_four") {
		distancemetric = MINKOWSKI_FOUR;
	} else if(distmetric == "minkowski") {
		distancemetric = MINKOWSKI;
	};
}

float BlenderVoronoiTexture::GetFloatValue(const HitPoint &hitPoint) const {
    float da[4], pa[12]; /* distance and point coordinate arrays of 4 nearest neighbours */
	luxrays::Point P(mapping->Map(hitPoint));

	float scale = 1.f;
	if(fabs(noisesize) > 0.00001f) scale = (1.f/noisesize);
	P *= scale;

	const float aw1 = fabs(feature_weight1);
    const float aw2 = fabs(feature_weight2);
    const float aw3 = fabs(feature_weight3);
    const float aw4 = fabs(feature_weight4);

    float sc = (aw1 + aw2 + aw3 + aw4);

    if (sc > 0.00001f) sc = intensity / sc;

    float result = 1.f;

	voronoi(P.x, P.y, P.z, da, pa, exponent, distancemetric);
    result = sc * fabs(feature_weight1 * da[0] + feature_weight2 * da[1] + feature_weight3 * da[2] + feature_weight4 * da[3]);

	result = (result - 0.5f) * contrast + bright - 0.5f;
    if(result < 0.f) result = 0.f;
	else if(result > 1.f) result = 1.f;

    return result;
}

Spectrum BlenderVoronoiTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderVoronoiTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	Properties props;

	const std::string name = GetName();

	std::string dm = "";
	switch(distancemetric) {
		case DISTANCE_SQUARED:
			dm = "distance_squared";
			break;
		case MANHATTAN:
			dm = "manhattan";
			break;
		case CHEBYCHEV:
			dm = "chebychev";
			break;
		case MINKOWSKI_HALF:
			dm = "minkowski_half";
			break;
		case MINKOWSKI_FOUR:
			dm = "minkowski_four";
			break;
		case MINKOWSKI:
			dm = "minkowski";
			break;
		case ACTUAL_DISTANCE:
		default:
			dm = "actual_distance";
			break;
	};

	props.Set(Property("scene.textures." + name + ".type")("blender_voronoi"));
	props.Set(Property("scene.textures." + name + ".distancemetric")(dm));
	props.Set(Property("scene.textures." + name + ".intensity")(intensity));
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

//------------------------------------------------------------------------------
// Blender wood texture
//------------------------------------------------------------------------------

BlenderWoodTexture::BlenderWoodTexture(const TextureMapping3D *mp, const std::string &ptype, const std::string &pnoise,
		const std::string &pnoisebasis, const float noises, float turb, bool hard, float bright, float contrast) :
		mapping(mp), type(BANDS), noisebasis(BLENDER_ORIGINAL), noisebasis2(TEX_SIN), noisesize(noises),
		turbulence(turb), hard(hard), bright(bright), contrast(contrast) {

	if(pnoisebasis == "blender_original") {
		noisebasis = BLENDER_ORIGINAL;
	} else if(pnoisebasis == "original_perlin") {
		noisebasis = ORIGINAL_PERLIN;
	} else if(pnoisebasis == "improved_perlin") {
		noisebasis = IMPROVED_PERLIN;
	} else if(pnoisebasis == "voronoi_f1") {
		noisebasis = VORONOI_F1;
	} else if(pnoisebasis == "voronoi_f2") {
		noisebasis = VORONOI_F2;
	} else if(pnoisebasis == "voronoi_f3") {
		noisebasis = VORONOI_F3;
	} else if(pnoisebasis == "voronoi_f4") {
		noisebasis = VORONOI_F4;
	} else if(pnoisebasis == "voronoi_f2_f1") {
		noisebasis = VORONOI_F2_F1;
	} else if(pnoisebasis == "voronoi_crackle") {
		noisebasis = VORONOI_CRACKLE;
	} else if(pnoisebasis == "cell_noise") {
		noisebasis = CELL_NOISE;
	};

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
			if(hard) wood = turbulence * fabs(2.f * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis) - 1.f);
			else wood = turbulence * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);
			wood = waveform[wf]((P.x + P.y + P.z) * 10.f + wood);
			break;
		case RINGNOISE:
			if(hard) wood = turbulence * fabs(2.f * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis) - 1.f);
			else wood = turbulence * BLI_gNoise(noisesize, P.x, P.y, P.z, hard, noisebasis);
			wood = waveform[wf](sqrtf(P.x*P.x + P.y*P.y + P.z*P.z) * 20.f + wood);
			break;
	}

	wood = (wood - 0.5f) * contrast + bright - 0.5f;
	wood = Clamp(wood, 0.f, 1.f);

    return wood;
}

Spectrum BlenderWoodTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

Properties BlenderWoodTexture::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
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

	std::string nbas;
	switch(noisebasis) {
		default:
		case BLENDER_ORIGINAL:
			nbas = "blender_original";
			break;
		case ORIGINAL_PERLIN:
			nbas = "original_perlin";
			break;
		case IMPROVED_PERLIN:
			nbas = "improved_perlin";
			break;
		case VORONOI_F1:
			nbas = "voronoi_f1";
			break;
		case VORONOI_F2:
			nbas = "voronoi_f2";
			break;
		case VORONOI_F3:
			nbas = "voronoi_f3";
			break;
		case VORONOI_F4:
			nbas = "voronoi_f4";
			break;
		case VORONOI_F2_F1:
			nbas = "voronoi_f2_f1";
			break;
		case VORONOI_CRACKLE:
			nbas = "voronoi_crackle";
			break;
		case CELL_NOISE:
			nbas = "cell_noise";
			break;
	}
	std::string noisetype = "soft_noise";
	if(hard) noisetype = "hard_noise";

	const std::string name = GetName();

	props.Set(Property("scene.textures." + name + ".type")("blender_wood"));
	props.Set(Property("scene.textures." + name + ".woodtype")(woodtype));
	props.Set(Property("scene.textures." + name + ".noisebasis")(nbas));
	props.Set(Property("scene.textures." + name + ".noisebasis2")(noise));
	props.Set(Property("scene.textures." + name + ".noisesize")(noisesize));
	props.Set(Property("scene.textures." + name + ".noisetype")(noisetype));
	props.Set(Property("scene.textures." + name + ".turbulence")(turbulence));
	props.Set(Property("scene.textures." + name + ".bright")(bright));
	props.Set(Property("scene.textures." + name + ".contrast")(contrast));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

