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

#ifndef _FILM_H
#define	_FILM_H

#include <cstddef>
#include <cmath>

#include <png.h>
#include <boost/thread/mutex.hpp>

#include "smalllux.h"
#include "sampler.h"
#include "samplebuffer.h"

class GaussianFilter {
public:

	GaussianFilter(float xw, float yw, float a) : xWidth(xw), yWidth(yw),
	invXWidth(1.f / xw), invYWidth(1.f / yw) {
		alpha = a;
		expX = expf(-alpha * xWidth * xWidth);
		expY = expf(-alpha * yWidth * yWidth);
	}

	~GaussianFilter() {
	}

	float Evaluate(float x, float y) const {
		return Gaussian(x, expX) * Gaussian(y, expY);
	}

	const float xWidth, yWidth;
	const float invXWidth, invYWidth;

private:
	float alpha;
	float expX, expY;

	float Gaussian(float d, float expv) const {
		return max(0.f, expf(-alpha * d * d) - expv);
	}
};

class Film {
public:
	Film(const bool lowLatencyMode, const unsigned int w, unsigned int h) {
		lowLatency = lowLatencyMode;

		Init(w, h);
	}

	virtual ~Film() { }

	virtual void Init(const unsigned int w, unsigned int h) {
		width = w;
		height = h;
		cerr << "Film size " << width << "x" << height << endl;

		statsTotalSampleCount = 0;
		statsAvgSampleSec = 0.0;
		statsStartSampleTime = WallClockTime();
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

	virtual void SplatSampleBuffer(const SampleBuffer *sampleBuffer) {
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
		const double elapsedTime = WallClockTime() - statsStartSampleTime;
		const double k = (elapsedTime < 10.0) ? 1.0 : (1.0 / (2.5 * elapsedTime));
		statsAvgSampleSec = k * statsTotalSampleCount / elapsedTime +
				(1.0 - k) * statsAvgSampleSec;

		return statsAvgSampleSec;
	}

	virtual void SavePPM(const string &fileName) {
		const float *pixels = GetScreenBuffer();

		ofstream file;
		file.exceptions(ifstream::eofbit | ifstream::failbit | ifstream::badbit);
		file.open(fileName.c_str(), ios::out);

		file << "P3\n" << width << " " << height << "\n255\n";

		for (unsigned int y = 0; y < height; ++y) {
			for (unsigned int x = 0; x < width; ++x) {
				const int offset = 3 * (x + (height - y - 1) * width);
				const int r = (int)(pixels[offset] * 255.f + .5f);
				const int g = (int)(pixels[offset + 1] * 255.f + .5f);
				const int b = (int)(pixels[offset + 2] * 255.f + .5f);

				file << r << " " << g << " " << b << " ";
			}
		}

		file.close();
	}

	virtual void SavePNG(const string &fileName) {
		const float *pixels = GetScreenBuffer();

		FILE *fp = fopen(fileName.c_str(), "wb");

		png_byte color_type = PNG_COLOR_TYPE_RGB;
		png_byte bit_depth = 16;
		png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
		png_infop info_ptr = png_create_info_struct(png_ptr);
		png_init_io(png_ptr, fp);

		png_set_IHDR(png_ptr, info_ptr, width, height,
				bit_depth, color_type, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

		png_write_info(png_ptr, info_ptr);

		std::vector<png_uint_16> row(width * 4);

		for (unsigned int y = 0; y < height; ++y) {
			int i = 0;

			for (unsigned int x = 0; x < width; ++x) {
				const int offset = 3 * (x + (height - y - 1) * width);

				png_uint_16 r = static_cast<png_uint_16> (pixels[offset]*255.0f + .5f);
				png_uint_16 g = static_cast<png_uint_16> (pixels[offset + 1]*255.0f + .5f);
				png_uint_16 b = static_cast<png_uint_16> (pixels[offset + 2]*255.0f + .5f);

				row[i++] = r;
				row[i++] = g;
				row[i++] = b;
			}

			png_write_row(png_ptr, reinterpret_cast<png_bytep> (&row[0]));
		}

		png_write_end(png_ptr, NULL);
		fclose(fp);
	}

protected:
	unsigned int width, height;
	unsigned int pixelCount;

	unsigned int statsTotalSampleCount;
	double statsStartSampleTime, statsAvgSampleSec;

	bool lowLatency;
};

#define GAMMA_TABLE_SIZE 1024
#define FILTER_TABLE_SIZE 16

class StandardFilm : public Film {
public:
	StandardFilm(const bool lowLatencyMode, const unsigned int w, unsigned int h) :
		Film(lowLatencyMode, w, h) {
		pixelsRadiance = NULL;
		pixelWeights = NULL;
		pixels = NULL;

		InitGammaTable();

		Init(w, h);
	}

	virtual ~StandardFilm() {
		if (pixelsRadiance)
			delete[] pixelsRadiance;
		if (pixelWeights)
			delete[] pixelWeights;
		if (pixels)
			delete[] pixels;
	}

	virtual void Init(const unsigned int w, unsigned int h) {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (pixelsRadiance)
			delete[] pixelsRadiance;
		if (pixelWeights)
			delete[] pixelWeights;
		if (pixels)
			delete[] pixels;

		pixelCount = w * h;

		pixelsRadiance = new Spectrum[pixelCount];
		pixels = new float[pixelCount * 3];
		pixelWeights = new float[pixelCount];

		for (unsigned int i = 0, j = 0; i < pixelCount; ++i) {
			pixelsRadiance[i] = 0.f;
			pixelWeights[i] = 0.f;
			pixels[j++] = 0.f;
			pixels[j++] = 0.f;
			pixels[j++] = 0.f;
		}

		Film::Init(w, h);
	}

	virtual void Reset() {
		boost::mutex::scoped_lock lock(radianceMutex);

		for (unsigned int i = 0; i < pixelCount; ++i) {
			pixelsRadiance[i] = 0.f;
			pixelWeights[i] = 0.f;
		}

		Film::Reset();
	}

	void UpdateScreenBuffer() {
		boost::mutex::scoped_lock lock(radianceMutex);

		UpdateScreenBufferImpl();
	}

	const float *GetScreenBuffer() const {
		return pixels;
	}

	void SplatSampleBuffer(const SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
			SplatSampleBufferElem(&sbe[i]);

		Film::SplatSampleBuffer(sampleBuffer);
	}

	void SavePPM(const string &fileName) {
		boost::mutex::scoped_lock lock(radianceMutex);

		// Update pixels
		UpdateScreenBufferImpl();

		Film::SavePPM(fileName);
	}

	void SavePNG(const string &fileName) {
		boost::mutex::scoped_lock lock(radianceMutex);

		// Update pixels
		UpdateScreenBufferImpl();

		Film::SavePNG(fileName);
	}

protected:
	float gammaTable[GAMMA_TABLE_SIZE];

	void InitGammaTable() {
		float x = 0.f;
		const float dx = 1.f / GAMMA_TABLE_SIZE;
		for (int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
			gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	void SplatSampleBufferElem(const SampleBufferElem *sampleElem) {
		int x = (int)sampleElem->screenX;
		int y = (int)sampleElem->screenY;

		const unsigned int offset = x + y * width;

		pixelsRadiance[offset] += sampleElem->radiance;
		pixelWeights[offset] += 1.f;
	}

	void UpdateScreenBufferImpl() {
		unsigned int count = width * height;
		for (unsigned int i = 0, j = 0; i < count; ++i) {
			const float weight = pixelWeights[i];

			if (weight == 0.f)
				j +=3;
			else {
				const float invWeight = 1.f / weight;

				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].r * invWeight);
				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].g * invWeight);
				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].b * invWeight);
			}
		}
	}

	boost::mutex radianceMutex;
	Spectrum *pixelsRadiance;
	float *pixelWeights;

	float *pixels;
};

