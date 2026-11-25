/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#include "slg/usings.h"
#include "slg/textures/imagemaptex.h"

using namespace std;
using namespace luxrays;
using namespace slg;


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
    // ITU-T T.871 from https://www.itu.int/rec/T-REC-T.871-201105-I/en
	const float offset = 128.f/255.f;
	static const float RGBToYCbCrOff[3] = {
		0.f,
		offset,
		offset,
	};
    static const float RGBToYCbCrMat[3][3] = {
        {0.299,         0.587,        0.114},
        {-0.299/1.772, -0.587/1.772,  0.886/1.772},
        { 0.701/1.402, -0.587/1.402, -0.114/1.402}
	};

	// RGB to YCbCr color space transformation
	const float Y  = RGBToYCbCrOff[0] + RGBToYCbCrMat[0][0] * rgb.c[0] + RGBToYCbCrMat[0][1] * rgb.c[1] + RGBToYCbCrMat[0][2] * rgb.c[2];
	const float Cb = RGBToYCbCrOff[1] + RGBToYCbCrMat[1][0] * rgb.c[0] + RGBToYCbCrMat[1][1] * rgb.c[1] + RGBToYCbCrMat[1][2] * rgb.c[2];
	const float Cr = RGBToYCbCrOff[2] + RGBToYCbCrMat[2][0] * rgb.c[0] + RGBToYCbCrMat[2][1] * rgb.c[1] + RGBToYCbCrMat[2][2] * rgb.c[2];

	return Spectrum(Y, Cb, Cr).Clamp(0.f, 1.f);
}

static inline Spectrum YCbCrToRGB(const Spectrum &YCbCr) {
	// ITU-T T.871 from https://www.itu.int/rec/T-REC-T.871-201105-I/en
    const float offset = 128.f/255.f;
	static const float YCbCrToRGBMat[3][3] = {
		{1.f,  0.f,          1.402f},
		{1.f, -0.114*1.772/0.587, -0.299*1.402/0.587},
		{1.f,  1.772f,        0.f}
	};
	const float r = YCbCrToRGBMat[0][0] * YCbCr.c[0] + YCbCrToRGBMat[0][1] * (YCbCr.c[1] - offset) + YCbCrToRGBMat[0][2] * (YCbCr.c[2] - offset);
	const float g = YCbCrToRGBMat[1][0] * YCbCr.c[0] + YCbCrToRGBMat[1][1] * (YCbCr.c[1] - offset) + YCbCrToRGBMat[1][2] * (YCbCr.c[2] - offset);
	const float b = YCbCrToRGBMat[2][0] * YCbCr.c[0] + YCbCrToRGBMat[2][1] * (YCbCr.c[1] - offset) + YCbCrToRGBMat[2][2] * (YCbCr.c[2] - offset);

	return Spectrum(r, g, b).Clamp(0.f, 1.f);
}

