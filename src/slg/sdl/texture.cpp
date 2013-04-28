/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include <sstream>
#include <boost/format.hpp>
#include <boost/foreach.hpp>

#include "slg/sdl/sdl.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/bsdf.h"

using namespace luxrays;
using namespace slg;

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

static float Noise(float x, float y = .5f, float z = .5f) {
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
	const float foctaves = static_cast<float>(maxOctaves);
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

static float Turbulence(const Point &P, const float omega, const int maxOctaves) {
	// Compute number of octaves for anti-aliased FBm
	const float foctaves = static_cast<float>(maxOctaves);
	const int octaves = Floor2Int(foctaves);
	// Compute sum of octaves of noise for turbulence
	float sum = 0.f, lambda = 1.f, o = 1.f;
	for (int i = 0; i < octaves; ++i) {
		sum += o * fabsf(Noise(lambda * P));
		lambda *= 1.99f;
		o *= omega;
	}
	const float partialOctave = foctaves - static_cast<float>(octaves);
	sum += o * SmoothStep(.3f, .7f, partialOctave) *
	       fabsf(Noise(lambda * P));

	// finally, add in value to account for average value of fabsf(Noise())
	// (~0.2) for the remaining octaves...
	sum += (maxOctaves - foctaves) * 0.2f;

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

		Init(dib);

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

void ImageMap::Init(FIBITMAP *dib) {
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
		msg << "Unsupported bitmap depth (" << bpp << ") in a texture map";
		throw std::runtime_error(msg.str());
	}
}

void ImageMap::Resize(const u_int newWidth, const u_int newHeight) {
	if ((width == newHeight) && (height == newHeight))
		return;

	// Convert the image in FreeImage format
	FIBITMAP *dib = GetFreeImageBitMap();

	// I can delete the current image
	delete[] pixels;
	pixels = NULL;

	FIBITMAP *scaleDib = FreeImage_Rescale(dib, newWidth, newHeight, FILTER_CATMULLROM);
	FreeImage_Unload(dib);

	Init(scaleDib);
	
	FreeImage_Unload(scaleDib);
}

FIBITMAP *ImageMap::GetFreeImageBitMap() const {
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

			return dib;
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

			return dib;
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

			return dib;
		} else
			throw std::runtime_error("Unable to allocate FreeImage HDR image");
	} else
		throw std::runtime_error("Unknown channel count in ImageMap::GetFreeImageBitMap(): " + boost::lexical_cast<std::string>(channelCount));
}

void ImageMap::WriteImage(const std::string &fileName) const {
	FIBITMAP *dib = GetFreeImageBitMap();

	if (!FreeImage_Save(FIF_EXR, dib, fileName.c_str(), 0))
		throw std::runtime_error("Failed image save");

	FreeImage_Unload(dib);
}

//------------------------------------------------------------------------------
// ImageMapCache
//------------------------------------------------------------------------------

ImageMapCache::ImageMapCache() {
	allImageScale = 1.f;
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
		const u_int width = im->GetWidth();
		const u_int height = im->GetHeight();

		// Scale the image if required
		if (allImageScale > 1.f) {
			// Enlarge all images
			const u_int newWidth = width * allImageScale;
			const u_int newHeight = height * allImageScale;
			im->Resize(newWidth, newHeight);
		} else if ((allImageScale < 1.f) && (width > 128) && (height > 128)) {
			const u_int newWidth = Max<u_int>(128, width * allImageScale);
			const u_int newHeight = Max<u_int>(128, height * allImageScale);
			im->Resize(newWidth, newHeight);
		}

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
// ImageMap texture
//------------------------------------------------------------------------------

ImageMapTexture::ImageMapTexture(const ImageMap * im, const TextureMapping2D *mp, const float g) :
	imgMap(im), mapping(mp), gain(g) {
	const UV o = mapping->Map(UV(0.f, 0.f));
	const UV i = mapping->Map(UV(imgMap->GetWidth(), imgMap->GetHeight()));
	const UV io = i - o;

	DuDv.u = 1.f / io.u;
	DuDv.v = 1.f / io.v;
}

float ImageMapTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetFloat(mapping->Map(hitPoint));
}