class BluredStandardFilm : public StandardFilm {
public:
	BluredStandardFilm(const bool lowLatencyMode, const unsigned int w, unsigned int h) :
		StandardFilm(lowLatencyMode, w, h) {
	}

	~BluredStandardFilm() {
	}

	void Init(const unsigned int w, unsigned int h) {
		StandardFilm::Init(w, h);

		// Out of the lock but doesn't matter, Init(0 is called in single thread anyway
		useLargeFilter = lowLatency;
	}

	void Reset() {
		StandardFilm::Reset();

		// Out of the lock but doesn't matter, Init(0 is called in single thread anyway
		useLargeFilter = lowLatency;
	}

	void SplatSampleBuffer(const SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (useLargeFilter) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				BluredSplatSampleBufferElem(&sbe[i]);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i]);
		}

		Film::SplatSampleBuffer(sampleBuffer);
	}

private:
	void BluredSplatSampleBufferElem(const SampleBufferElem *sampleElem) {
		useLargeFilter = (useLargeFilter && (sampleElem->pass < 32));

		const int splatSize = 4;

		// Compute sample's raster extent
		float dImageX = sampleElem->screenX - 0.5f;
		float dImageY = sampleElem->screenY - 0.5f;
		int x0 = Ceil2Int(dImageX - splatSize);
		int x1 = Floor2Int(dImageX + splatSize);
		int y0 = Ceil2Int(dImageY - splatSize);
		int y1 = Floor2Int(dImageY + splatSize);
		if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
			return;

		for (u_int y = static_cast<u_int>(max<int>(y0, 0)); y <= static_cast<u_int>(min<int>(y1, height - 1)); ++y)
			for (u_int x = static_cast<u_int>(max<int>(x0, 0)); x <= static_cast<u_int>(min<int>(x1, width - 1)); ++x) {
				const unsigned int offset = x + y * width;

				pixelsRadiance[offset] += 0.01f * sampleElem->radiance;
				pixelWeights[offset] += 0.01f;
			}
	}

	bool useLargeFilter;
};

