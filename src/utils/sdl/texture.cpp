/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include <boost/format.hpp>

#include "luxrays/utils/sdl/sdl.h"
#include "luxrays/utils/sdl/texture.h"
#include "luxrays/utils/sdl/bsdf.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// TextureDefinitions
//------------------------------------------------------------------------------

TextureDefinitions::TextureDefinitions() { }

TextureDefinitions::~TextureDefinitions() {
	for (std::vector<Texture *>::const_iterator it = texs.begin(); it != texs.end(); ++it)
		delete (*it);
}

void TextureDefinitions::DefineTexture(const std::string &name, Texture *t) {
	texs.push_back(t);
	texsByName.insert(std::make_pair(name, t));
}

Texture *TextureDefinitions::GetTexture(const std::string &name) {
	// Check if the texture has been already defined
	std::map<std::string, Texture *>::const_iterator it = texsByName.find(name);

	if (it == texsByName.end())
		throw std::runtime_error("Reference to an undefined texture: " + name);
	else
		return it->second;
}

std::vector<std::string> TextureDefinitions::GetTextureNames() const {
	std::vector<std::string> names;
	names.reserve(texs.size());
	for (std::map<std::string, Texture *>::const_iterator it = texsByName.begin(); it != texsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void TextureDefinitions::DeleteTexture(const std::string &name) {
	const u_int index = GetTextureIndex(name);
	texs.erase(texs.begin() + index);
	texsByName.erase(name);
}

u_int TextureDefinitions::GetTextureIndex(const Texture *t) const {
	for (u_int i = 0; i < texs.size(); ++i) {
		if (t == texs[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined texture: " + boost::lexical_cast<std::string>(t));
}

u_int TextureDefinitions::GetTextureIndex(const std::string &name) {
	return GetTextureIndex(GetTexture(name));
}

//------------------------------------------------------------------------------
// ImageMap
//------------------------------------------------------------------------------

ImageMap::ImageMap(const std::string &fileName, const float g) {
	gamma = g;

	SDL_LOG("Reading texture map: " << fileName);

	FREE_IMAGE_FORMAT fif = FreeImage_GetFileType(fileName.c_str(), 0);
	if(fif == FIF_UNKNOWN)
		fif = FreeImage_GetFIFFromFilename(fileName.c_str());

	if((fif != FIF_UNKNOWN) && FreeImage_FIFSupportsReading(fif)) {
		FIBITMAP *dib = FreeImage_Load(fif, fileName.c_str(), 0);

		if (!dib)
			throw std::runtime_error("Unable to read texture map: " + fileName);

		width = FreeImage_GetWidth(dib);
		height = FreeImage_GetHeight(dib);

		u_int pitch = FreeImage_GetPitch(dib);
		FREE_IMAGE_TYPE imageType = FreeImage_GetImageType(dib);
		u_int bpp = FreeImage_GetBPP(dib);
		BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

		if ((imageType == FIT_RGBAF) && (bpp == 128)) {
			channelCount = 3;
			SDL_LOG("HDR RGB (128bit) texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				FIRGBAF *pixel = (FIRGBAF *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[x].red;
						pixels[offset + 1] = pixel[x].green;
						pixels[offset + 2] = pixel[x].blue;
					} else {
						// Apply reverse gamma correction
						pixels[offset] = powf(pixel[x].red, gamma);
						pixels[offset + 1] = powf(pixel[x].green, gamma);
						pixels[offset + 2] = powf(pixel[x].blue, gamma);
					}
				}

				// Next line
				bits += pitch;
			}
		} else if ((imageType == FIT_RGBF) && (bpp == 96)) {
			channelCount = 3;
			SDL_LOG("HDR RGB (96bit) texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				FIRGBF *pixel = (FIRGBF *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[x].red;
						pixels[offset + 1] = pixel[x].green;
						pixels[offset + 2] = pixel[x].blue;
					} else {
						// Apply reverse gamma correction
						pixels[offset] = powf(pixel[x].red, gamma);
						pixels[offset + 1] = powf(pixel[x].green, gamma);
						pixels[offset + 2] = powf(pixel[x].blue, gamma);
					}
				}

				// Next line
				bits += pitch;
			}
		} else if ((imageType == FIT_FLOAT) && (bpp == 32)) {
			channelCount = 1;
			SDL_LOG("HDR FLOAT (32bit) texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				float *pixel = (float *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f)
						pixels[offset] = pixel[x];
					else {
						// Apply reverse gamma correction
						pixels[offset] = powf(pixel[x], gamma);
					}
				}

				// Next line
				bits += pitch;
			}
		} else if ((imageType == FIT_BITMAP) && (bpp == 32)) {
			channelCount = 4;
			SDL_LOG("RGBA texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				BYTE *pixel = (BYTE *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[FI_RGBA_RED] / 255.f;
						pixels[offset + 1] = pixel[FI_RGBA_GREEN] / 255.f;
						pixels[offset + 2] = pixel[FI_RGBA_BLUE] / 255.f;
					} else {
						pixels[offset] = powf(pixel[FI_RGBA_RED] / 255.f, gamma);
						pixels[offset + 1] = powf(pixel[FI_RGBA_GREEN] / 255.f, gamma);
						pixels[offset + 2] = powf(pixel[FI_RGBA_BLUE] / 255.f, gamma);
					}
					pixels[offset + 3] = pixel[FI_RGBA_ALPHA] / 255.f;
					pixel += 4;
				}

				// Next line
				bits += pitch;
			}
		} else if (bpp == 24) {
			channelCount = 3;
			SDL_LOG("RGB texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height * channelCount];

			for (u_int y = 0; y < height; ++y) {
				BYTE *pixel = (BYTE *)bits;
				for (u_int x = 0; x < width; ++x) {
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f) {
						pixels[offset] = pixel[FI_RGBA_RED] / 255.f;
						pixels[offset + 1] = pixel[FI_RGBA_GREEN] / 255.f;
						pixels[offset + 2] = pixel[FI_RGBA_BLUE] / 255.f;
					} else {
						pixels[offset] = powf(pixel[FI_RGBA_RED] / 255.f, gamma);
						pixels[offset + 1] = powf(pixel[FI_RGBA_GREEN] / 255.f, gamma);
						pixels[offset + 2] = powf(pixel[FI_RGBA_BLUE] / 255.f, gamma);
					}
					pixel += 3;
				}

				// Next line
				bits += pitch;
			}
		} else if (bpp == 8) {
			channelCount = 1;
			SDL_LOG("Grey texture map size: " << width << "x" << height << " (" <<
					width * height * sizeof(float) * channelCount / 1024 << "Kbytes)");
			pixels = new float[width * height];

			for (u_int y = 0; y < height; ++y) {
				BYTE pixel;
				for (u_int x = 0; x < width; ++x) {
					FreeImage_GetPixelIndex(dib, x, y, &pixel);
					const u_int offset = (x + (height - y - 1) * width) * channelCount;

					if (gamma == 1.f)
						pixels[offset] = pixel / 255.f;
					else
						pixels[offset] = powf(pixel / 255.f, gamma);
				}

				// Next line
				bits += pitch;
			}
		} else {
			std::stringstream msg;
			msg << "Unsupported bitmap depth (" << bpp << ") in a texture map: " << fileName;
			throw std::runtime_error(msg.str());
		}

		FreeImage_Unload(dib);
	} else
		throw std::runtime_error("Unknown image file format: " + fileName);
}

