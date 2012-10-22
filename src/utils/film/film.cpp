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

#include "luxrays/utils/film/film.h"

using namespace luxrays;
using namespace luxrays::utils;

Film::Film(const unsigned int w, const unsigned int h, const bool perScreenNorm) {
	filterType = FILTER_GAUSSIAN;
	toneMapParams = new LinearToneMapParams();
	usePerScreenNormalization = perScreenNorm;

	InitGammaTable();
	Init(w, h);
}

Film::~Film() {
	delete toneMapParams;
}

void Film::Init(const unsigned int w, const unsigned int h) {
	width = w;
	height = h;
	pixelCount = w * h;

	statsTotalSampleCount = 0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::InitGammaTable(const float gamma) {
	float x = 0.f;
	const float dx = 1.f / GAMMA_TABLE_SIZE;
	for (unsigned int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
}

void Film::AddFilm(const std::string &filmFile) {
	std::ifstream file;
	file.open(filmFile.c_str(), std::ios::in | std::ios::binary);

	// Check if the file exists
	if (file.is_open()) {
		file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

		SampleFrameBuffer samplePixels(width, height);

		unsigned int tag;
		file.read((char *)&tag, sizeof(unsigned int));
		if (tag != (('S' << 24) | ('L' << 16) | ('G' << 8) | '0'))
			throw std::runtime_error("Unknown film file format");

		unsigned int w;
		file.read((char *)&w, sizeof(unsigned int));
		if (w != width)
			throw std::runtime_error("Film file width doesn't match internal width");

		unsigned int h;
		file.read((char *)&h, sizeof(unsigned int));
		if (h != height)
			throw std::runtime_error("Film file height doesn't match internal height");

		Spectrum spectrum;
		float weight;
		for (unsigned int i = 0; i < pixelCount; ++i) {
			file.read((char *)&spectrum, sizeof(Spectrum));
			file.read((char *)&weight, sizeof(float));

			samplePixels.SetPixel(i, spectrum, weight);
		}
		file.close();

		AddSampleFrameBuffer(&samplePixels);
	} else
		throw std::runtime_error("Film file doesn't exist");
}

void Film::SaveFilm(const std::string &filmFile) {
	const SampleFrameBuffer *samplePixels = GetSampleFrameBuffer();

	std::ofstream file;
	file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
	file.open(filmFile.c_str(), std::ios::out | std::ios::binary);

	const unsigned int tag = ('S' << 24) | ('L' << 16) | ('G' << 8) | '0';
	file.write((char *)&tag, sizeof(unsigned int));
	file.write((char *)&width, sizeof(unsigned int));
	file.write((char *)&height, sizeof(unsigned int));

	for (unsigned int i = 0; i < pixelCount; ++i) {
		const SamplePixel *sp = samplePixels->GetPixel(i);

		file.write((char *)&(sp->radiance), sizeof(Spectrum));
		file.write((char *)&(sp->weight), sizeof(float));
	}

	file.close();
}

void Film::SaveImpl(const std::string &fileName) {
	FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(fileName.c_str());
	if (fif != FIF_UNKNOWN) {
		if ((fif == FIF_HDR) || (fif == FIF_EXR)) {
			const AlphaFrameBuffer *alphaFramebuffer = GetAlphaFrameBuffer();

			if (alphaFramebuffer) {
				// Save the alpha channel too
				FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBAF, width, height, 128);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const SampleFrameBuffer *samplePixels = GetSampleFrameBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						FIRGBAF *pixel = (FIRGBAF *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const unsigned int ridx = y * width + x;

							const SamplePixel *sp = samplePixels->GetPixel(ridx);
							const float weight = sp->weight;
							if (weight == 0.f) {
								pixel[x].red = 0.f;
								pixel[x].green = 0.f;
								pixel[x].blue = 0.f;
								pixel[x].alpha = 0.f;
							} else {
								const float iw = 1.f / weight;
								pixel[x].red = sp->radiance.r * iw;
								pixel[x].green =  sp->radiance.g * iw;
								pixel[x].blue =  sp->radiance.b * iw;

								const AlphaPixel *ap = alphaFramebuffer->GetPixel(ridx);
								pixel[x].alpha = ap->alpha * iw;
							}
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage HDR image");
			} else {
				// No alpha channel available
				FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, width, height, 96);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const SampleFrameBuffer *samplePixels = GetSampleFrameBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						FIRGBF *pixel = (FIRGBF *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const SamplePixel *sp = samplePixels->GetPixel(x, y);
							const float weight = sp->weight;

							if (weight == 0.f) {
								pixel[x].red = 0.f;
								pixel[x].green = 0.f;
								pixel[x].blue = 0.f;
							} else {
								pixel[x].red = sp->radiance.r / weight;
								pixel[x].green =  sp->radiance.g / weight;
								pixel[x].blue =  sp->radiance.b / weight;
							}
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage HDR image");
			}
		} else {
			const AlphaFrameBuffer *alphaFramebuffer = GetAlphaFrameBuffer();

			if (alphaFramebuffer) {
				// Save the alpha channel too
				FIBITMAP *dib = FreeImage_Allocate(width, height, 32);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const float *pixels = GetScreenBuffer();
					const AlphaPixel *alphaPixels = alphaFramebuffer->GetPixels();
					const SampleFrameBuffer *samplePixels = GetSampleFrameBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						BYTE *pixel = (BYTE *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const int offset = 3 * (x + y * width);
							pixel[FI_RGBA_RED] = (BYTE)(pixels[offset] * 255.f + .5f);
							pixel[FI_RGBA_GREEN] = (BYTE)(pixels[offset + 1] * 255.f + .5f);
							pixel[FI_RGBA_BLUE] = (BYTE)(pixels[offset + 2] * 255.f + .5f);

							const SamplePixel *sp = samplePixels->GetPixel(x, y);
							const float weight = sp->weight;

							if (weight == 0.f)
								pixel[FI_RGBA_ALPHA] = (BYTE)0;
							else {
								const int alphaOffset = (x + y * width);
								const float alpha = Clamp(
									alphaPixels[alphaOffset].alpha / weight,
									0.f, 1.f);
								pixel[FI_RGBA_ALPHA] = (BYTE)(alpha * 255.f + .5f);
							}

							pixel += 4;
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage image");
			} else {
				// No alpha channel available
				FIBITMAP *dib = FreeImage_Allocate(width, height, 24);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const float *pixels = GetScreenBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						BYTE *pixel = (BYTE *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const int offset = 3 * (x + y * width);
							pixel[FI_RGBA_RED] = (BYTE)(pixels[offset] * 255.f + .5f);
							pixel[FI_RGBA_GREEN] = (BYTE)(pixels[offset + 1] * 255.f + .5f);
							pixel[FI_RGBA_BLUE] = (BYTE)(pixels[offset + 2] * 255.f + .5f);
							pixel += 3;
						}

						// Next line
						bits += pitch;
					}

					if (!FreeImage_Save(fif, dib, fileName.c_str(), 0))
						throw std::runtime_error("Failed image save");

					FreeImage_Unload(dib);
				} else
					throw std::runtime_error("Unable to allocate FreeImage image");
			}
		}
	} else
		throw std::runtime_error("Image type unknown");
}