Spectrum ImageMapTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return gain * imgMap->GetSpectrum(mapping->Map(hitPoint));
}

Properties ImageMapTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "imagemap");
	props.SetString("scene.textures." + name + ".file", "imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imgMap)).str() + ".exr");
	props.SetString("scene.textures." + name + ".gamma", "1.0");
	props.SetString("scene.textures." + name + ".gain", ToString(gain));
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Scale texture
//------------------------------------------------------------------------------

float ScaleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return tex1->GetFloatValue(hitPoint) * tex2->GetFloatValue(hitPoint);
}

Spectrum ScaleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex1->GetSpectrumValue(hitPoint) * tex2->GetSpectrumValue(hitPoint);
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

float FresnelApproxNTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxNTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxN(tex->GetSpectrumValue(hitPoint));
}

UV FresnelApproxNTexture::GetDuDv() const {
	return tex->GetDuDv();
}

float FresnelApproxKTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetFloatValue(hitPoint));
}

Spectrum FresnelApproxKTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return FresnelApproxK(tex->GetSpectrumValue(hitPoint));
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
// CheckerBoard 2D & 3D texture
//------------------------------------------------------------------------------

float CheckerBoard2DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard2DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	if ((Floor2Int(uv.u) + Floor2Int(uv.v)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
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
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

float CheckerBoard3DTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	if ((Floor2Int(p.x) + Floor2Int(p.y) + Floor2Int(p.z)) % 2 == 0)
		return tex1->GetFloatValue(hitPoint);
	else
		return tex2->GetFloatValue(hitPoint);
}

Spectrum CheckerBoard3DTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const Point p = mapping->Map(hitPoint);
	if ((Floor2Int(p.x) + Floor2Int(p.y) + Floor2Int(p.z)) % 2 == 0)
		return tex1->GetSpectrumValue(hitPoint);
	else
		return tex2->GetSpectrumValue(hitPoint);
}

UV CheckerBoard3DTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties CheckerBoard3DTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "checkerboard3d");
	props.SetString("scene.textures." + name + ".texture1", tex1->GetName());
	props.SetString("scene.textures." + name + ".texture2", tex2->GetName());
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Mix texture
//------------------------------------------------------------------------------

float MixTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const float value1 = tex1->GetFloatValue(hitPoint);
	const float value2 = tex2->GetFloatValue(hitPoint);

	return Lerp(amt, value1, value2);
}

Spectrum MixTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float amt = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);
	const Spectrum value1 = tex1->GetSpectrumValue(hitPoint);
	const Spectrum value2 = tex2->GetSpectrumValue(hitPoint);

	return Lerp(amt, value1, value2);
}

UV MixTexture::GetDuDv() const {
	const UV uv1 = amount->GetDuDv();
	const UV uv2 = tex1->GetDuDv();
	const UV uv3 = tex2->GetDuDv();

	return UV(Max(Max(uv1.u, uv2.u), uv3.u), Max(Max(uv1.v, uv2.v), uv3.v));
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

float FBMTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint));
	const float value = FBm(p, omega, octaves);
	
	return value;
}

Spectrum FBMTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

UV FBMTexture::GetDuDv() const {
	return UV(DUDV_VALUE, DUDV_VALUE);
}

Properties FBMTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "fbm");
	props.SetString("scene.textures." + name + ".octaves", ToString(octaves));
	props.SetString("scene.textures." + name + ".roughness", ToString(omega));
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Marble texture
//------------------------------------------------------------------------------

Spectrum MarbleTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	Point P(mapping->Map(hitPoint));
	P *= scale;

	float marble = P.y + variation * FBm(P, omega, octaves);
	float t = .5f + .5f * sinf(marble);
	// Evaluate marble spline at _t_
	static float c[9][3] = {
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{ .5f, .5f, .5f},
		{ .6f, .59f, .58f},
		{ .58f, .58f, .6f},
		{ .58f, .58f, .6f},
		{.2f, .2f, .33f},
		{ .58f, .58f, .6f}
	};
