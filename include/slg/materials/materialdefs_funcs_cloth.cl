#line 2 "materialdefs_funcs_cloth.cl"

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
// Cloth material
//------------------------------------------------------------------------------

#if defined (PARAM_ENABLE_MAT_CLOTH)

BSDFEvent ClothMaterial_GetEventTypes() {
	return GLOSSY | REFLECT;
}

bool ClothMaterial_IsDelta() {
	return false;
}

__constant WeaveConfig ClothWeaves[] = {
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
    // WoolGarbardineWeave
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

__constant Yarn ClothYarns[][14] = {
    // DenimYarn[8]
    {
        {-30, 12, 0, 1, 5, 0.1667f, 0.75f, WARP},
        {-30, 12, 0, 1, 5, 0.1667f, -0.25f, WARP},
        {-30, 12, 0, 1, 5, 0.5f, 1.0833f, WARP},
        {-30, 12, 0, 1, 5, 0.5f, 0.0833f, WARP},
        {-30, 12, 0, 1, 5, 0.8333f, 0.4167f, WARP},
        {-30, 38, 0, 1, 1, 0.1667f, 0.25f, WEFT},
        {-30, 38, 0, 1, 1, 0.5f, 0.5833f, WEFT},
        {-30, 38, 0, 1, 1, 0.8333f, 0.9167f, WEFT}
    },
    // SilkShantungYarn[5]
    {
        {0, 50, -0.5, 2, 4,  0.3333f, 0.25f, WARP},
        {0, 50, -0.5, 2, 4,  0.8333f, 0.75f, WARP},
        {0, 23, -0.3, 4, 4,  0.3333f, 0.75f, WEFT},
        {0, 23, -0.3, 4, 4, -0.1667f, 0.25f, WEFT},
        {0, 23, -0.3, 4, 4,  0.8333f, 0.25f, WEFT}
    },
    // SilkCharmeuseYarn[14]
    {
        {0, 40, 2, 1, 9, 0.1, 0.45, WARP},
        {0, 40, 2, 1, 9, 0.3, 1.05, WARP},
        {0, 40, 2, 1, 9, 0.3, 0.05, WARP},
        {0, 40, 2, 1, 9, 0.5, 0.65, WARP},
        {0, 40, 2, 1, 9, 0.5, -0.35, WARP},
        {0, 40, 2, 1, 9, 0.7, 1.25, WARP},
        {0, 40, 2, 1, 9, 0.7, 0.25, WARP},
        {0, 40, 2, 1, 9, 0.9, 0.85, WARP},
        {0, 40, 2, 1, 9, 0.9, -0.15, WARP},
        {0, 60, 0, 1, 1, 0.1, 0.95, WEFT},
        {0, 60, 0, 1, 1, 0.3, 0.55, WEFT},
        {0, 60, 0, 1, 1, 0.5, 0.15, WEFT},
        {0, 60, 0, 1, 1, 0.7, 0.75, WEFT},
        {0, 60, 0, 1, 1, 0.9, 0.35, WEFT}
    },
    // CottonTwillYarn[10]
    {
        {-30, 24, 0, 1, 6, 0.125,  0.375, WARP},
        {-30, 24, 0, 1, 6, 0.375,  1.125, WARP},
        {-30, 24, 0, 1, 6, 0.375,  0.125, WARP},
        {-30, 24, 0, 1, 6, 0.625,  0.875, WARP},
        {-30, 24, 0, 1, 6, 0.625, -0.125, WARP},
        {-30, 24, 0, 1, 6, 0.875,  0.625, WARP},
        {-30, 36, 0, 2, 1, 0.125,  0.875, WEFT},
        {-30, 36, 0, 2, 1, 0.375,  0.625, WEFT},
        {-30, 36, 0, 2, 1, 0.625,  0.375, WEFT},
        {-30, 36, 0, 2, 1, 0.875,  0.125, WEFT}
    },
    // WoolGarbardineYarn[7]
    {
        {30, 30, 0, 2, 6, 0.167, 0.667, WARP},
        {30, 30, 0, 2, 6, 0.500, 1.000, WARP},
        {30, 30, 0, 2, 6, 0.500, 0.000, WARP},
        {30, 30, 0, 2, 6, 0.833, 0.333, WARP},
        {30, 30, 0, 3, 2, 0.167, 0.167, WEFT},
        {30, 30, 0, 3, 2, 0.500, 0.500, WEFT},
        {30, 30, 0, 3, 2, 0.833, 0.833, WEFT}
    },
    // PolyesterYarn[4]
    {
        {0, 22, -0.7, 1, 1, 0.25, 0.25, WARP},
        {0, 22, -0.7, 1, 1, 0.75, 0.75, WARP},
        {0, 16, -0.7, 1, 1, 0.25, 0.75, WEFT},
        {0, 16, -0.7, 1, 1, 0.75, 0.25, WEFT}
    }
};

__constant int ClothPatterns[][6 * 9] = {
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
    // WoolGarbardinePattern[6 * 9]
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

ulong sampleTEA(uint v0, uint v1, uint rounds) {
	uint sum = 0;

	for (uint i = 0; i < rounds; ++i) {
		sum += 0x9e3779b9;
		v0 += ((v1 << 4) + 0xA341316C) ^ (v1 + sum) ^ ((v1 >> 5) + 0xC8013EA4);
		v1 += ((v0 << 4) + 0xAD90777D) ^ (v0 + sum) ^ ((v0 >> 5) + 0x7E95761E);
	}

	return ((ulong) v1 << 32) + v0;
}

float sampleTEAfloat(uint v0, uint v1, uint rounds) {
	/* Trick from MTGP: generate an uniformly distributed
	   single precision number in [1,2) and subtract 1. */
	union {
		uint u;
		float f;
	} x;
	x.u = ((sampleTEA(v0, v1, rounds) & 0xFFFFFFFF) >> 9) | 0x3f800000UL;
	return x.f - 1.0f;
}

// von Mises Distribution
float vonMises(float cos_x, float b) {
	// assumes a = 0, b > 0 is a concentration parameter.

	const float factor = exp(b * cos_x) * (.5f * M_1_PI_F);
	const float absB = fabs(b);
	if (absB <= 3.75f) {
		const float t0 = absB / 3.75f;
		const float t = t0 * t0;
		return factor / (1.0f + t * (3.5156229f + t * (3.0899424f +
			t * (1.2067492f + t * (0.2659732f + t * (0.0360768f +
			t * 0.0045813f))))));
	} else {
		const float t = 3.75f / absB;
		return factor * sqrt(absB) / (exp(absB) * (0.39894228f +
			t * (0.01328592f + t * (0.00225319f +
			t * (-0.00157565f + t * (0.00916281f +
			t * (-0.02057706f + t * (0.02635537f +
			t * (-0.01647633f + t * 0.00392377f)))))))));
	}
}

// Attenuation term
float seeliger(float cos_th1, float cos_th2, float sg_a, float sg_s) {
	const float al = sg_s / (sg_a + sg_s); // albedo
	const float c1 = fmax(0.f, cos_th1);
	const float c2 = fmax(0.f, cos_th2);
	if (c1 == 0.0f || c2 == 0.0f)
		return 0.0f;
	return al * (.5f * M_1_PI_F) * .5f * c1 * c2 / (c1 + c2);
}

void GetYarnUV(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float Repeat_U, const float Repeat_V,
        const float3 center, const float3 xy, float2 *uv, float *umaxMod) {
	*umaxMod = Radians(yarn->umax);
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
		
		if (yarn->yarn_type == WARP)
	  		*umaxMod += random1 * Radians(Weave->dWarpUmaxOverDWarp) +
				random2 * Radians(Weave->dWarpUmaxOverDWeft);
		else
			*umaxMod += random1 * Radians(Weave->dWeftUmaxOverDWarp) +
				random2 * Radians(Weave->dWeftUmaxOverDWeft);
	}
	

	// Compute u and v.
	// See Chapter 6.
	// Rotate pi/2 radians around z axis
	if (yarn->yarn_type == WARP) {
		(*uv).s0 = xy.y * 2.f * *umaxMod / yarn->length;
		(*uv).s1 = xy.x * M_PI_F / yarn->width;
	} else {
		(*uv).s0 = xy.x * 2.f * *umaxMod / yarn->length;
		(*uv).s1 = -xy.y * M_PI_F / yarn->width;
	}
}

__constant Yarn *GetYarn(const ClothPreset Preset, __constant WeaveConfig *Weave,
        const float Repeat_U, const float Repeat_V,
        const float u_i, const float v_i,
        float2 *uv, float *umax, float *scale) {
	const float u = u_i * Repeat_U;
	const int bu = Floor2Int(u);
	const float ou = u - bu;
	const float v = v_i * Repeat_V;
	const int bv = Floor2Int(v);
	const float ov = v - bv;
	const uint lx = min(Weave->tileWidth - 1, Floor2UInt(ou * Weave->tileWidth));
	const uint ly = Weave->tileHeight - 1 -
		min(Weave->tileHeight - 1, Floor2UInt(ov * Weave->tileHeight));

	const int yarnID = ClothPatterns[Preset][lx + Weave->tileWidth * ly] - 1;
	__constant Yarn *yarn = &ClothYarns[Preset][yarnID];

	const float3 center = (float3)((bu + yarn->centerU) * Weave->tileWidth,
		(bv + yarn->centerV) * Weave->tileHeight, 0.f);
	const float3 xy = (float3)((ou - yarn->centerU) * Weave->tileWidth,
		(ov - yarn->centerV) * Weave->tileHeight, 0.f);

	GetYarnUV(Weave, yarn, Repeat_U, Repeat_V, center, xy, uv, umax);

	/* Number of TEA iterations (the more, the better the
	   quality of the pseudorandom floats) */
	const int teaIterations = 8;

	// Compute random variation and scale specular component.
	if (Weave->fineness > 0.0f) {
		// Initialize random number generator based on texture location.
		// Generate fineness^2 seeds per 1 unit of texture.
		const uint index1 = (uint) ((center.x + xy.x) * Weave->fineness);
		const uint index2 = (uint) ((center.y + xy.y) * Weave->fineness);

		const float xi = sampleTEAfloat(index1, index2, teaIterations);
		
		*scale *= fmin(-log(xi), 10.0f);
	}

	return yarn;
}

float RadiusOfCurvature(__constant Yarn *yarn, float u, float umaxMod) {
	// rhat determines whether the spine is a segment
	// of an ellipse, a parabole, or a hyperbola.
	// See Section 5.3.
	const float rhat = 1.0f + yarn->kappa * (1.0f + 1.0f / tan(umaxMod));
	const float a = 0.5f * yarn->width;
	
	if (rhat == 1.0f) { // circle; see Subsection 5.3.1.
		return 0.5f * yarn->length / sin(umaxMod) - a;
	} else if (rhat > 0.0f) { // ellipsis
		const float tmax = atan(rhat * tan(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sin(umaxMod)) / sin(tmax);
		const float ahat = bhat / rhat;
		const float t = atan(rhat * tan(u));
		return pow(bhat * bhat * cos(t) * cos(t) +
			ahat * ahat * sin(t) * sin(t), 1.5f) / (ahat * bhat);
	} else if (rhat < 0.0f) { // hyperbola; see Subsection 5.3.3.
		const float tmax = -atanh(rhat * tan(umaxMod));
		const float bhat = (0.5f * yarn->length - a * sin(umaxMod)) / sinh(tmax);
		const float ahat = bhat / rhat;
		const float t = -atanh(rhat * tan(u));
		return -pow(bhat * bhat * cosh(t) * cosh(t) +
			ahat * ahat * sinh(t) * sinh(t), 1.5f) / (ahat * bhat);
	} else { // rhat == 0  // parabola; see Subsection 5.3.2.
		const float tmax = tan(umaxMod);
		const float ahat = (0.5f * yarn->length - a * sin(umaxMod)) / (2.f * tmax);
		const float t = tan(u);
		return 2.f * ahat * pow(1.f + t * t, 1.5f);
	}
}

float EvalFilamentIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn, const float3 om_i,
        const float3 om_r, float u, float v, float umaxMod) {
	// 0 <= ss < 1.0
	if (Weave->ss < 0.0f || Weave->ss >= 1.0f)
		return 0.0f;

	// w * sin(umax) < l
	if (yarn->width * sin(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const float3 h = normalize(om_r + om_i);

	// u_of_v is location of specular reflection.
	const float u_of_v = atan2(h.y, h.z);

	// Check if u_of_v within the range of valid u values
	if (fabs(u_of_v) >= umaxMod)
		return 0.f;

	// Highlight has constant width delta_u
	const float delta_u = umaxMod * Weave->hWidth;

	// Check if |u(v) - u| < delta_u.
	if (fabs(u_of_v - u) >= delta_u)
		return 0.f;

	
	// n is normal to the yarn surface
	// t is tangent of the fibers.
	const float3 n = normalize((float3)(sin(v), sin(u_of_v) * cos(v),
		cos(u_of_v) * cos(v)));
	const float3 t = normalize((float3)(0.0f, cos(u_of_v), -sin(u_of_v)));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fmin(fabs(u_of_v),
		(1.f - Weave->ss) * umaxMod), (1.f - Weave->ss) * umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const float3 om_i_plus_om_r = om_i + om_r;
    const float3 t_cross_h = cross(t, h);
	const float Gu = a * (R + a * cos(v)) /
		(length(om_i_plus_om_r) * fabs(t_cross_h.x));


	// fc is phase function
	const float fc = Weave->alpha + vonMises(-dot(om_i, om_r), Weave->beta);

	// attenuation function without smoothing.
	float As = seeliger(dot(n, om_i), dot(n, om_r), 0, 1);
	// As is attenuation function with smoothing.
	if (Weave->ss > 0.0f)
		As *= SmoothStep(0.f, 1.f, (umaxMod - fabs(u_of_v)) /
			(Weave->ss * umaxMod));

	// fs is scattering function.
	const float fs = Gu * fc * As;

	// Domain transform.
	return fs * M_PI_F / Weave->hWidth;
}

float EvalStapleIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float3 om_i, const float3 om_r, float u, float v, float umaxMod) {
	// w * sin(umax) < l
	if (yarn->width * sin(umaxMod) >= yarn->length)
		return 0.0f;

	// -1 < kappa < inf
	if (yarn->kappa < -1.0f)
		return 0.0f;

	// h is the half vector
	const float3 h = normalize(om_i + om_r);

	// v_of_u is location of specular reflection.
	const float D = (h.y * cos(u) - h.z * sin(u)) /
		(sqrt(h.x * h.x + pow(h.y * sin(u) + h.z * cos(u),
		2.0f)) * tan(Radians(yarn->psi)));
	if (!(fabs(D) < 1.f))
		return 0.f;
	const float v_of_u = atan2(-h.y * sin(u) - h.z * cos(u), h.x) +
		acos(D);

	// Highlight has constant width delta_x on screen.
	const float delta_v = .5f * M_PI_F * Weave->hWidth;

	// Check if |x(v(u)) - x(v)| < delta_x/2.
	if (fabs(v_of_u - v) >= delta_v)
		return 0.f;

	// n is normal to the yarn surface.
	const float3 n = normalize((float3)(sin(v_of_u), sin(u) * cos(v_of_u),
		cos(u) * cos(v_of_u)));

	// R is radius of curvature.
	const float R = RadiusOfCurvature(yarn, fabs(u), umaxMod);

	// G is geometry factor.
	const float a = 0.5f * yarn->width;
	const float3 om_i_plus_om_r = om_i + om_r;
	const float Gv = a * (R + a * cos(v_of_u)) /
		(length(om_i_plus_om_r) * dot(n, h) * fabs(sin(Radians(yarn->psi))));

	// fc is phase function.
	const float fc = Weave->alpha + vonMises(-dot(om_i, om_r), Weave->beta);

	// A is attenuation function without smoothing.
	const float A = seeliger(dot(n, om_i), dot(n, om_r), 0, 1);

	// fs is scattering function.
	const float fs = Gv * fc * A;
	
	// Domain transform.
	return fs * 2.0f * umaxMod / Weave->hWidth;
}

float EvalIntegrand(__constant WeaveConfig *Weave, __constant Yarn *yarn,
        const float2 uv, float umaxMod, float3 *om_i, float3 *om_r) {
	if (yarn->yarn_type == WARP) {
		if (yarn->psi != 0.0f)
			return EvalStapleIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
		else
			return EvalFilamentIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->warpArea;
	} else {
		// Rotate pi/2 radians around z axis
        //swap((*om_i).x, (*om_i).y);
        float tmp = (*om_i).x;
        (*om_i).x = (*om_i).y;
        (*om_i).y = tmp;
		(*om_i).x = -(*om_i).x;

		//swap((*om_r).x, (*om_r).y);
        tmp = (*om_r).x;
        (*om_r).x = (*om_r).y;
        (*om_r).y = tmp;
		(*om_r).x = -(*om_r).x;

		if (yarn->psi != 0.0f)
			return EvalStapleIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
		else
			return EvalFilamentIntegrand(Weave, yarn, *om_i, *om_r, uv.s0, uv.s1,
				umaxMod) * (Weave->warpArea + Weave->weftArea) /
				Weave->weftArea;
	}
}

float EvalSpecular(__constant WeaveConfig *Weave, __constant Yarn *yarn, const float2 uv,
        float umax, const float3 wo, const float3 wi) {
	// Get incident and exitant directions.
	float3 om_i = wi;
	if (om_i.z < 0.f)
		om_i = -om_i;
	float3 om_r = wo;
	if (om_r.z < 0.f)
		om_r = -om_r;

	// Compute specular contribution.
	return EvalIntegrand(Weave, yarn, uv, umax, &om_i, &om_r);
}

float3 ClothMaterial_Evaluate(
		__global HitPoint *hitPoint, const float3 localLightDir, const float3 localEyeDir,
		BSDFEvent *event, float *directPdfW,
		const ClothPreset Preset, const float Repeat_U, const float Repeat_V,
		const float s, const float3 Warp_Ks, const float3 Weft_Ks,
		const float3 Warp_Kd, const float3 Weft_Kd) {
    *directPdfW = fabs(localLightDir.z * M_1_PI_F);
    
    *event = GLOSSY | REFLECT;

    __constant WeaveConfig *Weave = &ClothWeaves[Preset];

	float2 uv;
	float umax, scale = s;
	__constant Yarn *yarn = GetYarn(Preset, Weave, Repeat_U, Repeat_V,
            hitPoint->uv.u, hitPoint->uv.v, &uv, &umax, &scale);
    
    scale = scale * EvalSpecular(Weave, yarn, uv, umax, localLightDir, localEyeDir);
	
	const float3 ks = (yarn->yarn_type == WARP) ? Warp_Ks : Weft_Ks;
	const float3 kd = (yarn->yarn_type == WARP) ? Warp_Kd : Weft_Kd;

    const float3 ksVal = Spectrum_Clamp(ks);
    const float3 kdVal = Spectrum_Clamp(kd);

	return (kdVal + ksVal * scale) * M_1_PI_F * fabs(localLightDir.z);
}

float3 ClothMaterial_Sample(
		__global HitPoint *hitPoint, const float3 localFixedDir, float3 *localSampledDir,
		const float u0, const float u1,
#if defined(PARAM_HAS_PASSTHROUGH)
		const float passThroughEvent,
#endif
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent,
		const ClothPreset Preset, const float Repeat_U, const float Repeat_V,
		const float s, const float3 Warp_Ks, const float3 Weft_Ks,
		const float3 Warp_Kd, const float3 Weft_Kd) {
	if (!(requestedEvent & (GLOSSY | REFLECT)) ||
			(fabs(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC))
		return BLACK;

	*localSampledDir = (signbit(localFixedDir.z) ? -1.f : 1.f) * CosineSampleHemisphereWithPdf(u0, u1, pdfW);

	*absCosSampledDir = fabs((*localSampledDir).z);
	if (*absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return BLACK;

	*event = GLOSSY | REFLECT;

    __constant WeaveConfig *Weave = &ClothWeaves[Preset];

	float2 uv;
	float umax, scale = s;
	__constant Yarn *yarn = GetYarn(Preset, Weave, Repeat_U, Repeat_V,
            hitPoint->uv.u, hitPoint->uv.v, &uv, &umax, &scale);

//	if (!hitPoint.fromLight)
	    scale = scale * EvalSpecular(Weave, yarn, uv, umax, localFixedDir, *localSampledDir);
//	else
//	    scale = scale * EvalSpecular(Weave, yarn, uv, umax, *localSampledDir, localFixedDir);

    const float3 ks = (yarn->yarn_type == WARP) ? Warp_Ks : Weft_Ks;
	const float3 kd = (yarn->yarn_type == WARP) ? Warp_Kd : Weft_Kd;

    const float3 ksVal = Spectrum_Clamp(ks);
    const float3 kdVal = Spectrum_Clamp(kd);

	return kdVal + ksVal * scale;
}

#endif
