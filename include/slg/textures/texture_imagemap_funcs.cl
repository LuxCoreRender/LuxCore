#line 2 "texture_imagemap_funcs.cl"

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
// ImageMap texture
//------------------------------------------------------------------------------

OPENCL_FORCE_INLINE float3 RGBToYCbCr(const float3 rgb) {
	// ITU-R BT.601 from https://en.wikipedia.org/wiki/YCbCr
	const float RGBToYCbCrOff[3] = {
		 16.f / 255.f,
		128.f / 255.f,
		128.f / 255.f
	};
    const float RGBToYCbCrMat[3][3] = {
        {  65.481f / 255.f, 128.553f / 255.f,   24.966f / 255.f },
        { -37.797f / 255.f, -74.203f / 255.f,  112.f    / 255.f },
        { 112.f    / 255.f, -93.786f / 255.f, -18.214f  / 255.f }
	};

	// RGB to YCbCr color space transformation
	const float Y  = RGBToYCbCrOff[0] + RGBToYCbCrMat[0][0] * rgb.x + RGBToYCbCrMat[0][1] * rgb.y + RGBToYCbCrMat[0][2] * rgb.z;
	const float Cb = RGBToYCbCrOff[1] + RGBToYCbCrMat[1][0] * rgb.x + RGBToYCbCrMat[1][1] * rgb.y + RGBToYCbCrMat[1][2] * rgb.z;
	const float Cr = RGBToYCbCrOff[2] + RGBToYCbCrMat[2][0] * rgb.x + RGBToYCbCrMat[2][1] * rgb.y + RGBToYCbCrMat[2][2] * rgb.z;

	return clamp(MAKE_FLOAT3(Y, Cb, Cr), 0.f, 1.f);
}

OPENCL_FORCE_INLINE float3 YCbCrToRGB(const float3 YCbCr) {
	// ITU-R BT.601 from https://en.wikipedia.org/wiki/YCbCr
	const float YCbCrToRGBOff[3] = {
		-222.921f / 256.f,
		 135.576f / 256.f,
		-276.836f / 256.f
	};
    const float YCbCrToRGBMat[3][3] = {
		{ 298.082f / 256.f,    0.f    / 256.f,  408.583f / 256.f },
		{ 298.082f / 256.f, -100.291f / 256.f, -208.120f / 256.f },
		{ 298.082f / 256.f,  516.412f / 256.f,    0.f    / 256.f }
	};

	// YCbCr to RGB color space transformation
	const float r = YCbCrToRGBOff[0] + YCbCrToRGBMat[0][0] * YCbCr.x + YCbCrToRGBMat[0][1] * YCbCr.y + YCbCrToRGBMat[0][2] * YCbCr.z;
	const float g = YCbCrToRGBOff[1] + YCbCrToRGBMat[1][0] * YCbCr.x + YCbCrToRGBMat[1][1] * YCbCr.y + YCbCrToRGBMat[1][2] * YCbCr.z;
	const float b = YCbCrToRGBOff[2] + YCbCrToRGBMat[2][0] * YCbCr.x + YCbCrToRGBMat[2][1] * YCbCr.y + YCbCrToRGBMat[2][2] * YCbCr.z;

	return clamp(MAKE_FLOAT3(r, g, b), 0.f, 1.f);
}