#define NC  sizeof(c) / sizeof(c[0])
#define NSEG (NC-3)
	int first = Floor2Int(t * NSEG);
	t = (t * NSEG - first);
#undef NC
#undef NSEG
	Spectrum c0(c[first]), c1(c[first + 1]), c2(c[first + 2]), c3(c[first + 3]);
	// Bezier spline evaluated with de Castilejau's algorithm
	Spectrum s0(Lerp(t, c0, c1));
	Spectrum s1(Lerp(t, c1, c2));
	Spectrum s2(Lerp(t, c2, c3));
	s0 = Lerp(t, s0, s1);
	s1 = Lerp(t, s1, s2);
	// Extra scale of 1.5 to increase variation among colors
	return 1.5f * Lerp(t, s0, s1);
}

float MarbleTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

UV MarbleTexture::GetDuDv() const {
	return UV(DUDV_VALUE, DUDV_VALUE);
}

Properties MarbleTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "marble");
	props.SetString("scene.textures." + name + ".octaves", ToString(octaves));
	props.SetString("scene.textures." + name + ".roughness", ToString(omega));
	props.SetString("scene.textures." + name + ".scale", ToString(scale));
	props.SetString("scene.textures." + name + ".variation", ToString(variation));
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Dots texture
//------------------------------------------------------------------------------

float DotsTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);

	const int sCell = Floor2Int(uv.u + .5f);
	const int tCell = Floor2Int(uv.v + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f);
		const float ds = uv.u - sCenter, dt = uv.v - tCenter;
		if (ds * ds + dt * dt < radius * radius)
			return insideTex->GetFloatValue(hitPoint);
	}

	return outsideTex->GetFloatValue(hitPoint);
}

Spectrum DotsTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);

	const int sCell = Floor2Int(uv.u + .5f);
	const int tCell = Floor2Int(uv.v + .5f);
	// Return _insideDot_ result if point is inside dot
	if (Noise(sCell + .5f, tCell + .5f) > 0.f) {
		const float radius = .35f;
		const float maxShift = 0.5f - radius;
		const float sCenter = sCell + maxShift *
			Noise(sCell + 1.5f, tCell + 2.8f);
		const float tCenter = tCell + maxShift *
			Noise(sCell + 4.5f, tCell + 9.8f);
		const float ds = uv.u - sCenter, dt = uv.v - tCenter;
		if (ds * ds + dt * dt < radius * radius)
			return insideTex->GetSpectrumValue(hitPoint);
	}

	return outsideTex->GetSpectrumValue(hitPoint);
}

UV DotsTexture::GetDuDv() const {
	const UV uv1 = insideTex->GetDuDv();
	const UV uv2 = outsideTex->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties DotsTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "dots");
	props.SetString("scene.textures." + name + ".inside", insideTex->GetName());
	props.SetString("scene.textures." + name + ".outside", outsideTex->GetName());
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Brick texture
//------------------------------------------------------------------------------

BrickTexture::BrickTexture(const TextureMapping3D *mp, const Texture *t1,
		const Texture *t2, const Texture *t3,
		float brickw, float brickh, float brickd, float mortar,
		float r, float bev, const std::string &b) :
		mapping(mp), tex1(t1), tex2(t3), tex3(t3),
		brickwidth(brickw), brickheight(brickh), brickdepth(brickd), mortarsize(mortar),
		run(r), initialbrickwidth(brickw), initialbrickheight(brickh), initialbrickdepth(brickd) {
	if (b == "stacked") {
		bond = RUNNING;
		run = 0.f;
	} else if (b == "flemish")
		bond = FLEMISH;
	else if (b == "english") {
		bond = ENGLISH;
		run = 0.25f;
	} else if (b == "herringbone")
		bond = HERRINGBONE;
	else if (b == "basket")
		bond = BASKET;
	else if (b == "chain link") {
		bond = KETTING;
		run = 1.25f;
		offset = Point(.25f, -1.f, 0.f);
	} else {
		bond = RUNNING;
		offset = Point(0.f, -.5f , 0.f);
	}
	if (bond == HERRINGBONE || bond == BASKET) {
		proportion = floorf(brickwidth / brickheight);
		brickdepth = brickheight = brickwidth;
		invproportion = 1.f / proportion;
	}
	mortarwidth = mortarsize / brickwidth;
	mortarheight = mortarsize / brickheight;
	mortardepth = mortarsize / brickdepth;
	bevelwidth = bev / brickwidth;
	bevelheight = bev / brickheight;
	beveldepth = bev / brickdepth;
	usebevel = bev > 0.f;
}

float BrickTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

bool BrickTexture::RunningAlternate(const Point &p, Point &i, Point &b,
		int nWhole) const {
	const float sub = nWhole + 0.5f;
	const float rsub = ceilf(sub);
	i.z = floorf(p.z);
	b.x = (p.x + i.z * run) / sub;
	b.y = (p.y + i.z * run) / sub;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.x = (b.x - i.x) * sub;
	b.y = (b.y - i.y) * sub;
	b.z = (p.z - i.z) * sub;
	i.x += floor(b.x) / rsub;
	i.y += floor(b.y) / rsub;
	b.x -= floor(b.x);
	b.y -= floor(b.y);
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

bool BrickTexture::Basket(const Point &p, Point &i) const {
	i.x = floorf(p.x);
	i.y = floorf(p.y);
	float bx = p.x - i.x;
	float by = p.y - i.y;
	i.x += i.y - 2.f * floorf(0.5f * i.y);
	const bool split = (i.x - 2.f * floor(0.5f * i.x)) < 1.f;
	if (split) {
		bx = fmodf(bx, invproportion);
		i.x = floorf(proportion * p.x) * invproportion;
	} else {
		by = fmodf(by, invproportion);
		i.y = floorf(proportion * p.y) * invproportion;
	}
	return by > mortardepth && bx > mortarwidth;
}

bool BrickTexture::Herringbone(const Point &p, Point &i) const {
	i.y = floorf(proportion * p.y);
	const float px = p.x + i.y * invproportion;
	i.x = floorf(px);
	float bx = 0.5f * px - floorf(px * 0.5f);
	bx *= 2.f;
	float by = proportion * p.y - floorf(proportion * p.y);
	by *= invproportion;
	if (bx > 1.f + invproportion) {
		bx = proportion * (bx - 1.f);
		i.y -= floorf(bx - 1.f);
		bx -= floorf(bx);
		bx *= invproportion;
		by = 1.f;
	} else if (bx > 1.f) {
		bx = proportion * (bx - 1.f);
		i.y -= floorf(bx - 1.f);
		bx -= floorf(bx);
		bx *= invproportion;
	}
	return by > mortarheight && bx > mortarwidth;
}

bool BrickTexture::Running(const Point &p, Point &i, Point &b) const {
	i.z = floorf(p.z);
	b.x = p.x + i.z * run;
	b.y = p.y - i.z * run;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.z = p.z - i.z;
	b.x -= i.x;
	b.y -= i.y;
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

bool BrickTexture::English(const Point &p, Point &i, Point &b) const {
	i.z = floorf(p.z);
	b.x = p.x + i.z * run;
	b.y = p.y - i.z * run;
	i.x = floorf(b.x);
	i.y = floorf(b.y);
	b.z = p.z - i.z;
	const float divider = floorf(fmodf(fabsf(i.z), 2.f)) + 1.f;
	b.x = (divider * b.x - floorf(divider * b.x)) / divider;
	b.y = (divider * b.y - floorf(divider * b.y)) / divider;
	return b.z > mortarheight && b.y > mortardepth &&
		b.x > mortarwidth;
}

Spectrum BrickTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
#define BRICK_EPSILON 1e-3f
	const Point P(mapping->Map(hitPoint));

	const float offs = BRICK_EPSILON + mortarsize;
	Point bP(P + Point(offs, offs, offs));

	// Normalize coordinates according brick dimensions
	bP.x /= brickwidth;
	bP.y /= brickdepth;
	bP.z /= brickheight;

	bP += offset;

	Point brickIndex;
	Point bevel;
	bool b;
	switch (bond) {
		case FLEMISH:
			b = RunningAlternate(bP, brickIndex, bevel, 1);
			break;
		case RUNNING:
			b = Running(bP, brickIndex, bevel);
			break;
		case ENGLISH:
			b = English(bP, brickIndex, bevel);
			break;
		case HERRINGBONE:
			b = Herringbone(bP, brickIndex);
			break;
		case BASKET:
			b = Basket(bP, brickIndex);
			break;
		case KETTING:
			b = RunningAlternate(bP, brickIndex, bevel, 2);
			break; 
		default:
			b = true;
			break;
	}

	if (b) {
		// Brick texture * Modulation texture
		return tex1->GetSpectrumValue(hitPoint) * tex3->GetSpectrumValue(hitPoint);
	} else {
		// Mortar texture
		return tex2->GetSpectrumValue(hitPoint);
	}
#undef BRICK_EPSILON
}


UV BrickTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();
	const UV uv3 = tex3->GetDuDv();

	return UV(Max(Max(uv1.u, uv2.u), uv3.u), Max(Max(uv1.v, uv2.v), uv3.v));
}

