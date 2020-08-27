#line 2 "texture_brick_funcs.cl"

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

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float BrickTexture_BrickNoise(uint n) {
	n = (n + 1013) & 0x7fffffff;
	n = (n >> 13) ^ n;
	const uint nn = (n * (n * n * 60493 + 19990303) + 1376312589) & 0x7fffffff;
	return 0.5f * ((float)nn / 1073741824.0f);
}

OPENCL_FORCE_INLINE bool BrickTexture_RunningAlternate(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		int nWhole) {
	const float sub = nWhole + 0.5f;
	const float rsub = ceil(sub);
	(*i).z = floor(p.z);
	(*b).x = (p.x + (*i).z * run) / sub;
	(*b).y = (p.y + (*i).z * run) / sub;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).x = ((*b).x - (*i).x) * sub;
	(*b).y = ((*b).y - (*i).y) * sub;
	(*b).z = (p.z - (*i).z) * sub;
	(*i).x += floor((*b).x) / rsub;
	(*i).y += floor((*b).y) / rsub;
	(*b).x -= floor((*b).x);
	(*b).y -= floor((*b).y);
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

OPENCL_FORCE_INLINE bool BrickTexture_Basket(const float3 p, float3 *i,
		const float mortarwidth, const float mortardepth,
		const float proportion, const float invproportion) {
	(*i).x = floor(p.x);
	(*i).y = floor(p.y);
	float bx = p.x - (*i).x;
	float by = p.y - (*i).y;
	(*i).x += (*i).y - 2.f * floor(0.5f * (*i).y);
	const bool split = ((*i).x - 2.f * floor(0.5f * (*i).x)) < 1.f;
	if (split) {
		bx = fmod(bx, invproportion);
		(*i).x = floor(proportion * p.x) * invproportion;
	} else {
		by = fmod(by, invproportion);
		(*i).y = floor(proportion * p.y) * invproportion;
	}
	return by > mortardepth && bx > mortarwidth;
}

OPENCL_FORCE_INLINE bool BrickTexture_Herringbone(const float3 p, float3 *i,
		const float mortarwidth, const float mortarheight,
		const float proportion, const float invproportion) {
	(*i).y = floor(proportion * p.y);
	const float px = p.x + (*i).y * invproportion;
	(*i).x = floor(px);
	float bx = 0.5f * px - floor(px * 0.5f);
	bx *= 2.f;
	float by = proportion * p.y - floor(proportion * p.y);
	by *= invproportion;
	if (bx > 1.f + invproportion) {
		bx = proportion * (bx - 1.f);
		(*i).y -= floor(bx - 1.f);
		bx -= floor(bx);
		bx *= invproportion;
		by = 1.f;
	} else if (bx > 1.f) {
		bx = proportion * (bx - 1.f);
		(*i).y -= floor(bx - 1.f);
		bx -= floor(bx);
		bx *= invproportion;
	}
	return by > mortarheight && bx > mortarwidth;
}

OPENCL_FORCE_INLINE bool BrickTexture_Running(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth) {
	(*i).z = floor(p.z);
	(*b).x = p.x + (*i).z * run;
	(*b).y = p.y - (*i).z * run;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).z = p.z - (*i).z;
	(*b).x -= (*i).x;
	(*b).y -= (*i).y;
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

OPENCL_FORCE_INLINE bool BrickTexture_English(const float3 p, float3 *i, float3 *b,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth) {
	(*i).z = floor(p.z);
	(*b).x = p.x + (*i).z * run;
	(*b).y = p.y - (*i).z * run;
	(*i).x = floor((*b).x);
	(*i).y = floor((*b).y);
	(*b).z = p.z - (*i).z;
	const float divider = floor(fmod(fabs((*i).z), 2.f)) + 1.f;
	(*b).x = (divider * (*b).x - floor(divider * (*b).x)) / divider;
	(*b).y = (divider * (*b).y - floor(divider * (*b).y)) / divider;
	return (*b).z > mortarheight && (*b).y > mortardepth &&
		(*b).x > mortarwidth;
}

OPENCL_FORCE_NOT_INLINE bool BrickTexture_Evaluate(__global const HitPoint *hitPoint,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		float3 *brickIndex,
		__global const TextureMapping3D *mapping
		TEXTURES_PARAM_DECL) {
#define BRICK_EPSILON 1e-3f
	const float3 P = TextureMapping3D_Map(mapping, hitPoint, NULL TEXTURES_PARAM);

	const float offs = BRICK_EPSILON + mortarsize;
	float3 bP = P + MAKE_FLOAT3(offs, offs, offs);

	// Normalize coordinates according brick dimensions
	bP.x /= brickwidth;
	bP.y /= brickdepth;
	bP.z /= brickheight;

	bP += offset;

	float3 bevel;
	bool b;
	switch (bond) {
		case FLEMISH:
			b = BrickTexture_RunningAlternate(bP, brickIndex, &bevel,
					run , mortarwidth, mortarheight, mortardepth, 1);
			break;
		case RUNNING:
			b = BrickTexture_Running(bP, brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth);
			break;
		case ENGLISH:
			b = BrickTexture_English(bP, brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth);
			break;
		case HERRINGBONE:
			b = BrickTexture_Herringbone(bP, brickIndex,
					mortarwidth, mortarheight, proportion, invproportion);
			break;
		case BASKET:
			b = BrickTexture_Basket(bP, brickIndex,
					mortarwidth, mortardepth, proportion, invproportion);
			break;
		case KETTING:
			b = BrickTexture_RunningAlternate(bP, brickIndex, &bevel,
					run, mortarwidth, mortarheight, mortardepth, 2);
			break;
		default:
			b = true;
			break;
	}

	return b;
#undef BRICK_EPSILON
}

