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

#ifndef _LUXRAYS_UTILS_FILM_H
#define	_LUXRAYS_UTILS_FILM_H

#include <cstddef>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include <boost/thread/mutex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/core/pixeldevice.h"
#include "luxrays/core/pixel/samplebuffer.h"

namespace luxrays { namespace utils {

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

#define GAMMA_TABLE_SIZE 1024

class Film {
public:
	Film(Context *context, const unsigned int w, const unsigned int h) {
		ctx = context;
		filterType = FILTER_GAUSSIAN;
		toneMapParams = new LinearToneMapParams();

		InitGammaTable();
		Init(w, h);
	}

	virtual ~Film() {
		delete toneMapParams;
	}

	virtual void Init(const unsigned int w, const unsigned int h) {
		width = w;
		height = h;
		LR_LOG(ctx, "Film size " << width << "x" << height);
		pixelCount = w * h;

		statsTotalSampleCount = 0;
		statsAvgSampleSec = 0.0;
		statsStartSampleTime = WallClockTime();
	}

	virtual void InitGammaTable(const float gamma = 2.2f) {
		float x = 0.f;
		const float dx = 1.f / GAMMA_TABLE_SIZE;
		for (unsigned int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
			gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / gamma);
	}

	void SetFilterType(const FilterType filter) {
		filterType = filter;
	}

	const ToneMapParams *GetToneMapParams() const { return toneMapParams; }
	void SetToneMapParams(const ToneMapParams &params) {
		delete toneMapParams;

		toneMapParams = params.Copy();
	}

	void AddFilm(const std::string &filmFile) {
		std::ifstream file;
		file.open(filmFile.c_str(), std::ios::in | std::ios::binary);

		// Check if the file exists
		if (file.is_open()) {
			file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);

			LR_LOG(ctx, "Loading film file: " << filmFile);

			SampleFrameBuffer sbe(width, height);

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

				sbe.SetPixel(i, spectrum, weight);
			}
			file.close();

			AddSampleFrameBuffer(&sbe);
		} else
			LR_LOG(ctx, "Film file doesn't exist: " << filmFile);
	}

	void SaveFilm(const std::string &filmFile) {
		LR_LOG(ctx, "Saving film file: " << filmFile);

		const SampleFrameBuffer *sbe = GetSampleFrameBuffer();

		std::ofstream file;
		file.exceptions(std::ifstream::eofbit | std::ifstream::failbit | std::ifstream::badbit);
		file.open(filmFile.c_str(), std::ios::out | std::ios::binary);

		const unsigned int tag = ('S' << 24) | ('L' << 16) | ('G' << 8) | '0';
		file.write((char *)&tag, sizeof(unsigned int));
		file.write((char *)&width, sizeof(unsigned int));
		file.write((char *)&height, sizeof(unsigned int));

		for (unsigned int i = 0; i < pixelCount; ++i) {
			const SamplePixel *sp = sbe->GetPixel(i);

			file.write((char *)&(sp->radiance), sizeof(Spectrum));
			file.write((char *)&(sp->weight), sizeof(float));
		}

		file.close();
	}

	void StartSampleTime() {
		statsStartSampleTime = WallClockTime();
	}

	virtual void Reset() {
		statsTotalSampleCount = 0;
		statsAvgSampleSec = 0.0;
		statsStartSampleTime = WallClockTime();
	}

	virtual void UpdateScreenBuffer() = 0;
	virtual const float *GetScreenBuffer() const = 0;

	virtual SampleBuffer *GetFreeSampleBuffer() = 0;
	virtual void FreeSampleBuffer(SampleBuffer *sampleBuffer) = 0;
	virtual void SplatSampleBuffer(const bool preview, SampleBuffer *sampleBuffer) {
		// Update statistics
		statsTotalSampleCount += (unsigned int)sampleBuffer->GetSampleCount();
	}

	unsigned int GetWidth() { return width; }
	unsigned int GetHeight() { return height; }
	unsigned int GetTotalSampleCount() { return statsTotalSampleCount; }
	double GetTotalTime() {
		return WallClockTime() - statsStartSampleTime;
	}
	double GetAvgSampleSec() {
		/*const double elapsedTime = WallClockTime() - statsStartSampleTime;
		const double k = (elapsedTime < 10.0) ? 1.0 : (1.0 / (2.5 * elapsedTime));
		statsAvgSampleSec = k * statsTotalSampleCount / elapsedTime +
				(1.0 - k) * statsAvgSampleSec;

		return statsAvgSampleSec;*/

		const double elapsedTime = WallClockTime() - statsStartSampleTime;
		return statsTotalSampleCount / elapsedTime;
	}