class GaussianFilm : public Film {
public:
	GaussianFilm(const bool lowLatencyMode, const unsigned int w, unsigned int h) :
		Film(lowLatencyMode, w, h),  filter2x2(2.f, 2.f, 2.f),  filter4x4(4.f, 4.f, 0.05f) {
		pixelsRadiance = NULL;
		pixelWeights = NULL;
		pixels = NULL;

        // Precompute filter weight table
		filterTable2x2 = new float[FILTER_TABLE_SIZE * FILTER_TABLE_SIZE];
		float *ftp2x2 = filterTable2x2;
		for (u_int y = 0; y < FILTER_TABLE_SIZE; ++y) {
			const float fy = (static_cast<float>(y) + .5f) * filter2x2.yWidth / FILTER_TABLE_SIZE;
			for (u_int x = 0; x < FILTER_TABLE_SIZE; ++x) {
				const float fx = (static_cast<float>(x) + .5f) * filter2x2.xWidth / FILTER_TABLE_SIZE;
				*ftp2x2++ = filter2x2.Evaluate(fx, fy);
			}
		}

		filterTable4x4 = new float[FILTER_TABLE_SIZE * FILTER_TABLE_SIZE];
		float *ftp10x10 = filterTable4x4;
		for (u_int y = 0; y < FILTER_TABLE_SIZE; ++y) {
			const float fy = (static_cast<float>(y) + .5f) * filter4x4.yWidth / FILTER_TABLE_SIZE;
			for (u_int x = 0; x < FILTER_TABLE_SIZE; ++x) {
				const float fx = (static_cast<float>(x) + .5f) * filter4x4.xWidth / FILTER_TABLE_SIZE;
				*ftp10x10++ = filter4x4.Evaluate(fx, fy);
			}
		}

		InitGammaTable();

		Init(w, h);
	}

