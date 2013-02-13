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
// Texture utility functions
//------------------------------------------------------------------------------

// Perlin Noise Data
#define NOISE_PERM_SIZE 256
static int NoisePerm[2 * NOISE_PERM_SIZE] = {
	151, 160, 137, 91, 90, 15, 131, 13, 201, 95, 96,
	53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142,
	// Rest of noise permutation table
	8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180,
	151, 160, 137, 91, 90, 15,
	131, 13, 201, 95, 96, 53, 194, 233, 7, 225, 140, 36, 103, 30, 69, 142, 8, 99, 37, 240, 21, 10, 23,
	190, 6, 148, 247, 120, 234, 75, 0, 26, 197, 62, 94, 252, 219, 203, 117, 35, 11, 32, 57, 177, 33,
	88, 237, 149, 56, 87, 174, 20, 125, 136, 171, 168, 68, 175, 74, 165, 71, 134, 139, 48, 27, 166,
	77, 146, 158, 231, 83, 111, 229, 122, 60, 211, 133, 230, 220, 105, 92, 41, 55, 46, 245, 40, 244,
	102, 143, 54, 65, 25, 63, 161, 1, 216, 80, 73, 209, 76, 132, 187, 208, 89, 18, 169, 200, 196,
	135, 130, 116, 188, 159, 86, 164, 100, 109, 198, 173, 186, 3, 64, 52, 217, 226, 250, 124, 123,
	5, 202, 38, 147, 118, 126, 255, 82, 85, 212, 207, 206, 59, 227, 47, 16, 58, 17, 182, 189, 28, 42,
	223, 183, 170, 213, 119, 248, 152, 2, 44, 154, 163, 70, 221, 153, 101, 155, 167, 43, 172, 9,
	129, 22, 39, 253, 19, 98, 108, 110, 79, 113, 224, 232, 178, 185, 112, 104, 218, 246, 97, 228,
	251, 34, 242, 193, 238, 210, 144, 12, 191, 179, 162, 241, 81, 51, 145, 235, 249, 14, 239, 107,
	49, 192, 214, 31, 181, 199, 106, 157, 184, 84, 204, 176, 115, 121, 50, 45, 127, 4, 150, 254,
	138, 236, 205, 93, 222, 114, 67, 29, 24, 72, 243, 141, 128, 195, 78, 66, 215, 61, 156, 180
};

static float Grad(int x, int y, int z, float dx, float dy, float dz) {
	const int h = NoisePerm[NoisePerm[NoisePerm[x] + y] + z] & 15;
	const float u = h < 8 || h == 12 || h == 13 ? dx : dy;
	const float v = h < 4 || h == 12 || h == 13 ? dy : dz;
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

static float NoiseWeight(float t) {
	const float t3 = t * t * t;
	const float t4 = t3 * t;
	return 6.f * t4 * t - 15.f * t4 + 10.f * t3;
}

static float Noise(float x, float y, float z) {
	// Compute noise cell coordinates and offsets
	int ix = Floor2Int(x);
	int iy = Floor2Int(y);
	int iz = Floor2Int(z);
	const float dx = x - ix, dy = y - iy, dz = z - iz;
	// Compute gradient weights
	ix &= (NOISE_PERM_SIZE - 1);
	iy &= (NOISE_PERM_SIZE - 1);
	iz &= (NOISE_PERM_SIZE - 1);
	const float w000 = Grad(ix, iy, iz, dx, dy, dz);
	const float w100 = Grad(ix + 1, iy, iz, dx - 1, dy, dz);
	const float w010 = Grad(ix, iy + 1, iz, dx, dy - 1, dz);
	const float w110 = Grad(ix + 1, iy + 1, iz, dx - 1, dy - 1, dz);
	const float w001 = Grad(ix, iy, iz + 1, dx, dy, dz - 1);
	const float w101 = Grad(ix + 1, iy, iz + 1, dx - 1, dy, dz - 1);
	const float w011 = Grad(ix, iy + 1, iz + 1, dx, dy - 1, dz - 1);
	const float w111 = Grad(ix + 1, iy + 1, iz + 1, dx - 1, dy - 1, dz - 1);
	// Compute trilinear interpolation of weights
	const float wx = NoiseWeight(dx);
	const float wy = NoiseWeight(dy);
	const float wz = NoiseWeight(dz);
	const float x00 = Lerp(wx, w000, w100);
	const float x10 = Lerp(wx, w010, w110);
	const float x01 = Lerp(wx, w001, w101);
	const float x11 = Lerp(wx, w011, w111);
	const float y0 = Lerp(wy, x00, x10);
	const float y1 = Lerp(wy, x01, x11);
	return Lerp(wz, y0, y1);
}

static float Noise(const Point &P) {
	return Noise(P.x, P.y, P.z);
}

static float FBm(const Point &P, const float omega, const int maxOctaves) {
	// Compute number of octaves for anti-aliased FBm
	const float foctaves = Min(static_cast<float>(maxOctaves), 1.f);
	const int octaves = Floor2Int(foctaves);
	// Compute sum of octaves of noise for FBm
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (int i = 0; i < octaves; ++i) {
		sum += o * Noise(lambda * P);
		lambda *= 1.99f;
		o *= omega;
	}
	const float partialOctave = foctaves - static_cast<float>(octaves);
	sum += o * SmoothStep(.3f, .7f, partialOctave) *
			Noise(lambda * P);
	return sum;
}

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

ImageMapTexture::ImageMapTexture(const ImageMap * im, const TextureMapping *mp, const float g) :
	imgMap(im), mapping(mp), gain(g) {
	const UV o = mapping->Map(UV(0.f, 0.f));
	const UV i = mapping->Map(UV(imgMap->GetWidth(), imgMap->GetHeight()));
	const UV io = i - o;

	DuDv.u = 1.f / io.u;
	DuDv.v = 1.f / io.v;
}

float ImageMapTexture::GetGreyValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetGrey(mapping->Map(hitPoint.uv));
}