ImageMap::ImageMap(float *p, const float g, const u_int count,
		const u_int w, const u_int h) {
	gamma = g;

	pixels = p;

	channelCount = count;
	width = w;
	height = h;
}

ImageMap::~ImageMap() {
	delete[] pixels;
}

void ImageMap::WriteImage(const std::string &fileName) const {
	if (channelCount == 4) {
		// RGBA image
		FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBAF, width, height, 128);

		if (dib) {
			unsigned int pitch = FreeImage_GetPitch(dib);
			BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

			for (unsigned int y = 0; y < height; ++y) {
				FIRGBAF *pixel = (FIRGBAF *)bits;
				for (unsigned int x = 0; x < width; ++x) {
					const unsigned int ridx = (height - y - 1) * width + x;

					pixel[x].red = pixels[ridx * channelCount];
					pixel[x].green = pixels[ridx * channelCount + 1];
					pixel[x].blue = pixels[ridx * channelCount + 2];
					pixel[x].alpha = pixels[ridx * channelCount + 3];
				}

				// Next line
				bits += pitch;
			}

			if (!FreeImage_Save(FIF_EXR, dib, fileName.c_str(), 0))
				throw std::runtime_error("Failed image save");

			FreeImage_Unload(dib);
		} else
			throw std::runtime_error("Unable to allocate FreeImage HDR image");
	} else if (channelCount == 3) {
		// RGB image
		FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, width, height, 96);

		if (dib) {
			unsigned int pitch = FreeImage_GetPitch(dib);
			BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

			for (unsigned int y = 0; y < height; ++y) {
				FIRGBF *pixel = (FIRGBF *)bits;
				for (unsigned int x = 0; x < width; ++x) {
					const unsigned int ridx = (height - y - 1) * width + x;

					pixel[x].red = pixels[ridx * channelCount];
					pixel[x].green = pixels[ridx * channelCount + 1];
					pixel[x].blue = pixels[ridx * channelCount + 2];
				}

				// Next line
				bits += pitch;
			}

			if (!FreeImage_Save(FIF_EXR, dib, fileName.c_str(), 0))
				throw std::runtime_error("Failed image save");

			FreeImage_Unload(dib);
		} else
			throw std::runtime_error("Unable to allocate FreeImage HDR image");
	} else if (channelCount == 1) {
		// Grey image
		FIBITMAP *dib = FreeImage_AllocateT(FIT_FLOAT, width, height, 32);

		if (dib) {
			unsigned int pitch = FreeImage_GetPitch(dib);
			BYTE *bits = (BYTE *)FreeImage_GetBits(dib);

			for (unsigned int y = 0; y < height; ++y) {
				float *pixel = (float *)bits;
				for (unsigned int x = 0; x < width; ++x) {
					const unsigned int ridx = (height - y - 1) * width + x;

					pixel[x] = pixels[ridx * channelCount];
				}

				// Next line
				bits += pitch;
			}

			if (!FreeImage_Save(FIF_EXR, dib, fileName.c_str(), 0))
				throw std::runtime_error("Failed image save");

			FreeImage_Unload(dib);
		} else
			throw std::runtime_error("Unable to allocate FreeImage HDR image");
	} else
		throw std::runtime_error("Unknown channel count in ImageMap::writeImage(" + fileName + "): " + boost::lexical_cast<std::string>(channelCount));
}

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