Properties BrickTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "brick");
	props.SetString("scene.textures." + name + ".bricktex", tex1->GetName());
	props.SetString("scene.textures." + name + ".mortartex", tex2->GetName());
	props.SetString("scene.textures." + name + ".brickmodtex", tex3->GetName());
	props.SetString("scene.textures." + name + ".brickwidth", ToString(brickwidth));
	props.SetString("scene.textures." + name + ".brickheight", ToString(brickheight));
	props.SetString("scene.textures." + name + ".brickdepth", ToString(brickdepth));
	props.SetString("scene.textures." + name + ".mortarsize", ToString(mortarsize));
	props.SetString("scene.textures." + name + ".brickrun", ToString(run));
	props.SetString("scene.textures." + name + ".brickbevel", ToString(bevelwidth * brickwidth));

	std::string brickBondValue;
	switch (bond) {
		case FLEMISH:
			brickBondValue = "flemish";
			break;
		case ENGLISH:
			brickBondValue = "english";
			break;
		case HERRINGBONE:
			brickBondValue = "herringbone";
			break;
		case BASKET:
			brickBondValue = "basket";
			break;
		case KETTING:
			brickBondValue = "chain link";
			break;
		default:
		case RUNNING:
			brickBondValue = "stacked";
			break;
	}
	props.SetString("scene.textures." + name + ".brickbond", brickBondValue);

	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Add texture
//------------------------------------------------------------------------------

float AddTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return tex1->GetFloatValue(hitPoint) + tex2->GetFloatValue(hitPoint);
}

Spectrum AddTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return tex1->GetSpectrumValue(hitPoint) + tex2->GetSpectrumValue(hitPoint);
}

UV AddTexture::GetDuDv() const {
	const UV uv1 = tex1->GetDuDv();
	const UV uv2 = tex2->GetDuDv();

	return UV(Max(uv1.u, uv2.u), Max(uv1.v, uv2.v));
}

Properties AddTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "add");
	props.SetString("scene.textures." + name + ".texture1", tex1->GetName());
	props.SetString("scene.textures." + name + ".texture2", tex2->GetName());

	return props;
}

//------------------------------------------------------------------------------
// Windy texture
//------------------------------------------------------------------------------

float WindyTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point P(mapping->Map(hitPoint));
	const float windStrength = FBm(.1f * P, .5f, 3);
	const float waveHeight = FBm(P, .5f, 6);

	return fabsf(windStrength) * waveHeight;
}