OPENCL_FORCE_INLINE float3 ImageMapTexture_SampleTile(__global const Texture* restrict tex,
		const float2 vertex, const float2 offset
		TEXTURES_PARAM_DECL) {
	__global const ImageMap *randomImageMap = &imageMapDescs[tex->imageMapTex.randomImageMapIndex];
	__global const ImageMap *imageMap = &imageMapDescs[tex->imageMapTex.imageMapIndex];
	__global const ImageMap *randomizedTilingLUT = &imageMapDescs[tex->imageMapTex.randomizedTilingLUTIndex];
	
	const float2 noiseP = MAKE_FLOAT2(vertex.x / randomImageMap->desc.width, vertex.y / randomImageMap->desc.height);
	const float3 noise = ImageMap_GetSpectrum(randomImageMap, noiseP.x, noiseP.y IMAGEMAPS_PARAM);
	const float2 pos = MAKE_FLOAT2(.25f, .25f) + MAKE_FLOAT2(noise.x, noise.y) * .5f + offset;

	float3 YCbCr = RGBToYCbCr(ImageMap_GetSpectrum(imageMap, pos.x, pos.y IMAGEMAPS_PARAM));

	YCbCr.x = ImageMap_GetFloat(randomizedTilingLUT, YCbCr.x, .5f IMAGEMAPS_PARAM);

	return YCbCr;
}

OPENCL_FORCE_INLINE float ImageMapTexture_SoftClipContrast(const float x, const float w) {
	const float u = (x > .5f) ? (1.f - x) : x;

	float result;
	if (u >= .5f - .25f * w)
		result = (u - .5f) / w + .5f;
	else if (w >= 2.f / 3.f)
		result = 8.f * (1.f / w - 1.f) * Sqr(u / (2.f - w)) + (3.f - 2.f / w) * u / (2.f - w);
	else if (u >= .5f - .75f * w)
		result = Sqr((u - (.5f - .75f * w)) / w);
	else
		result = 0.f;

	if (x > .5f)
		result = 1.f - result;

	return result;
}

OPENCL_FORCE_NOT_INLINE float3 ImageMapTexture_RandomizedTilingGetSpectrumValue(
		__global const Texture* restrict tex,
		const float2 pos
		TEXTURES_PARAM_DECL) {
	const float latticeToSurface_0_0 = .125f;
	const float latticeToSurface_0_1 = .25f;
	const float latticeToSurface_1_0 = .216506f;
	const float latticeToSurface_1_1 = 0.f;

	const float surfaceToLattice_0_0 = 0.f;
	const float surfaceToLattice_0_1 = 4.6188f;
	const float surfaceToLattice_1_0 = 4.f;
	const float surfaceToLattice_1_1 = -2.3094f;

	const float2 lattice = MAKE_FLOAT2(
		surfaceToLattice_0_0 * pos.x + surfaceToLattice_0_1 * pos.y,
		surfaceToLattice_1_0 * pos.x + surfaceToLattice_1_1 * pos.y
	);
	const float2 cell = MAKE_FLOAT2(floor(lattice.x), floor(lattice.y));
	float2 uv = lattice - cell;

	float2 v0 = cell;
	if (uv.x + uv.y >= 1.f) {
		v0 += MAKE_FLOAT2(1.f, 1.f);
		uv = MAKE_FLOAT2(1.f, 1.f) - MAKE_FLOAT2(uv.y, uv.x);
	}
	float2 v1 = cell + MAKE_FLOAT2(1.f, 0.f);
	float2 v2 = cell + MAKE_FLOAT2(0.f, 1.f);

	const float2 v0offset = MAKE_FLOAT2(
		pos.x - (latticeToSurface_0_0 * v0.x + latticeToSurface_0_1 * v0.y),
		pos.y - (latticeToSurface_1_0 * v0.x + latticeToSurface_1_1 * v0.y)
	);
	const float2 v1offset = MAKE_FLOAT2(
		pos.x - (latticeToSurface_0_0 * v1.x + latticeToSurface_0_1 * v1.y),
		pos.y - (latticeToSurface_1_0 * v1.x + latticeToSurface_1_1 * v1.y)
	);
	const float2 v2offset = MAKE_FLOAT2(
		pos.x - (latticeToSurface_0_0 * v2.x + latticeToSurface_0_1 * v2.y),
		pos.y - (latticeToSurface_1_0 * v2.x + latticeToSurface_1_1 * v2.y)
	);

	const float3 color0 = ImageMapTexture_SampleTile(tex, v0, v0offset TEXTURES_PARAM);
	const float3 color1 = ImageMapTexture_SampleTile(tex, v1, v1offset TEXTURES_PARAM);
	const float3 color2 = ImageMapTexture_SampleTile(tex, v2, v2offset TEXTURES_PARAM);

	float3 uvWeights = MAKE_FLOAT3(1.f - uv.x - uv.y, uv.x, uv.y);

	uvWeights.x = uvWeights.x * uvWeights.x * uvWeights.x;
	uvWeights.y = uvWeights.y * uvWeights.y * uvWeights.y;
	uvWeights.z = uvWeights.z * uvWeights.z * uvWeights.z;

	uvWeights /= uvWeights.x + uvWeights.y + uvWeights.z;

	float3 YCbCr = uvWeights.x * color0 + uvWeights.y * color1 + uvWeights.z * color2;

	YCbCr.x = ImageMapTexture_SoftClipContrast(YCbCr.x, uvWeights.x + uvWeights.y + uvWeights.z);

	YCbCr.x = ImageMap_GetFloat(&imageMapDescs[tex->imageMapTex.randomizedTilingInvLUTIndex], YCbCr.x, .5f IMAGEMAPS_PARAM);

	return YCbCrToRGB(YCbCr);
}