Spectrum ImageMapTexture::GetColorValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetColor(mapping->Map(hitPoint.uv));
}

float ImageMapTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	return imgMap->GetAlpha(mapping->Map(hitPoint.uv));
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "imagemap");
	props.SetString("scene.textures." + name + ".file", "imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imgMap)).str() + ".exr");
	props.SetString("scene.textures." + name + ".gamma", "1.0");
	props.SetString("scene.textures." + name + ".gain", ToString(gain));
	props.Load(mapping->ToProperties("scene.textures." + name));

	return props;
}

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

float ScaleTexture::GetGreyValue(const HitPoint &hitPoint) const {
	return tex1->GetGreyValue(hitPoint) * tex2->GetGreyValue(hitPoint);
}

Spectrum ScaleTexture::GetColorValue(const HitPoint &hitPoint) const {
	return tex1->GetColorValue(hitPoint) * tex2->GetColorValue(hitPoint);
}

float ScaleTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	return tex1->GetAlphaValue(hitPoint) * tex2->GetAlphaValue(hitPoint);
}

UV ScaleTexture::GetDuDv() const {
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

float FresnelApproxNTexture::GetGreyValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetGreyValue(hitPoint));
}

Spectrum FresnelApproxNTexture::GetColorValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetColorValue(hitPoint));
}

float FresnelApproxNTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	return tex->GetAlphaValue(hitPoint);
}

UV FresnelApproxNTexture::GetDuDv() const {
	return tex->GetDuDv();
}

float FresnelApproxKTexture::GetGreyValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetGreyValue(hitPoint));
}

Spectrum FresnelApproxKTexture::GetColorValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetColorValue(hitPoint));
}

float FresnelApproxKTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	return tex->GetAlphaValue(hitPoint);
}

UV FresnelApproxKTexture::GetDuDv() const {
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

//------------------------------------------------------------------------------
// CheckerBoard 2D texture
//------------------------------------------------------------------------------

float CheckerBoard2DTexture::GetGreyValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint.uv);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetGreyValue(hitPoint);
	else
		return tex2->GetGreyValue(hitPoint);
}

Spectrum CheckerBoard2DTexture::GetColorValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint.uv);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetColorValue(hitPoint);
	else
		return tex2->GetColorValue(hitPoint);
}

float CheckerBoard2DTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint.uv);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetAlphaValue(hitPoint);
	else
		return tex2->GetAlphaValue(hitPoint);
}

UV CheckerBoard2DTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties CheckerBoard2DTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "checkerboard2d");
	props.SetString("scene.textures." + name + ".texture1", tex1->GetName());
	props.SetString("scene.textures." + name + ".texture2", tex2->GetName());
	props.Load(mapping->ToProperties("scene.textures." + name));

	return props;
}

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

float MixTexture::GetGreyValue(const HitPoint &hitPoint) const {
	const float amt = amount->GetGreyValue(hitPoint);
	const float value1 = tex1->GetGreyValue(hitPoint);
	const float value2 = tex2->GetGreyValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Spectrum MixTexture::GetColorValue(const HitPoint &hitPoint) const {
	const float amt = amount->GetGreyValue(hitPoint);
	const Spectrum value1 = tex1->GetColorValue(hitPoint);
	const Spectrum value2 = tex2->GetColorValue(hitPoint);

	return Lerp(amt, value1, value2);
}

float MixTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	const float amt = amount->GetGreyValue(hitPoint);
	const float value1 = tex1->GetAlphaValue(hitPoint);
	const float value2 = tex2->GetAlphaValue(hitPoint);

	return Lerp(amt, value1, value2);
}

UV MixTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties MixTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "mix");
	props.SetString("scene.textures." + name + ".amount", amount->GetName());
	props.SetString("scene.textures." + name + ".texture1", tex1->GetName());
	props.SetString("scene.textures." + name + ".texture2", tex2->GetName());

	return props;
}

//------------------------------------------------------------------------------
// FBM texture
//------------------------------------------------------------------------------

float FBMTexture::GetGreyValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint.p));
	const float value = FBm(p, omega, octaves);
	
	return value;
}

Spectrum FBMTexture::GetColorValue(const HitPoint &hitPoint) const {
	return Spectrum(GetGreyValue(hitPoint));
}

float FBMTexture::GetAlphaValue(const HitPoint &hitPoint) const {
	return GetGreyValue(hitPoint);
}

UV FBMTexture::GetDuDv() const {
	return UV(0.001f, 0.001f);
}

Properties FBMTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "fbm");
	props.SetString("scene.textures." + name + ".octaves", ToString(octaves));
	props.SetString("scene.textures." + name + ".roughness", ToString(omega));
	props.Load(mapping->ToProperties("scene.textures." + name));

	return props;
}