Spectrum WindyTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

UV WindyTexture::GetDuDv() const {
	return UV(DUDV_VALUE, DUDV_VALUE);
}

Properties WindyTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "windy");

	return props;
}

//------------------------------------------------------------------------------
// Wrinkled texture
//------------------------------------------------------------------------------

float WrinkledTexture::GetFloatValue(const HitPoint &hitPoint) const {
	const Point p(mapping->Map(hitPoint));
	return Turbulence(p, omega, octaves);
}

Spectrum WrinkledTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(GetFloatValue(hitPoint));
}

UV WrinkledTexture::GetDuDv() const {
	return UV(DUDV_VALUE, DUDV_VALUE);
}

Properties WrinkledTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "wrinkled");
	props.SetString("scene.textures." + name + ".octaves", ToString(octaves));
	props.SetString("scene.textures." + name + ".roughness", ToString(omega));
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// UV texture
//------------------------------------------------------------------------------

float UVTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum UVTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const UV uv = mapping->Map(hitPoint);
	
	return Spectrum(uv.u - Floor2Int(uv.u), uv.v - Floor2Int(uv.v), 0.f);
}

UV UVTexture::GetDuDv() const {
	return UV(DUDV_VALUE, DUDV_VALUE);
}

Properties UVTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "uv");
	props.Load(mapping->ToProperties("scene.textures." + name + ".mapping"));

	return props;
}

//------------------------------------------------------------------------------
// Band texture
//------------------------------------------------------------------------------

float BandTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return GetSpectrumValue(hitPoint).Y();
}

Spectrum BandTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float a = Clamp(amount->GetFloatValue(hitPoint), 0.f, 1.f);

	if (a < offsets.front())
		return values.front();
	if (a >= offsets.back())
		return values.back();
	// std::upper_bound is not available on OpenCL
	//const u_int p = std::upper_bound(offsets.begin(), offsets.end(), a) - offsets.begin();
	u_int p = 0;
	for (; p < offsets.size(); ++p) {
		if (a < offsets[p])
			break;
	}

	return Lerp((a - offsets[p - 1]) / (offsets[p] - offsets[p - 1]),
			values[p - 1], values[p]);
}

UV BandTexture::GetDuDv() const {
	return amount->GetDuDv();
}

Properties BandTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "band");
	props.SetString("scene.textures." + name + ".amount", amount->GetName());

	for (u_int i = 0; i < offsets.size(); ++i) {
		props.SetString("scene.textures." + name + ".offset" + ToString(i), ToString(offsets[i]));
		props.SetString("scene.textures." + name + ".value" + ToString(i),
				ToString(values[i].r) + " " + ToString(values[i].g) + " " + ToString(values[i].b));
	}

	return props;
}

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

float HitPointColorTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.color.Y();
}

Spectrum HitPointColorTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return hitPoint.color;
}

Properties HitPointColorTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "hitpointcolor");

	return props;
}

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

float HitPointAlphaTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return hitPoint.alpha;
}

Spectrum HitPointAlphaTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	return Spectrum(hitPoint.alpha);
}

Properties HitPointAlphaTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "hitpointalpha");

	return props;
}

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

float HitPointGreyTexture::GetFloatValue(const HitPoint &hitPoint) const {
	return (channel > 2) ? hitPoint.color.Y() : hitPoint.color[channel];
}

Spectrum HitPointGreyTexture::GetSpectrumValue(const HitPoint &hitPoint) const {
	const float v = (channel > 2) ? hitPoint.color.Y() : hitPoint.color[channel];
	return Spectrum(v);
}

Properties HitPointGreyTexture::ToProperties(const ImageMapCache &imgMapCache) const {
	Properties props;

	const std::string name = GetName();
	props.SetString("scene.textures." + name + ".type", "hitpointgrey");
	props.SetString("scene.textures." + name + ".channel", ToString(
		((channel != 0) && (channel != 1) && (channel != 2)) ? -1 : ((int)channel)));

	return props;
}
