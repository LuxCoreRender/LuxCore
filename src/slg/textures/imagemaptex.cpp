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

#include "luxrays/core/geometry/matrix3x3.h"

#include "slg/textures/imagemaptex.h"
#include "luxrays/core/randomgen.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Static random image map used by some texture
//------------------------------------------------------------------------------

static ImageMap *AllocRandomImageMap(const u_int size) {
	unique_ptr<ImageMap> randomImageMap(ImageMap::AllocImageMap<float>(1.f, 3, size, size, ImageMapStorage::REPEAT));

	// Initialized the random image map

	RandomGenerator rndGen(123);
	float *randomMapData = (float *)randomImageMap->GetStorage()->GetPixelsData();
	for (u_int i = 0; i < 3 * size * size; ++i)
		randomMapData[i] = rndGen.floatValue();
	
	return randomImageMap.release();
}

unique_ptr<ImageMap> ImageMapTexture::randomImageMap(AllocRandomImageMap(512));

//------------------------------------------------------------------------------
// Histogram-preserving Blending for Randomized Texture Tiling functions

// From Brent Burley's "Histogram-preserving Blending for Randomized
// Texture Tiling":  http://www.jcgt.org/published/0008/04/02/paper.pdf
// and
// Benedikt Bitterli's WebGL demo: https://benedikt-bitterli.me/histogram-tiling
//------------------------------------------------------------------------------

#define RT_HISTOGRAM_SIZE 256
#define RT_LUT_SIZE (RT_HISTOGRAM_SIZE * 4)

static inline Spectrum RGBToYCbCr(const Spectrum &rgb) {
	// ITU-R BT.601 from https://en.wikipedia.org/wiki/YCbCr
	static const float RGBToYCbCrOff[3] = {
		 16.f / 255.f,
		128.f / 255.f,
		128.f / 255.f
	};
    static const float RGBToYCbCrMat[3][3] = {
        {  65.481f / 255.f, 128.553f / 255.f,   24.966f / 255.f },
        { -37.797f / 255.f, -74.203f / 255.f,  112.f    / 255.f },
        { 112.f    / 255.f, -93.786f / 255.f, -18.214f  / 255.f }
	};

	// RGB to YCbCr color space transformation
	const float Y  = RGBToYCbCrOff[0] + RGBToYCbCrMat[0][0] * rgb.c[0] + RGBToYCbCrMat[0][1] * rgb.c[1] + RGBToYCbCrMat[0][2] * rgb.c[2];
	const float Cb = RGBToYCbCrOff[1] + RGBToYCbCrMat[1][0] * rgb.c[0] + RGBToYCbCrMat[1][1] * rgb.c[1] + RGBToYCbCrMat[1][2] * rgb.c[2];
	const float Cr = RGBToYCbCrOff[2] + RGBToYCbCrMat[2][0] * rgb.c[0] + RGBToYCbCrMat[2][1] * rgb.c[1] + RGBToYCbCrMat[2][2] * rgb.c[2];

	return Spectrum(Y, Cb, Cr).Clamp(0.f, 1.f);
}

static inline Spectrum YCbCrToRGB(const Spectrum &YCbCr) {
	// ITU-R BT.601 from https://en.wikipedia.org/wiki/YCbCr
	static const float YCbCrToRGBOff[3] = {
		-222.921f / 256.f,
		 135.576f / 256.f,
		-276.836f / 256.f
	};
    static const float YCbCrToRGBMat[3][3] = {
		{ 298.082f / 256.f,    0.f    / 256.f,  408.583f / 256.f },
		{ 298.082f / 256.f, -100.291f / 256.f, -208.120f / 256.f },
		{ 298.082f / 256.f,  516.412f / 256.f,    0.f    / 256.f }
	};

	// YCbCr to RGB color space transformation
	const float r = YCbCrToRGBOff[0] + YCbCrToRGBMat[0][0] * YCbCr.c[0] + YCbCrToRGBMat[0][1] * YCbCr.c[1] + YCbCrToRGBMat[0][2] * YCbCr.c[2];
	const float g = YCbCrToRGBOff[1] + YCbCrToRGBMat[1][0] * YCbCr.c[0] + YCbCrToRGBMat[1][1] * YCbCr.c[1] + YCbCrToRGBMat[1][2] * YCbCr.c[2];
	const float b = YCbCrToRGBOff[2] + YCbCrToRGBMat[2][0] * YCbCr.c[0] + YCbCrToRGBMat[2][1] * YCbCr.c[1] + YCbCrToRGBMat[2][2] * YCbCr.c[2];

	return Spectrum(r, g, b).Clamp(0.f, 1.f);
}

static inline float Erf(float x) {
    static const float a1 =   .254829592f;
    static const float a2 = -0.284496736f;
    static const float a3 =  1.421413741f;
    static const float a4 = -1.453152027f;
    static const float a5 =  1.061405429f;
    static const float p  =   .3275911f;
    
    const float sign = (x < 0.f) ? -1.f : 1.f;
    x = fabsf(x);
    
    const float t = 1.f / (1.f + p * x);
    const float y = 1.f - ((((a5 * t + a4) * t + a3) * t + a2) * t + a1) * t * expf(-x * x);

    return sign * y;
}