ImageMapCache::ImageMapCache() {
}

ImageMapCache::~ImageMapCache() {
	for (std::map<std::string, ImageMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
		delete it->second;
}

ImageMap *ImageMapCache::GetImageMap(const std::string &fileName, const float gamma) {
	// Check if the texture map has been already loaded
	std::map<std::string, ImageMap *>::const_iterator it = maps.find(fileName);

	if (it == maps.end()) {
		// I have yet to load the file

		ImageMap *im = new ImageMap(fileName, gamma);
		maps.insert(std::make_pair(fileName, im));

		return im;
	} else {
		//SDL_LOG("Cached image map: " << fileName);
		ImageMap *im = (it->second);
		if (im->GetGamma() != gamma)
			throw std::runtime_error("Image map: " + fileName + " can not be used with 2 different gamma");
		return im;
	}
}

void ImageMapCache::DefineImgMap(const std::string &name, ImageMap *tm) {
	SDL_LOG("Define ImageMap: " << name);
	maps.insert(std::make_pair(name, tm));
}

u_int ImageMapCache::GetImageMapIndex(const ImageMap *im) const {
	// TODO: use a std::map
	u_int i = 0;
	for (std::map<std::string, ImageMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it) {
		if (it->second == im)
			return i;
		else
			++i;
	}

	throw std::runtime_error("Unknown image map: " + boost::lexical_cast<std::string>(im));
}

void ImageMapCache::GetImageMaps(std::vector<ImageMap *> &ims) {
	ims.reserve(maps.size());
	for (std::map<std::string, ImageMap *>::const_iterator it = maps.begin(); it != maps.end(); ++it)
		ims.push_back(it->second);
}

//------------------------------------------------------------------------------
// ConstFloat texture
//------------------------------------------------------------------------------

Properties ConstFloatTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "constfloat1");
	props.SetString("scene.textures." + name + ".value", ToString(value));

	return props;
}

//------------------------------------------------------------------------------
// ConstFloat3 texture
//------------------------------------------------------------------------------

Properties ConstFloat3Texture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "constfloat3");
	props.SetString("scene.textures." + name + ".value",
			ToString(color.r) + " " + ToString(color.g) + " " + ToString(color.b));

	return props;
}

//------------------------------------------------------------------------------
// ConstFloat4 texture
//------------------------------------------------------------------------------

Properties ConstFloat4Texture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "constfloat4");
	props.SetString("scene.textures." + name + ".value",
			ToString(color.r) + " " + ToString(color.g) + " " + ToString(color.b) + " " + ToString(alpha));

	return props;
}

//------------------------------------------------------------------------------
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap * im, const UVMapping &mp, const float g) :
	imgMap(im), mapping(mp), gain(g) {
	const UV uv = mapping.GetDuDv();
	DuDv.u = uv.u * (1.f / imgMap->GetWidth());
	DuDv.v = uv.v * (1.f / imgMap->GetHeight());
}