OPENCL_FORCE_NOT_INLINE float BrickTexture_ConstEvaluateFloat(__global const HitPoint *hitPoint,
		const float value1, const float value2, const float value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		const float modulationBias,
		__global const TextureMapping3D *mapping TEXTURES_PARAM_DECL) {
	float3 brickIndex;
	const bool b = BrickTexture_Evaluate(hitPoint,
			bond,
			brickwidth, brickheight,
			brickdepth, mortarsize,
			offset,
			run, mortarwidth,
			mortarheight, mortardepth,
			proportion, invproportion,
			&brickIndex,
			mapping
			TEXTURES_PARAM);
	
	if (b) {
		// Return brick color
		if (modulationBias == -1.f) {
			return value1;
		} else if (modulationBias == 1.f) {
			return value3;
		}
		
		const int rownum = brickIndex.y;
		const int bricknum = brickIndex.x;
		const float noise = BrickTexture_BrickNoise((rownum << 16) + (bricknum & 0xffff));
		const float modulation = clamp(noise + modulationBias, 0.f, 1.f);
		
		return Lerp(modulation, value1, value3);
	} else {
		// Return mortar color
		return value2;
	}
}

OPENCL_FORCE_NOT_INLINE float3 BrickTexture_ConstEvaluateSpectrum(__global const HitPoint *hitPoint,
		const float3 value1, const float3 value2, const float3 value3,
		const MasonryBond bond,
		const float brickwidth, const float brickheight,
		const float brickdepth, const float mortarsize,
		const float3 offset,
		const float run, const float mortarwidth,
		const float mortarheight, const float mortardepth,
		const float proportion, const float invproportion,
		const float modulationBias,
		__global const TextureMapping3D *mapping
		TEXTURES_PARAM_DECL) {
	float3 brickIndex;
	const bool b = BrickTexture_Evaluate(hitPoint,
			bond,
			brickwidth, brickheight,
			brickdepth, mortarsize,
			offset,
			run, mortarwidth,
			mortarheight, mortardepth,
			proportion, invproportion,
			&brickIndex,
			mapping
			TEXTURES_PARAM);
	
	if (b) {
		// Return brick color
		if (modulationBias == -1.f) {
			return value1;
		} else if (modulationBias == 1.f) {
			return value3;
		}
		
		const int rownum = brickIndex.y;
		const int bricknum = brickIndex.x;
		const float noise = BrickTexture_BrickNoise((rownum << 16) + (bricknum & 0xffff));
		const float modulation = clamp(noise + modulationBias, 0.f, 1.f);
		
		return Lerp3(modulation, value1, value3);
	} else {
		// Return mortar color
		return value2;
	}
}

OPENCL_FORCE_NOT_INLINE void BrickTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			float tex1, tex2, tex3;
			EvalStack_PopFloat(tex3);
			EvalStack_PopFloat(tex2);
			EvalStack_PopFloat(tex1);

			const float eval = BrickTexture_ConstEvaluateFloat(hitPoint,
					tex1, tex2, tex3,
					texture->brick.bond,
					texture->brick.brickwidth, texture->brick.brickheight,
					texture->brick.brickdepth, texture->brick.mortarsize,
					MAKE_FLOAT3(texture->brick.offsetx, texture->brick.offsety, texture->brick.offsetz),
					texture->brick.run, texture->brick.mortarwidth,
					texture->brick.mortarheight, texture->brick.mortardepth,
					texture->brick.proportion, texture->brick.invproportion,
					texture->brick.modulationBias,
					&texture->brick.mapping
					TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			float3 tex1, tex2, tex3;
			EvalStack_PopFloat3(tex3);
			EvalStack_PopFloat3(tex2);
			EvalStack_PopFloat3(tex1);

			const float3 eval = BrickTexture_ConstEvaluateSpectrum(hitPoint,
					tex1, tex2, tex3,
					texture->brick.bond,
					texture->brick.brickwidth, texture->brick.brickheight,
					texture->brick.brickdepth, texture->brick.mortarsize,
					MAKE_FLOAT3(texture->brick.offsetx, texture->brick.offsety, texture->brick.offsetz),
					texture->brick.run, texture->brick.mortarwidth,
					texture->brick.mortarheight, texture->brick.mortardepth,
					texture->brick.proportion, texture->brick.invproportion,
					texture->brick.modulationBias,
					&texture->brick.mapping
					TEXTURES_PARAM);
			EvalStack_PushFloat3(eval);
			break;
		}
		case EVAL_BUMP_GENERIC_OFFSET_U:
			Texture_EvalOpGenericBumpOffsetU(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		case EVAL_BUMP_GENERIC_OFFSET_V:
			Texture_EvalOpGenericBumpOffsetV(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		case EVAL_BUMP:
			Texture_EvalOpGenericBump(evalStack, evalStackOffset,
					hitPoint, sampleDistance);
			break;
		default:
			// Something wrong here
			break;
	}
}