static inline float Derf(const float x) {
    return 2.f / sqrtf(M_PI) * expf(-x * x);
}

static inline float ErfInv(const float x) {
    float y = 0.f;
	float err;

    do {
        err = Erf(y) - x;
        y -= err / Derf(y);
    } while (fabsf(err) > 1e-5f);

    return y;
}

static inline float C(const float sigma) {
    return 1.f / Erf(.5f / (sigma * sqrtf(2.f)));
}

static inline float TruncCDFInv(const float x, const float sigma) {
    return .5f + sqrtf(2.f) * sigma * ErfInv((2.f * x - 1.f) / C(sigma));
}

static inline float SoftClipContrast(float x, float W) {
	const float u = (x > .5f) ? (1.f - x) : x;

	float result;
	if (u >= .5f - .25f * W)
		result = (u - .5f) / W + .5f;
	else if (W >= 2.f / 3.f)
		result = 8.f * (1.f / W - 1.f) * Sqr(u / (2.f - W)) + (3.f - 2.f / W) * u / (2.f - W);
	else if (u >= .5f - .75f * W)
		result = Sqr((u - (.5f - .75f * W)) / W);
	else
		result = 0.f;

	if (x > .5f)
		result = 1.f - result;

	return result;
}

Spectrum ImageMapTexture::SampleTile(const UV &vertex, const UV &offset)  const {
	const UV noiseP(vertex.u / randomImageMap->GetWidth(), vertex.v / randomImageMap->GetHeight());
	const Spectrum noise = randomImageMap->GetSpectrum(noiseP);
	const UV pos = UV(.25f, .25f) + UV(noise.c[0], noise.c[1]) * .5f + offset;

	Spectrum YCbCr = RGBToYCbCr(imageMap->GetSpectrum(pos));
	
	YCbCr.c[0] = randomizedTilingLUT->GetFloat(UV(YCbCr.c[0], .5f));

	return YCbCr;
}