static inline float Erf(float x) {
	// Note from re-reading: This approximation can be found in equation 7.1.26 of
	// Abramowitz and Stegun: Handbook of Mathematical Functions
	// https://personal.math.ubc.ca/%7Ecbm/aands/page_299.htm
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

static inline float SoftClipContrast(const float x, const float w) {
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

Spectrum ImageMapTexture::SampleTile(const UV &vertex, const UV &offset) const {
	const UV noiseP(vertex.u / randomImageMap.GetWidth(), vertex.v / randomImageMap.GetHeight());
	const Spectrum noise = randomImageMap.GetSpectrum(noiseP);
	const UV pos = UV(.25f, .25f) + UV(noise.c[0], noise.c[1]) * .5f + offset;

	Spectrum YCbCr = RGBToYCbCr(GetImageMap().GetSpectrum(pos));
	
	YCbCr.c[0] = GetRandomizedTilingLUT().GetFloat(UV(YCbCr.c[0], .5f));

	return YCbCr;
}

Spectrum ImageMapTexture::RandomizedTilingGetSpectrumValue(const UV &pos) const {
	/*static const float triangleSize = .25f;
    static const float a = triangleSize * cosf(M_PI / 3.f);
	static const float b = triangleSize;
    static const float c = triangleSize * sinf(M_PI / 3.f);
	static const float d = 0.f;
	static const float det = a * d - c * b;
	static const float latticeToSurface[2][2] = {
		{ a, b },
		{ c, d }
	};
	cout << "latticeToSurface => " <<
			latticeToSurface[0][0] << " " <<
			latticeToSurface[0][1] << " " <<
			latticeToSurface[1][0] << " " <<
			latticeToSurface[1][1] << "\n";
	static const float surfaceToLattice[2][2] = {
		{  d / det, -b / det },
		{ -c / det,  a / det }
	};
	cout << "surfaceToLattice => " <<
			surfaceToLattice[0][0] << " " <<
			surfaceToLattice[0][1] << " " <<
			surfaceToLattice[1][0] << " " <<
			surfaceToLattice[1][1] << "\n";*/

	const float latticeToSurface_0_0 = .125f;
	const float latticeToSurface_0_1 = .25f;
	const float latticeToSurface_1_0 = .216506f;
	const float latticeToSurface_1_1 = 0.f;

	const float surfaceToLattice_0_0 = 0.f;
	const float surfaceToLattice_0_1 = 4.6188f;
	const float surfaceToLattice_1_0 = 4.f;
	const float surfaceToLattice_1_1 = -2.3094f;

	const UV lattice(
		surfaceToLattice_0_0 * pos.u + surfaceToLattice_0_1 * pos.v,
		surfaceToLattice_1_0 * pos.u + surfaceToLattice_1_1 * pos.v
	);
	const UV cell(floorf(lattice.u), floorf(lattice.v));
	UV uv = lattice - cell;

	UV v0 = cell;
	if (uv.u + uv.v >= 1.f) {
		v0 += UV(1.f, 1.f);
		uv = UV(1.f, 1.f) - UV(uv.v, uv.u);
	}
	UV v1 = cell + UV(1.f, 0.f);
	UV v2 = cell + UV(0.f, 1.f);

	const UV v0offset(
		pos.u - (latticeToSurface_0_0 * v0.u + latticeToSurface_0_1 * v0.v),
		pos.v - (latticeToSurface_1_0 * v0.u + latticeToSurface_1_1 * v0.v)
	);
	const UV v1offset(
		pos.u - (latticeToSurface_0_0 * v1.u + latticeToSurface_0_1 * v1.v),
		pos.v - (latticeToSurface_1_0 * v1.u + latticeToSurface_1_1 * v1.v)
	);
	const UV v2offset(
		pos.u - (latticeToSurface_0_0 * v2.u + latticeToSurface_0_1 * v2.v),
		pos.v - (latticeToSurface_1_0 * v2.u + latticeToSurface_1_1 * v2.v)
	);

	const Spectrum color0 = SampleTile(v0, v0offset);
	const Spectrum color1 = SampleTile(v1, v1offset);
	const Spectrum color2 = SampleTile(v2, v2offset);

	Vector uvWeights(1.f - uv.u - uv.v, uv.u, uv.v);

	const float gamma = 3.; // could be exposed as a variable, paper suggests between 2 and 8 might be a valid range
	uvWeights.x = powf(uvWeights.x, gamma);
	uvWeights.y = powf(uvWeights.y, gamma);
	uvWeights.z = powf(uvWeights.z, gamma);

	uvWeights /= uvWeights.x + uvWeights.y + uvWeights.z;

	Spectrum YCbCr = uvWeights.x * color0 + uvWeights.y * color1 + uvWeights.z * color2;

	YCbCr.c[0] = SoftClipContrast(YCbCr.c[0], uvWeights.x + uvWeights.y + uvWeights.z);
    // According to the paper, the following weight W should be used.
    // It appears to increase/improve contrast over the current implementation,
	// but currently increases residual color errors in some texttures.
    // float W = sqrtf(uvWeights.x * uvWeights.x + uvWeights.y * uvWeights.y + uvWeights.z * uvWeights.z);
    // YCbCr.c[0] = SoftClipContrast(YCbCr.c[0], W);

	YCbCr.c[0] = GetRandomizedTilingInvLUT().GetFloat(UV(YCbCr.c[0], .5f));

	return YCbCrToRGB(YCbCr);
}

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTextureUPtr
ImageMapTexture::AllocImageMapTexture(
	const string &texName,
	ImageMapCache &imgMapCache,
	ImageMapConstRef img,
	TextureMapping2DUPtr&& mp,
	const float g,
	const bool rt,
	ImageMapConstRef randomIM
) {
	auto imt = std::make_unique<ImageMapTexture>(
		texName, img, std::move(mp), g, rt, randomIM
	);

	if (rt) {
		// I need to add the LUTs to the ImageMapCache
		imt->refRandomizedTilingLUT = &imgMapCache.DefineImageMap(
			std::move(imt->randomizedTilingLUT)
		);
		imt->refRandomizedTilingInvLUT = &imgMapCache.DefineImageMap(
			std::move(imt->randomizedTilingInvLUT)
		);
	}

	return imt;
}

ImageMapTexture::ImageMapTexture(
	const string &texName,
	ImageMapConstRef img,
	TextureMapping2DUPtr&& mp,
	const float g,
	const bool rt,
	ImageMapConstRef randomIM
) :
	imageMap(img),
	mapping(std::move(mp)),
	gain(g),
	randomizedTiling(rt),
	randomImageMap(randomIM)
{
	SetName(texName);

	if (randomizedTiling) {
		// Preprocessing work for Histogram-preserving Blending for Randomized Texture Tiling

		vector<u_int> histogram(RT_HISTOGRAM_SIZE, 0);

		const u_int width = img.GetWidth();
		const u_int height = img.GetHeight();
		const u_int pixelsCount = width * height;

		for (u_int i = 0; i < pixelsCount; ++i) {
			// Read the pixel RGB
			const Spectrum rgb = GetImageMap().GetStorage().GetSpectrum(i);

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

		randomizedTilingLUT = ImageMap::AllocImageMap(1, RT_HISTOGRAM_SIZE, 1,
				ImageMapConfig(1.f,
					ImageMapStorage::StorageType::FLOAT,
					ImageMapStorage::WrapType::CLAMP,
					ImageMapStorage::ChannelSelectionType::DEFAULT));
		randomizedTilingLUT->SetName(this->GetName() + "_#_randomizedTilingLUT");
		float *randomizedTilingLUTData = (float *)randomizedTilingLUT->GetStorage().GetPixelsData();
		for (u_int i = 0; i < RT_HISTOGRAM_SIZE; ++i)
			randomizedTilingLUTData[i] = Clamp(lut[i], 0.f, 1.f);

		// Initialize randomized tiling inverse LUT
		randomizedTilingInvLUT = ImageMap::AllocImageMap(1, RT_LUT_SIZE, 1,
				ImageMapConfig(1.f,
					ImageMapStorage::StorageType::FLOAT,
					ImageMapStorage::WrapType::CLAMP,
					ImageMapStorage::ChannelSelectionType::DEFAULT));
		randomizedTilingInvLUT->SetName(this->GetName() + "_#_randomizedTilingInvLUT");
		float *randomizedTilingInvLUTData = (float *)randomizedTilingInvLUT->GetStorage().GetPixelsData();

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
	// TODO
	//
	//mapping = nullptr;
	// randomizedTilingLUT and randomizedTilingInvLUT are deleted by ImageMapCache 
}

void ImageMapTexture::AddReferencedImageMaps(std::unordered_set<const ImageMap *> &referencedImgMaps) const {
	referencedImgMaps.insert(&imageMap);
	if (randomizedTilingLUT)
		referencedImgMaps.insert(refRandomizedTilingLUT);
	if (randomizedTilingInvLUT)
		referencedImgMaps.insert(refRandomizedTilingInvLUT);
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV pos = mapping->Map(hitPoint);

	const float value = randomizedTiling ? RandomizedTilingGetSpectrumValue(pos).Y() : GetImageMap().GetFloat(pos);

	return gain * value;
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV pos = mapping->Map(hitPoint);

	const Spectrum value = randomizedTiling ? RandomizedTilingGetSpectrumValue(pos) : GetImageMap().GetSpectrum(pos);

	return gain * value;
}

Normal ImageMapTexture::Bump(const HitPoint &hitPoint, const float sampleDistance) const {
	UV dst, du, dv;
	dst = GetImageMap().GetDuv(mapping->MapDuv(hitPoint, &du, &dv));

	UV duv;
	duv.u = gain * (dst.u * du.u + dst.v * du.v);
	duv.v = gain * (dst.u * dv.u + dst.v * dv.v);
	
	const Vector dpdu = hitPoint.dpdu + duv.u * Vector(hitPoint.shadeN);
	const Vector dpdv = hitPoint.dpdv + duv.v * Vector(hitPoint.shadeN);

	Normal n(Normal(Normalize(Cross(dpdu, dpdv))));

	return ((Dot(n, hitPoint.shadeN) < 0.f) ? -1.f : 1.f) * n;
}

PropertiesUPtr ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache,
		const bool useRealFileName) const {
	auto props = std::make_unique<Properties>();

	const string name = GetName();
	props->Set(Property("scene.textures." + name + ".type")("imagemap"));

	const string fileName = useRealFileName ?
		GetImageMap().GetName() : imgMapCache.GetSequenceFileName(imageMap);
	props->Set(Property("scene.textures." + name + ".file")(fileName));
	props->Set(Property("scene.textures." + name + ".gain")(gain));
	props->Set(GetImageMap().ToProperties("scene.textures." + name, false));
	props->Set(mapping->ToProperties("scene.textures." + name + ".mapping"));
	props->Set(Property("scene.textures." + name + ".randomizedtiling.enable")(randomizedTiling));

	return props;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