	virtual ~GaussianFilm() {
		if (pixelsRadiance)
			delete[] pixelsRadiance;
		if (pixelWeights)
			delete[] pixelWeights;
		if (pixels)
			delete[] pixels;

		delete[] filterTable2x2;
		delete[] filterTable4x4;
	}

	void Init(const unsigned int w, unsigned int h) {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (pixelsRadiance)
			delete[] pixelsRadiance;
		if (pixelWeights)
			delete[] pixelWeights;
		if (pixels)
			delete[] pixels;

		pixelCount = w * h;

		pixelsRadiance = new Spectrum[pixelCount];
		pixels = new float[pixelCount * 3];
		pixelWeights = new float[pixelCount];

		for (unsigned int i = 0, j = 0; i < pixelCount; ++i) {
			pixelsRadiance[i] = 0.f;
			pixelWeights[i] = 0.f;
			pixels[j++] = 0.f;
			pixels[j++] = 0.f;
			pixels[j++] = 0.f;
		}

		useLargeFilter = lowLatency;

		Film::Init(w, h);
	}

	void Reset() {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (lowLatency) {
			for (unsigned int i = 0; i < pixelCount; ++i) {
				if (pixelWeights[i] != 0.0f) {
					pixelsRadiance[i] /= 100.0f * pixelWeights[i];
					pixelWeights[i] = 0.01f;
				}
			}
		} else {
			for (unsigned int i = 0; i < pixelCount; ++i) {
				pixelsRadiance[i] = 0.f;
				pixelWeights[i] = 0.f;
			}
		}

		useLargeFilter = lowLatency;

		Film::Reset();
	}

	void UpdateScreenBuffer() {
		boost::mutex::scoped_lock lock(radianceMutex);

		UpdateScreenBufferImpl();
	}

	const float *GetScreenBuffer() const {
		return pixels;
	}

	virtual void SplatSampleBuffer(const SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (useLargeFilter) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter4x4, filterTable4x4);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter2x2, filterTable2x2);
		}

		Film::SplatSampleBuffer(sampleBuffer);
	}

	void SavePPM(const string &fileName) {
		boost::mutex::scoped_lock lock(radianceMutex);

		// Update pixels
		UpdateScreenBufferImpl();

		Film::SavePPM(fileName);
	}

	void SavePNG(const string &fileName) {
		boost::mutex::scoped_lock lock(radianceMutex);

		// Update pixels
		UpdateScreenBufferImpl();

		Film::SavePNG(fileName);
	}