Spectrum ImageMapTexture::RandomizedTilingGetSpectrumValue(const UV &pos) const {
	static const float triangleSize = .25f;
    static const float a = triangleSize * cosf(M_PI / 3.f);
	static const float b = triangleSize;
    static const float c = triangleSize * sinf(M_PI / 3.f);
	static const float d = 0.f;
	static const float det = a * d - c * b;
	static const float latticeToSurface[2][2] = {
		{ a, b },
		{ c, d }
	};
	static const float surfaceToLattice[2][2] = {
		{  d / det, -b / det },
		{ -c / det,  a / det }
	};

	const UV lattice(
		surfaceToLattice[0][0] * pos.u + surfaceToLattice[0][1] * pos.v,
		surfaceToLattice[1][0] * pos.u + surfaceToLattice[1][1] * pos.v
	);
	const UV cell = UV(floorf(lattice.u), floorf(lattice.v));
	UV uv = lattice - cell;

	UV v0 = cell;
	if (uv.u + uv.v >= 1.f) {
		v0 += UV(1.f, 1.f);
		uv = UV(1.f, 1.f) - UV(uv.v, uv.u);
	}
	UV v1 = cell + UV(1.f, 0.f);
	UV v2 = cell + UV(0.f, 1.f);

	const UV v0offset(
		pos.u - (latticeToSurface[0][0] * v0.u + latticeToSurface[0][1] * v0.v),
		pos.v - (latticeToSurface[1][0] * v0.u + latticeToSurface[1][1] * v0.v)
	);
	const UV v1offset(
		pos.u - (latticeToSurface[0][0] * v1.u + latticeToSurface[0][1] * v1.v),
		pos.v - (latticeToSurface[1][0] * v1.u + latticeToSurface[1][1] * v1.v)
	);
	const UV v2offset(
		pos.u - (latticeToSurface[0][0] * v2.u + latticeToSurface[0][1] * v2.v),
		pos.v - (latticeToSurface[1][0] * v2.u + latticeToSurface[1][1] * v2.v)
	);

	const Spectrum color0 = SampleTile(v0, v0offset);
	const Spectrum color1 = SampleTile(v1, v1offset);
	const Spectrum color2 = SampleTile(v2, v2offset);

	Vector uvWeights(1.f - uv.u - uv.v, uv.u, uv.v);

	uvWeights.x = uvWeights.x * uvWeights.x * uvWeights.x;
	uvWeights.y = uvWeights.y * uvWeights.y * uvWeights.y;
	uvWeights.z = uvWeights.z * uvWeights.z * uvWeights.z;

	uvWeights /= uvWeights.x + uvWeights.y + uvWeights.z;

	Spectrum YCbCr = uvWeights.x * color0 + uvWeights.y * color1 + uvWeights.z * color2;

	YCbCr.c[0] = SoftClipContrast(YCbCr.c[0], uvWeights.x + uvWeights.y + uvWeights.z);

	YCbCr.c[0] = randomizedTilingInvLUT->GetFloat(UV(YCbCr.c[0], .5f));

	return YCbCrToRGB(YCbCr);
}

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap *img, const TextureMapping2D *mp,
		const float g, const bool rt) :
		imageMap(img), mapping(mp), gain(g), randomizedTiling(rt),
		randomizedTilingLUT(nullptr), randomizedTilingInvLUT(nullptr) {
	if (randomizedTiling) {
		// Preprocessing work for Histogram-preserving Blending for Randomized Texture Tiling

		vector<u_int> histogram(RT_HISTOGRAM_SIZE, 0);

		const u_int width = img->GetWidth();
		const u_int height = img->GetHeight();
		const u_int pixelsCount = width * height;

		for (u_int i = 0; i < pixelsCount; ++i) {
			// Read the pixel RGB
			const Spectrum rgb = imageMap->GetStorage()->GetSpectrum(i);

			// RGB to YCbCr color space transformation
			const Spectrum ycbcr = RGBToYCbCr(rgb);

			// Fill the histogram
			const u_int histogramIndex = Min<u_int>(Floor2UInt(ycbcr.c[0] * RT_HISTOGRAM_SIZE), RT_HISTOGRAM_SIZE - 1);

			histogram[histogramIndex]++;
		}

		// Transform the histogram in a CDF
		for (u_int i = 1; i < RT_HISTOGRAM_SIZE; ++i)
			histogram[i] += histogram[i - 1];

		vector<float> lut(RT_HISTOGRAM_SIZE);
		for (u_int i = 0; i < RT_HISTOGRAM_SIZE; ++i)
			lut[i] = TruncCDFInv(histogram[i] / (float)histogram[RT_HISTOGRAM_SIZE - 1], 1.f / 6.f);

		randomizedTilingLUT = ImageMap::AllocImageMap<float>(1.f, 1, RT_HISTOGRAM_SIZE, 1, ImageMapStorage::CLAMP);
		float *randomizedTilingLUTData = (float *)randomizedTilingLUT->GetStorage()->GetPixelsData();
		for (u_int i = 0; i < RT_HISTOGRAM_SIZE; ++i)
			randomizedTilingLUTData[i] = Clamp(lut[i], 0.f, 1.f);

		// Initialize randomized tiling LUT
		randomizedTilingInvLUT = ImageMap::AllocImageMap<float>(1.f, 1, RT_LUT_SIZE, 1, ImageMapStorage::CLAMP);
		float *randomizedTilingInvLUTData = (float *)randomizedTilingInvLUT->GetStorage()->GetPixelsData();

		for (u_int i = 0; i < RT_LUT_SIZE; ++i) {
			const float f = (i + .5f) / RT_LUT_SIZE;

			randomizedTilingInvLUTData[i] = 1.f;
			for (u_int j = 0; j < RT_HISTOGRAM_SIZE; ++j) {
				if (f < lut[j]) {
					randomizedTilingInvLUTData[i] = j / (float)(RT_HISTOGRAM_SIZE - 1);
					break;
				}
			}
		}
	}
}

ImageMapTexture::~ImageMapTexture() {
	delete mapping;
	delete randomizedTilingLUT;
	delete randomizedTilingInvLUT;
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV pos = mapping->Map(hitPoint);

	const float value = randomizedTiling ? RandomizedTilingGetSpectrumValue(pos).Y() : imageMap->GetFloat(pos);

	return gain * value;
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV pos = mapping->Map(hitPoint);

	const Spectrum value = randomizedTiling ? RandomizedTilingGetSpectrumValue(pos) : imageMap->GetSpectrum(pos);

	return gain * value;
}

Normal ImageMapTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	// TODO: add support for randomizedTiling

	UV dst, du, dv;
	dst = imageMap->GetDuv(mapping->MapDuv(hitPoint, &du, &dv));

	UV duv;
	duv.u = gain * (dst.u * du.u + dst.v * du.v);
	duv.v = gain * (dst.u * dv.u + dst.v * dv.v);
	
	const Vector dpdu = hitPoint.dpdu + duv.u * Vector(hitPoint.shadeN);
	const Vector dpdv = hitPoint.dpdv + duv.v * Vector(hitPoint.shadeN);

	Normal n(Normal(Normalize(Cross(dpdu, dpdv))));

	return ((Dot(n, hitPoint.shadeN) < 0.f) ? -1.f : 1.f) * n;
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache,
		const bool useRealFileName) const {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.textures." + name + ".type")("imagemap"));

	const string fileName = useRealFileName ?
		imageMap->GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props.Set(Property("scene.textures." + name + ".file")(fileName));
	props.Set(Property("scene.textures." + name + ".gain")(gain));
	props.Set(imageMap->ToProperties("scene.textures." + name, false));
	props.Set(mapping->ToProperties("scene.textures." + name + ".mapping"));
	props.Set(Property("scene.textures." + name + ".randomizedtiling.enable")(randomizedTiling));

	return props;
}