OPENCL_FORCE_INLINE float ImageMapTexture_ConstEvaluateFloat(__global const Texture* restrict tex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	const float2 pos = TextureMapping2D_Map(&tex->imageMapTex.mapping, hitPoint TEXTURES_PARAM);

	const float value = (tex->imageMapTex.randomizedTiling) ?
		Spectrum_Y(ImageMapTexture_RandomizedTilingGetSpectrumValue(tex, pos TEXTURES_PARAM)) :
		ImageMap_GetFloat(&imageMapDescs[tex->imageMapTex.imageMapIndex], pos.x, pos.y IMAGEMAPS_PARAM);
	
	return tex->imageMapTex.gain * value;
}

OPENCL_FORCE_INLINE float3 ImageMapTexture_ConstEvaluateSpectrum(__global const Texture* restrict tex,
		__global const HitPoint *hitPoint
		TEXTURES_PARAM_DECL) {
	const float2 pos = TextureMapping2D_Map(&tex->imageMapTex.mapping, hitPoint TEXTURES_PARAM);

	const float3 value = (tex->imageMapTex.randomizedTiling) ?
		ImageMapTexture_RandomizedTilingGetSpectrumValue(tex, pos TEXTURES_PARAM) :
		ImageMap_GetSpectrum(&imageMapDescs[tex->imageMapTex.imageMapIndex], pos.x, pos.y IMAGEMAPS_PARAM);

	return tex->imageMapTex.gain * value;
}

// Note: ImageMapTexture_Bump() is defined in texture_bump_funcs.cl

OPENCL_FORCE_NOT_INLINE void ImageMapTexture_EvalOp(
		__global const Texture* restrict texture,
		const TextureEvalOpType evalType,
		__global float *evalStack,
		uint *evalStackOffset,
		__global const HitPoint *hitPoint,
		const float sampleDistance
		TEXTURES_PARAM_DECL) {
	switch (evalType) {
		case EVAL_FLOAT: {
			const float eval = ImageMapTexture_ConstEvaluateFloat(texture, hitPoint TEXTURES_PARAM);
			EvalStack_PushFloat(eval);
			break;
		}
		case EVAL_SPECTRUM: {
			const float3 eval = ImageMapTexture_ConstEvaluateSpectrum(texture, hitPoint TEXTURES_PARAM);
			EvalStack_PushFloat3(eval);
			break;
		}
		case EVAL_BUMP: {
			const float3 shadeN = ImageMapTexture_Bump(texture, hitPoint TEXTURES_PARAM);
			EvalStack_PushFloat3(shadeN);
			break;
		}
		default:
			// Something wrong here
			break;
	}
}
