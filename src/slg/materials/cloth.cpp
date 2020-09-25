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

#include "luxrays/core/randomgen.h"
#include "slg/materials/cloth.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Cloth material
//------------------------------------------------------------------------------

ClothMaterial::ClothMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const slg::ocl::ClothPreset preset, const Texture *weft_kd, const Texture *weft_ks,
		const Texture *warp_kd, const Texture *warp_ks, const float repeat_u, const float repeat_v) :
		    Material(frontTransp, backTransp, emitted, bump), Preset(preset), Weft_Kd(weft_kd), Weft_Ks(weft_ks),
            Warp_Kd(warp_kd), Warp_Ks(warp_ks), Repeat_U(repeat_u), Repeat_V(repeat_v) {
	SetPreset();
	
	glossiness = 1.f;
}

static const slg::ocl::WeaveConfig ClothWeaves[] = {
    // DenimWeave
    {
        3, 6,
        0.01f, 4.0f,
        0.0f, 0.5f,
        5.0f, 1.0f, 3.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // SilkShantungWeave
    {
        6, 8,
        0.02f, 1.5f,
        0.5f, 0.5f, 
        8.0f, 16.0f, 0.0f,
        20.0f, 20.0f, 10.0f, 10.0f,
        500.0f
    },
    // SilkCharmeuseWeave
    {
        5, 10,
        0.02f, 7.3f,
        0.5f, 0.5f, 
        9.0f, 1.0f, 3.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // CottonTwillWeave
    {
        4, 8,
        0.01f, 4.0f,
        0.0f, 0.5f, 
        6.0f, 2.0f, 4.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // WoolGabardineWeave
    {
        6, 9,
        0.01f, 4.0f,
        0.0f, 0.5f, 
        12.0f, 6.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f
    },
    // PolyesterWeave
    {
        2, 2,
        0.015f, 4.0f,
        0.5f, 0.5f,
        1.0f, 1.0f, 0.0f, 
        8.0f, 8.0f, 6.0f, 6.0f,
        50.0f
    }
};

static const slg::ocl::Yarn ClothYarns[][14] = {
    // DenimYarn[8]
    {
        {-30, 12, 0, 1, 5, 0.1667f, 0.75f, slg::ocl::WARP},
        {-30, 12, 0, 1, 5, 0.1667f, -0.25f, slg::ocl::WARP},
        {-30, 12, 0, 1, 5, 0.5f, 1.0833f, slg::ocl::WARP},
        {-30, 12, 0, 1, 5, 0.5f, 0.0833f, slg::ocl::WARP},
        {-30, 12, 0, 1, 5, 0.8333f, 0.4167f, slg::ocl::WARP},
        {-30, 38, 0, 1, 1, 0.1667f, 0.25f, slg::ocl::WEFT},
        {-30, 38, 0, 1, 1, 0.5f, 0.5833f, slg::ocl::WEFT},
        {-30, 38, 0, 1, 1, 0.8333f, 0.9167f, slg::ocl::WEFT}
    },
    // SilkShantungYarn[5]
    {
        {0, 50, -0.5, 2, 4,  0.3333f, 0.25f, slg::ocl::WARP},
        {0, 50, -0.5, 2, 4,  0.8333f, 0.75f, slg::ocl::WARP},
        {0, 23, -0.3, 4, 4,  0.3333f, 0.75f, slg::ocl::WEFT},
        {0, 23, -0.3, 4, 4, -0.1667f, 0.25f, slg::ocl::WEFT},
        {0, 23, -0.3, 4, 4,  0.8333f, 0.25f, slg::ocl::WEFT}
    },
    // SilkCharmeuseYarn[14]
    {
        {0, 40, 2, 1, 9, 0.1, 0.45, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.3, 1.05, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.3, 0.05, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.5, 0.65, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.5, -0.35, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.7, 1.25, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.7, 0.25, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.9, 0.85, slg::ocl::WARP},
        {0, 40, 2, 1, 9, 0.9, -0.15, slg::ocl::WARP},
        {0, 60, 0, 1, 1, 0.1, 0.95, slg::ocl::WEFT},
        {0, 60, 0, 1, 1, 0.3, 0.55, slg::ocl::WEFT},
        {0, 60, 0, 1, 1, 0.5, 0.15, slg::ocl::WEFT},
        {0, 60, 0, 1, 1, 0.7, 0.75, slg::ocl::WEFT},
        {0, 60, 0, 1, 1, 0.9, 0.35, slg::ocl::WEFT}
    },
    // CottonTwillYarn[10]
    {
        {-30, 24, 0, 1, 6, 0.125,  0.375, slg::ocl::WARP},
        {-30, 24, 0, 1, 6, 0.375,  1.125, slg::ocl::WARP},
        {-30, 24, 0, 1, 6, 0.375,  0.125, slg::ocl::WARP},
        {-30, 24, 0, 1, 6, 0.625,  0.875, slg::ocl::WARP},
        {-30, 24, 0, 1, 6, 0.625, -0.125, slg::ocl::WARP},
        {-30, 24, 0, 1, 6, 0.875,  0.625, slg::ocl::WARP},
        {-30, 36, 0, 2, 1, 0.125,  0.875, slg::ocl::WEFT},
        {-30, 36, 0, 2, 1, 0.375,  0.625, slg::ocl::WEFT},
        {-30, 36, 0, 2, 1, 0.625,  0.375, slg::ocl::WEFT},
        {-30, 36, 0, 2, 1, 0.875,  0.125, slg::ocl::WEFT}
    },
    // WoolGabardineYarn[7]
    {
        {30, 30, 0, 2, 6, 0.167, 0.667, slg::ocl::WARP},
        {30, 30, 0, 2, 6, 0.500, 1.000, slg::ocl::WARP},
        {30, 30, 0, 2, 6, 0.500, 0.000, slg::ocl::WARP},
        {30, 30, 0, 2, 6, 0.833, 0.333, slg::ocl::WARP},
        {30, 30, 0, 3, 2, 0.167, 0.167, slg::ocl::WEFT},
        {30, 30, 0, 3, 2, 0.500, 0.500, slg::ocl::WEFT},
        {30, 30, 0, 3, 2, 0.833, 0.833, slg::ocl::WEFT}
    },
    // PolyesterYarn[4]
    {
        {0, 22, -0.7, 1, 1, 0.25, 0.25, slg::ocl::WARP},
        {0, 22, -0.7, 1, 1, 0.75, 0.75, slg::ocl::WARP},
        {0, 16, -0.7, 1, 1, 0.25, 0.75, slg::ocl::WEFT},
        {0, 16, -0.7, 1, 1, 0.75, 0.25, slg::ocl::WEFT}
    }
};

static const int ClothPatterns[][6 * 9] = {
    // DenimPattern[3 * 6]
    {
        1, 3, 8,  1, 3, 5,  1, 7, 5,  1, 4, 5,  6, 4, 5,  2, 4, 5
    },
    // SilkShantungPattern[6 * 8]
    {
        3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,  3, 3, 3, 3, 2, 2,
        4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5,  4, 1, 1, 5, 5, 5
    },
    // SilkCharmeusePattern[5 * 10]
    {
        10, 2, 4, 6, 8,   1, 2, 4, 6,  8,  1, 2, 4, 13, 8,  1, 2,  4, 7, 8,  1, 11, 4, 7, 8,
        1, 3, 4, 7, 8,   1, 3, 4, 7, 14,  1, 3, 4,  7, 9,  1, 3, 12, 7, 9,  1,  3, 5, 7, 9
    },
    // CottonTwillPattern[4 * 8]
    {
        7, 2, 4, 6,  7, 2, 4, 6,  1, 8, 4,  6,  1, 8, 4,  6,
        1, 3, 9, 6,  1, 3, 9, 6,  1, 3, 5, 10,  1, 3, 5, 10
    },
    // WoolGabardinePattern[6 * 9]
    {
        1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,  1, 1, 2, 2, 7, 7,
        1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,  1, 1, 6, 6, 4, 4,
        5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4,  5, 5, 3, 3, 4, 4
    },
    // PolyesterPattern[2 * 2]
    {
        3, 2, 1, 4
    }
};

static uint64_t sampleTEA(uint32_t v0, uint32_t v1, u_int rounds = 4) {
	uint32_t sum = 0;

	for (u_int i = 0; i < rounds; ++i) {
		sum += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + sum) ^ ((v1 >> 5) + 0xC8013EA4);
		v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7E95761E);
	}

	return ((uint64_t) v1 << 32) + v0;
}

static float sampleTEAfloat(uint32_t v0, uint32_t v1, u_int rounds = 4) {
	/* Trick from MTGP: generate an uniformly distributed
	   single precision number in [1,2) and subtract 1. */
	union {
		uint32_t u;
		float f;
	} x;
	x.u = ((sampleTEA(v0, v1, rounds) & 0xFFFFFFFF) >> 9) | 0x3f800000UL;
	return x.f - 1.0f;
}

// von Mises Distribution
static float vonMises(float cos_x, float b) {
	// assumes a = 0, b > 0 is a concentration parameter.

	const float factor = expf(b * cos_x) * INV_TWOPI;
	const float absB = fabsf(b);
	if (absB <= 3.75f) {
		const float t0 = absB / 3.75f;
		const float t = t0 * t0;
		return factor / (1.0f + t * (3.5156229f + t * (3.0899424f +
			t * (1.2067492f + t * (0.2659732f + t * (0.0360768f +
			t * 0.0045813f))))));
	} else {
		const float t = 3.75f / absB;
		return factor * sqrtf(absB) / (expf(absB) * (0.39894228f +
			t * (0.01328592f + t * (0.00225319f +
			t * (-0.00157565f + t * (0.00916281f +
			t * (-0.02057706f + t * (0.02635537f +
			t * (-0.01647633f + t * 0.00392377f)))))))));
	}
}

// Attenuation term
static float seeliger(float cos_th1, float cos_th2, float sg_a, float sg_s) {
	const float al = sg_s / (sg_a + sg_s); // albedo
	const float c1 = max(0.f, cos_th1);
	const float c2 = max(0.f, cos_th2);
	if (c1 == 0.0f || c2 == 0.0f)
		return 0.0f;
	return al * INV_TWOPI * .5f * c1 * c2 / (c1 + c2);
}

void ClothMaterial::GetYarnUV(const slg::ocl::Yarn *yarn, const Point &center, const Point &xy, UV *uv, float *umaxMod) const {
    const slg::ocl::WeaveConfig *Weave = &ClothWeaves[Preset];

	*umaxMod = luxrays::Radians(yarn->umax);
	if (Weave->period > 0.f) {
		/* Number of TEA iterations (the more, the better the
		   quality of the pseudorandom floats) */
		const int teaIterations = 8;

		// Correlated (Perlin) noise.
		// generate 1 seed per yarn segment
		const float random1 = Noise((center.x *
			(Weave->tileHeight * Repeat_V +
			sampleTEAfloat(center.x, 2.f * center.y,
			teaIterations)) + center.y) / Weave->period, 0.0, 0.0);
		const float random2 = Noise((center.y *
			(Weave->tileWidth * Repeat_U +
			sampleTEAfloat(center.x, 2.f * center.y + 1.f,
			teaIterations)) + center.x) / Weave->period, 0.0, 0.0);
		
		if (yarn->yarn_type == slg::ocl::WARP)
	  		*umaxMod += random1 * luxrays::Radians(Weave->dWarpUmaxOverDWarp) +
				random2 * luxrays::Radians(Weave->dWarpUmaxOverDWeft);
		else
			*umaxMod += random1 * luxrays::Radians(Weave->dWeftUmaxOverDWarp) +
				random2 * luxrays::Radians(Weave->dWeftUmaxOverDWeft);
	}
	

	// Compute u and v.
	// See Chapter 6.
	// Rotate pi/2 radians around z axis
	if (yarn->yarn_type == slg::ocl::WARP) {
		uv->u = xy.y * 2.f * *umaxMod / yarn->length;
		uv->v = xy.x * M_PI / yarn->width;
	} else {
		uv->u = xy.x * 2.f * *umaxMod / yarn->length;
		uv->v = -xy.y * M_PI / yarn->width;
	}
}

const slg::ocl::Yarn *ClothMaterial::GetYarn(const float u_i, const float v_i,
        UV *uv, float *umax, float *scale) const {
    const slg::ocl::WeaveConfig *Weave = &ClothWeaves[Preset];

	const float u = u_i * Repeat_U;
	const int bu = Floor2Int(u);
	const float ou = u - bu;
	const float v = v_i * Repeat_V;
	const int bv = Floor2Int(v);
	const float ov = v - bv;
	const u_int lx = min(Weave->tileWidth - 1, Floor2UInt(ou * Weave->tileWidth));
	const u_int ly = Weave->tileHeight - 1 -
		min(Weave->tileHeight - 1, Floor2UInt(ov * Weave->tileHeight));

	const int yarnID = ClothPatterns[Preset][lx + Weave->tileWidth * ly] - 1;
	const slg::ocl::Yarn *yarn = &ClothYarns[Preset][yarnID];

	const Point center((bu + yarn->centerU) * Weave->tileWidth,
		(bv + yarn->centerV) * Weave->tileHeight);
	const Point xy((ou - yarn->centerU) * Weave->tileWidth,
		(ov - yarn->centerV) * Weave->tileHeight);

	GetYarnUV(yarn, center, xy, uv, umax);

	/* Number of TEA iterations (the more, the better the
	   quality of the pseudorandom floats) */
	const int teaIterations = 8;

	// Compute random variation and scale specular component.
	if (Weave->fineness > 0.0f) {
		// Initialize random number generator based on texture location.
		// Generate fineness^2 seeds per 1 unit of texture.
		const uint32_t index1 = (uint32_t) ((center.x + xy.x) * Weave->fineness);
		const uint32_t index2 = (uint32_t) ((center.y + xy.y) * Weave->fineness);

		const float xi = sampleTEAfloat(index1, index2, teaIterations);
		
		*scale *= min(-logf(xi), 10.0f);
	}

	return yarn;
}

float ClothMaterial::RadiusOfCurvature(const slg::ocl::Yarn *yarn, float u, float umaxMod) const {
	// rhat determines whether the spine is a segment
	// of an ellipse, a parabola, or a hyperbola.
	// See Section 5.3.
	const float rhat = 1.0f + yarn->kappa * (1.0f + 1.0f / tanf(umaxMod));
	const float a = 0.5f * yarn->width;
	
	if (rhat == 1.0f) { // circle; see Subsection 5.3.1.
		return 0.5f * yarn->length / sinf(umaxMod) - a;
	} else if (rhat > 0.0f) { // ellipsis
		const float tmax = atanf(rhat * tanf(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sinf(umaxMod)) / sinf(tmax);
		const float ahat = bhat / rhat;
		const float t = atanf(rhat * tanf(u));
		return powf(bhat * bhat * cosf(t) * cosf(t) +
			ahat * ahat * sinf(t) * sinf(t), 1.5f) / (ahat * bhat);
	} else if (rhat < 0.0f) { // hyperbola; see Subsection 5.3.3.
		const float tmax = -atanhf(rhat * tanf(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sinf(umaxMod)) / sinhf(tmax);
		const float ahat = bhat / rhat;
		const float t = -atanhf(rhat * tanf(u));
		return -powf(bhat * bhat * coshf(t) * coshf(t) +
			ahat * ahat * sinhf(t) * sinhf(t), 1.5f) / (ahat * bhat);
	} else { // rhat == 0  // parabola; see Subsection 5.3.2.
		const float tmax = tanf(umaxMod);
		const float ahat = (0.5f * yarn->length - a * sinf(umaxMod)) / (2.f * tmax);
		const float t = tanf(u);
		return 2.f * ahat * powf(1.f + t * t, 1.5f);
	}
}

float ClothMaterial::EvalFilamentIntegrand(const slg::ocl::Yarn *yarn, const Vector &om_i,
        const Vector &om_r, float u, float v, float umaxMod) const {
    const slg::ocl::WeaveConfig *Weave = &ClothWeaves[Preset];

	// 0 <= ss < 1.0
	if (Weave->ss < 0.0f || Weave->ss >= 1.0f)
		return 0.0f;

	// w * sin(umax) < l
	if (yarn->width * sinf(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const Vector h(Normalize(om_r + om_i));

	// u_of_v is location of specular reflection.
	const float u_of_v = atan2f(h.y, h.z);

	// Check if u_of_v within the range of valid u values
	if (fabsf(u_of_v) >= umaxMod)
		return 0.f;

	// Highlight has constant width delta_u
	const float delta_u = umaxMod * Weave->hWidth;

	// Check if |u(v) - u| < delta_u.
	if (fabsf(u_of_v - u) >= delta_u)
		return 0.f;

	
	// n is normal to the yarn surface
	// t is tangent of the fibers.
	const Normal n(Normalize(Normal(sinf(v), sinf(u_of_v) * cosf(v),
		cosf(u_of_v) * cosf(v))));
	const Vector t(Normalize(Vector(0.0f, cosf(u_of_v), -sinf(u_of_v))));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, min(fabsf(u_of_v),
		(1.f - Weave->ss) * umaxMod), (1.f - Weave->ss) * umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const Vector om_i_plus_om_r(om_i + om_r), t_cross_h(Cross(t, h));
	const float Gu = a * (R + a * cosf(v)) /
		(om_i_plus_om_r.Length() * fabsf(t_cross_h.x));


	// fc is phase function
	const float fc = Weave->alpha + vonMises(-Dot(om_i, om_r), Weave->beta);

	// attenuation function without smoothing.
	float As = seeliger(Dot(n, om_i), Dot(n, om_r), 0, 1);
	// As is attenuation function with smoothing.
	if (Weave->ss > 0.0f)
		As *= SmoothStep(0.f, 1.f, (umaxMod - fabsf(u_of_v)) /
			(Weave->ss * umaxMod));

	// fs is scattering function.
	const float fs = Gu * fc * As;

	// Domain transform.
	return fs * M_PI / Weave->hWidth;
}

float ClothMaterial::EvalStapleIntegrand(const slg::ocl::Yarn *yarn, const Vector &om_i, const Vector &om_r, float u, float v, float umaxMod) const {
    const slg::ocl::WeaveConfig *Weave = &ClothWeaves[Preset];

	// w * sin(umax) < l
	if (yarn->width * sinf(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const Vector h(Normalize(om_i + om_r));

	// v_of_u is location of specular reflection.
	const float D = (h.y * cosf(u) - h.z * sinf(u)) /
		(sqrtf(h.x * h.x + powf(h.y * sinf(u) + h.z * cosf(u),
		2.0f)) * tanf(luxrays::Radians(yarn->psi)));
	if (!(fabsf(D) < 1.f))
		return 0.f;
	const float v_of_u = atan2f(-h.y * sinf(u) - h.z * cosf(u), h.x) +
		acosf(D);

	// Highlight has constant width delta_x on screen.
	const float delta_v = .5f * M_PI * Weave->hWidth;

	// Check if |x(v(u)) - x(v)| < delta_x/2.
	if (fabsf(v_of_u - v) >= delta_v)
		return 0.f;

	// n is normal to the yarn surface.
	const Vector n(Normalize(Vector(sinf(v_of_u), sinf(u) * cosf(v_of_u),
		cosf(u) * cosf(v_of_u))));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fabsf(u), umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const Vector om_i_plus_om_r(om_i + om_r);
	const float Gv = a * (R + a * cosf(v_of_u)) /
		(om_i_plus_om_r.Length() * Dot(n, h) * fabsf(sinf(luxrays::Radians(yarn->psi))));

	// fc is phase function.
	const float fc = Weave->alpha + vonMises(-Dot(om_i, om_r), Weave->beta);

	// A is attenuation function without smoothing.
	const float A = seeliger(Dot(n, om_i), Dot(n, om_r), 0, 1);

	// fs is scattering function.
	const float fs = Gv * fc * A;
	
	// Domain transform.
	return fs * 2.0f * umaxMod / Weave->hWidth;
}

float ClothMaterial::EvalIntegrand(const slg::ocl::Yarn *yarn, const UV &uv, float umaxMod, Vector &om_i, Vector &om_r) const {
    const slg::ocl::WeaveConfig *Weave = &ClothWeaves[Preset];

	if (yarn->yarn_type == slg::ocl::WARP) {
		if (yarn->psi != 0.0f)
			return EvalStapleIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
		else
			return EvalFilamentIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
	} else {
		// Rotate pi/2 radians around z axis
		swap(om_i.x, om_i.y);
		om_i.x = -om_i.x;
		swap(om_r.x, om_r.y);
		om_r.x = -om_r.x;

		if (yarn->psi != 0.0f)
			return EvalStapleIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
		else
			return EvalFilamentIntegrand(yarn, om_i, om_r, uv.u, uv.v,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
	}
}

float ClothMaterial::EvalSpecular(const slg::ocl::Yarn *yarn,const UV &uv, float umax,
        const Vector &wo, const Vector &wi) const {
	// Get incident and exitant directions.
	Vector om_i(wi);
	if (om_i.z < 0.f)
		om_i = -om_i;
	Vector om_r(wo);
	if (om_r.z < 0.f)
		om_r = -om_r;

	// Compute specular contribution.
	return EvalIntegrand(yarn, uv, umax, om_i, om_r);
}

Spectrum ClothMaterial::Albedo(const HitPoint &hitPoint) const {
	UV uv;
	float umax, scale = specularNormalization;
	const UV hitPountUV = hitPoint.GetUV(0);
	const slg::ocl::Yarn *yarn = GetYarn(hitPountUV.u, hitPountUV.v, &uv, &umax, &scale);
	
	const Texture *kd = yarn->yarn_type == slg::ocl::WARP ? Warp_Kd :  Weft_Kd;

	return kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum ClothMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);

	*event = GLOSSY | REFLECT;

	UV uv;
	float umax, scale = specularNormalization;
	const UV hitPountUV = hitPoint.GetUV(0);
	const slg::ocl::Yarn *yarn = GetYarn(hitPountUV.u, hitPountUV.v, &uv, &umax, &scale);
	
	scale = scale * EvalSpecular(yarn, uv, umax, localLightDir, localEyeDir);
	
	const Texture *ks = yarn->yarn_type == slg::ocl::WARP ? Warp_Ks :  Weft_Ks;
	const Texture *kd = yarn->yarn_type == slg::ocl::WARP ? Warp_Kd :  Weft_Kd;

	return (kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) + ks->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * scale) * INV_PI * fabsf(localLightDir.z);
}

Spectrum ClothMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);
	if (fabsf(CosTheta(*localSampledDir)) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = GLOSSY | REFLECT;
	
	UV uv;
	float umax, scale = specularNormalization;
	const UV hitPountUV = hitPoint.GetUV(0);
	const slg::ocl::Yarn *yarn = GetYarn(hitPountUV.u, hitPountUV.v, &uv, &umax, &scale);
	
	if (!hitPoint.fromLight)
	    scale = scale * EvalSpecular(yarn, uv, umax, localFixedDir, *localSampledDir);
	else
	    scale = scale * EvalSpecular(yarn, uv, umax, *localSampledDir, localFixedDir);

	const Texture *ks = (yarn->yarn_type == slg::ocl::WARP ? Warp_Ks :  Weft_Ks);
	const Texture *kd = (yarn->yarn_type == slg::ocl::WARP ? Warp_Kd :  Weft_Kd);
	
	return kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) + ks->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * scale;
}

void ClothMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((hitPoint.fromLight ? localEyeDir.z : localLightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((hitPoint.fromLight ? localLightDir.z : localEyeDir.z) * INV_PI);
}

void ClothMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Warp_Ks->AddReferencedTextures(referencedTexs);
	Weft_Ks->AddReferencedTextures(referencedTexs);
	Weft_Kd->AddReferencedTextures(referencedTexs);
	Warp_Kd->AddReferencedTextures(referencedTexs);
}

void ClothMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Weft_Kd == oldTex)
		Weft_Kd = newTex;
	if (Weft_Ks == oldTex)
		Weft_Ks = newTex;
	if (Warp_Kd == oldTex)
		Warp_Kd = newTex;
	if (Warp_Ks == oldTex)
		Warp_Ks = newTex;
}

Properties ClothMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const std::string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("cloth"));
	
	switch (Preset) {
	  case slg::ocl::DENIM:
		props.Set(Property("scene.materials." + name + ".preset")("denim"));
		break;
	  case slg::ocl::SILKCHARMEUSE:
		props.Set(Property("scene.materials." + name + ".preset")("silk_charmeuse"));
		break;
	  case slg::ocl::SILKSHANTUNG:
		props.Set(Property("scene.materials." + name + ".preset")("silk_shantung"));
		break;
	  case slg::ocl::COTTONTWILL:
		props.Set(Property("scene.materials." + name + ".preset")("cotton_twill"));
		break;
	  case slg::ocl::WOOLGABARDINE:
		props.Set(Property("scene.materials." + name + ".preset")("wool_gabardine"));
		break;
	  case slg::ocl::POLYESTER:
		props.Set(Property("scene.materials." + name + ".preset")("polyester_lining_cloth"));
		break;
	  default:
          throw runtime_error("Unknown preset in ClothMaterial::ToProperties(const ImageMapCache &imgMapCache): " + ToString(Preset));
	    break;
	}

	props.Set(Property("scene.materials." + name + ".weft_kd")(Weft_Kd->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".weft_ks")(Weft_Ks->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".warp_kd")(Warp_Kd->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".warp_ks")(Warp_Ks->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".repeat_u")(Repeat_U));
	props.Set(Property("scene.materials." + name + ".repeat_v")(Repeat_V));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}

void ClothMaterial::SetPreset() {
	// Calibrate scale factor
	
	RandomGenerator random(1);

	const u_int nSamples = 100000;
	
	float result = 0.f;
	for (u_int i = 0; i < nSamples; ++i) {
		const Vector wi = CosineSampleHemisphere(random.floatValue(), random.floatValue());
		const Vector wo = CosineSampleHemisphere(random.floatValue(), random.floatValue());
		
		UV uv;
		float umax, scale = 1.f;
		
		const slg::ocl::Yarn *yarn = GetYarn(random.floatValue(), random.floatValue(), &uv, &umax, &scale);
		
		result += EvalSpecular(yarn, uv, umax, wo, wi) * scale;
	}

	if (result > 0.f)
		specularNormalization = nSamples / result;
	else
		specularNormalization = 0;
//	printf("********************** specularNormalization = %f\n", specularNormalization);
}