	virtual void Save(const std::string &fileName) = 0;

protected:
	void SaveImpl(const std::string &fileName) {
		LR_LOG(ctx, "Saving " << fileName);

		FREE_IMAGE_FORMAT fif = FreeImage_GetFIFFromFilename(fileName.c_str());
		if (fif != FIF_UNKNOWN) {
			if ((fif == FIF_HDR) || (fif == FIF_EXR)) {
				FIBITMAP *dib = FreeImage_AllocateT(FIT_RGBF, width, height, 96);

				if (dib) {
					unsigned int pitch = FreeImage_GetPitch(dib);
					BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
					const SampleFrameBuffer *sbe = GetSampleFrameBuffer();

					for (unsigned int y = 0; y < height; ++y) {
						FIRGBF *pixel = (FIRGBF *)bits;
						for (unsigned int x = 0; x < width; ++x) {
							const unsigned int ridx = y * width + x;
							const SamplePixel *sp = sbe->GetPixel(ridx);
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
						LR_LOG(ctx, "Failed image save: " << fileName);

					FreeImage_Unload(dib);
				} else
					LR_LOG(ctx, "Unable to allocate FreeImage HDR image: " << fileName);
			} else {
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
						LR_LOG(ctx, "Failed image save: " << fileName);

					FreeImage_Unload(dib);
				} else
					LR_LOG(ctx, "Unable to allocate FreeImage image: " << fileName);
			}
		} else
			LR_LOG(ctx, "Image type unknown: " << fileName);
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = Min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	Spectrum Radiance2Pixel(const Spectrum& c) const {
		return Spectrum(Radiance2PixelFloat(c.r), Radiance2PixelFloat(c.g), Radiance2PixelFloat(c.b));
	}

	virtual const SampleFrameBuffer *GetSampleFrameBuffer() = 0;
	virtual void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) = 0;

	Context *ctx;
	unsigned int width, height;
	unsigned int pixelCount;

	unsigned int statsTotalSampleCount;
	double statsStartSampleTime, statsAvgSampleSec;

	float gammaTable[GAMMA_TABLE_SIZE];

	FilterType filterType;
	ToneMapParams *toneMapParams;
};

//------------------------------------------------------------------------------
// Film implementations
//------------------------------------------------------------------------------

class LuxRaysFilm : public Film {
public:
	LuxRaysFilm(Context *context, const unsigned int w,
			const unsigned int h, DeviceDescription *deviceDesc) : Film(context, w, h) {
		std::vector<DeviceDescription *> descs;
		descs.push_back(deviceDesc);
		pixelDevice = ctx->AddPixelDevices(descs)[0];
		pixelDevice->Init(w, h);
	}

	virtual ~LuxRaysFilm() { }

	virtual void Init(const unsigned int w, const unsigned int h) {
		pixelDevice->Init(w, h);
		Film::Init(w, h);
	}

	virtual void InitGammaTable(const float gamma = 2.2f) {
		pixelDevice->SetGamma(gamma);
	}

	virtual void Reset() {
		pixelDevice->ClearSampleFrameBuffer();
		Film::Reset();
	}

	void UpdateScreenBuffer() {
		pixelDevice->UpdateFrameBuffer(*toneMapParams);
	}

	const float *GetScreenBuffer() const {
		return (const float *)pixelDevice->GetFrameBuffer()->GetPixels();
	}

	SampleBuffer *GetFreeSampleBuffer() {
		return pixelDevice->GetFreeSampleBuffer();
	}

	void FreeSampleBuffer(SampleBuffer *sampleBuffer) {
		pixelDevice->FreeSampleBuffer(sampleBuffer);
	}

	void SplatSampleBuffer(const bool preview, SampleBuffer *sampleBuffer) {
		Film::SplatSampleBuffer(preview, sampleBuffer);

		if (preview)
			pixelDevice->AddSampleBuffer(filterType, sampleBuffer);
		else
			pixelDevice->AddSampleBuffer(FILTER_PREVIEW, sampleBuffer);
	}

	void Save(const std::string &fileName) {
		// Update pixels
		pixelDevice->UpdateFrameBuffer(*toneMapParams);
		SaveImpl(fileName);
	}

protected:
	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return pixelDevice->GetSampleFrameBuffer();
	}

	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) {
		pixelDevice->Merge(sfb);
	}

	PixelDevice *pixelDevice;
};

} }

#endif	/* _LUXRAYS_FILM_FILM_H */