protected:
	float gammaTable[GAMMA_TABLE_SIZE];

	void InitGammaTable() {
		float x = 0.f;
		const float dx = 1.f / GAMMA_TABLE_SIZE;
		for (int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
			gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	void SplatRadiance(const Spectrum radiance, const unsigned int x, const unsigned int y, const float weight = 1.f) {
		const unsigned int offset = x + y * width;

		pixelsRadiance[offset] += weight * radiance;
		pixelWeights[offset] += weight;
	}

	void SplatSampleBufferElem(const SampleBufferElem *sampleElem, const GaussianFilter &filter, const float *filterTable) {
		useLargeFilter = (useLargeFilter && (sampleElem->pass < 32));

		// Compute sample's raster extent
		float dImageX = sampleElem->screenX - 0.5f;
		float dImageY = sampleElem->screenY - 0.5f;
		int x0 = Ceil2Int(dImageX - filter.xWidth);
		int x1 = Floor2Int(dImageX + filter.xWidth);
		int y0 = Ceil2Int(dImageY - filter.yWidth);
		int y1 = Floor2Int(dImageY + filter.yWidth);
		if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
			return;
		// Loop over filter support and add sample to pixel arrays
		int ifx[32];
		for (int x = x0; x <= x1; ++x) {
			float fx = fabsf((x - dImageX) *
					filter.invXWidth * FILTER_TABLE_SIZE);
			ifx[x - x0] = min(Floor2Int(fx), FILTER_TABLE_SIZE - 1);
		}

		int ify[32];
		for (int y = y0; y <= y1; ++y) {
			float fy = fabsf((y - dImageY) *
					filter.invYWidth * FILTER_TABLE_SIZE);
			ify[y - y0] = min(Floor2Int(fy), FILTER_TABLE_SIZE - 1);
		}
		float filterNorm = 0.f;
		for (int y = y0; y <= y1; ++y) {
			for (int x = x0; x <= x1; ++x) {
				const int offset = ify[y - y0] * FILTER_TABLE_SIZE + ifx[x - x0];
				filterNorm += filterTable[offset];
			}
		}
		filterNorm = 1.f / filterNorm;

		for (u_int y = static_cast<u_int>(max<int>(y0, 0)); y <= static_cast<u_int>(min<int>(y1, height - 1)); ++y) {
			for (u_int x = static_cast<u_int>(max<int>(x0, 0)); x <= static_cast<u_int>(min<int>(x1, width - 1)); ++x) {
				const int offset = ify[y - y0] * FILTER_TABLE_SIZE + ifx[x - x0];
				const float filterWt = filterTable[offset] * filterNorm;
				SplatRadiance(sampleElem->radiance, x, y, filterWt);
			}
		}
	}

	void UpdateScreenBufferImpl() {
		for (unsigned int i = 0, j = 0; i < pixelCount; ++i) {
			const float weight = pixelWeights[i];

			if (weight == 0.f) {
				pixels[j++] = 0.f;
				pixels[j++] = 0.f;
				pixels[j++] = 0.f;
			} else {
				const float invWeight = 1.f / weight;

				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].r * invWeight);
				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].g * invWeight);
				pixels[j++] = Radiance2PixelFloat(pixelsRadiance[i].b * invWeight);
			}
		}
	}

	boost::mutex radianceMutex;
	GaussianFilter filter2x2, filter4x4;
	float *filterTable2x2, *filterTable4x4;
	Spectrum *pixelsRadiance;
	float *pixelWeights;

	float *pixels;

	bool useLargeFilter;
};

class FastGaussianFilm : public GaussianFilm {
public:
	FastGaussianFilm(const bool lowLatencyMode, const unsigned int w, unsigned int h) :
		GaussianFilm(lowLatencyMode, w, h) {
	}

	~FastGaussianFilm() {
	}

	void SplatSampleBuffer(const SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (useLargeFilter) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				FastSplatSampleBufferElem(&sbe[i]);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter2x2, filterTable2x2);
		}

		Film::SplatSampleBuffer(sampleBuffer);
	}

private:
	void FastSplatSampleBufferElem(const SampleBufferElem *sampleElem) {
		useLargeFilter = (useLargeFilter && (sampleElem->pass < 32));

		const int splatSize = 4;

		// Compute sample's raster extent
		float dImageX = sampleElem->screenX - 0.5f;
		float dImageY = sampleElem->screenY - 0.5f;
		int x0 = Ceil2Int(dImageX - splatSize);
		int x1 = Floor2Int(dImageX + splatSize);
		int y0 = Ceil2Int(dImageY - splatSize);
		int y1 = Floor2Int(dImageY + splatSize);
		if (x1 < x0 || y1 < y0 || x1 < 0 || y1 < 0)
			return;

		for (u_int y = static_cast<u_int>(max<int>(y0, 0)); y <= static_cast<u_int>(min<int>(y1, height - 1)); ++y)
			for (u_int x = static_cast<u_int>(max<int>(x0, 0)); x <= static_cast<u_int>(min<int>(x1, width - 1)); ++x)
				SplatRadiance(sampleElem->radiance, x, y, 0.01f);
	}
};

#endif	/* _FILM_H */