float ImageMapTexture::GetGreyValue(const BSDF &bsdf) const {
	return gain * imgMap->GetGrey(mapping.Map(bsdf.hitPointUV));
}

Spectrum ImageMapTexture::GetColorValue(const BSDF &bsdf) const {
	return gain * imgMap->GetColor(mapping.Map(bsdf.hitPointUV));
}

float ImageMapTexture::GetAlphaValue(const BSDF &bsdf) const {
	return imgMap->GetAlpha(mapping.Map(bsdf.hitPointUV));
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "imagemap");
	props.SetString("scene.textures." + name + ".file", "imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imgMap)).str() + ".exr");
	props.SetString("scene.textures." + name + ".gamma", "1.0");
	props.SetString("scene.textures." + name + ".gain", ToString(gain));
	props.SetString("scene.textures." + name + ".uvscale", ToString(mapping.GetUScale()) + " " + ToString(mapping.GetVScale()));
	props.SetString("scene.textures." + name + ".uvdelta", ToString(mapping.GetUDelta()) + " " + ToString(mapping.GetVDelta()));

	return props;
}

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

float ScaleTexture::GetGreyValue(const BSDF &bsdf) const {
	return tex1->GetGreyValue(bsdf) * tex2->GetGreyValue(bsdf);
}

Spectrum ScaleTexture::GetColorValue(const BSDF &bsdf) const {
	return tex1->GetColorValue(bsdf) * tex2->GetColorValue(bsdf);
}

float ScaleTexture::GetAlphaValue(const BSDF &bsdf) const {
	return tex1->GetAlphaValue(bsdf) * tex2->GetAlphaValue(bsdf);
}

const UV ScaleTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties ScaleTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "scale");
	props.SetString("scene.textures." + name + ".texture1", tex1->GetName());
	props.SetString("scene.textures." + name + ".texture2", tex2->GetName());

	return props;
}

//------------------------------------------------------------------------------
// FresnelApproxN & FresnelApproxK texture
//------------------------------------------------------------------------------

float FresnelApproxN(const float Fr) {
	const float sqrtReflectance = sqrtf(Clamp(Fr, 0.f, .999f));

	return (1.f + sqrtReflectance) /
		(1.f - sqrtReflectance);
}

Spectrum FresnelApproxN(const Spectrum &Fr) {
	const Spectrum sqrtReflectance = Sqrt(Fr.Clamp(0.f, .999f));

	return (Spectrum(1.f) + sqrtReflectance) /
		(Spectrum(1.f) - sqrtReflectance);
}

float FresnelApproxK(const float Fr) {
	const float reflectance = Clamp(Fr, 0.f, .999f);

	return 2.f * sqrtf(reflectance /
		(1.f - reflectance));
}

Spectrum FresnelApproxK(const Spectrum &Fr) {
	const Spectrum reflectance = Fr.Clamp(0.f, .999f);

	return 2.f * Sqrt(reflectance /
		(Spectrum(1.f) - reflectance));
}

float FresnelApproxNTexture::GetGreyValue(const BSDF &bsdf) const {
	return FresnelApproxN(tex->GetGreyValue(bsdf));
}

Spectrum FresnelApproxNTexture::GetColorValue(const BSDF &bsdf) const {
	return FresnelApproxN(tex->GetColorValue(bsdf));
}

float FresnelApproxNTexture::GetAlphaValue(const BSDF &bsdf) const {
	return tex->GetAlphaValue(bsdf);
}

const UV FresnelApproxNTexture::GetDuDv() const {
	return tex->GetDuDv();
}

float FresnelApproxKTexture::GetGreyValue(const BSDF &bsdf) const {
	return FresnelApproxK(tex->GetGreyValue(bsdf));
}

Spectrum FresnelApproxKTexture::GetColorValue(const BSDF &bsdf) const {
	return FresnelApproxK(tex->GetColorValue(bsdf));
}

float FresnelApproxKTexture::GetAlphaValue(const BSDF &bsdf) const {
	return tex->GetAlphaValue(bsdf);
}

const UV FresnelApproxKTexture::GetDuDv() const {
	return tex->GetDuDv();
}

Properties FresnelApproxNTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "fresnelapproxn");
	props.SetString("scene.textures." + name + ".texture", tex->GetName());

	return props;
}

Properties FresnelApproxKTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "fresnelapproxk");
	props.SetString("scene.textures." + name + ".texture", tex->GetName());

	return props;
}
