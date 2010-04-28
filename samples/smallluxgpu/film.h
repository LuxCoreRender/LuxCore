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

#if defined (WIN32)
#include <windows.h>
#endif

#include <FreeImage.h>

#include <boost/thread/mutex.hpp>

#include "smalllux.h"
#include "sampler.h"
#include "luxrays/core/pixel/samplebuffer.h"
#include "luxrays/core/pixeldevice.h"

#define GAMMA_TABLE_SIZE 1024

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

//------------------------------------------------------------------------------
// Base class for all Film implementation
//------------------------------------------------------------------------------

class Film {
public:
	Film(const bool lowLatencyMode, const unsigned int w, const unsigned int h) {
		lowLatency = lowLatencyMode;

		InitGammaTable();
		Init(w, h);
	}

	virtual ~Film() { }

	virtual void Init(const unsigned int w, const unsigned int h) {
		width = w;
		height = h;
		cerr << "Film size " << width << "x" << height << endl;
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

	void AddFilm(const string &filmFile) {
		ifstream file;
		file.open(filmFile.c_str(), ios::in | ios::binary);

		// Check if the file exists
		if (file.is_open()) {
			file.exceptions(ifstream::eofbit | ifstream::failbit | ifstream::badbit);

			cerr << "Loading film file: " << filmFile << endl;

			SampleFrameBuffer sbe(width, height);

			unsigned int tag;
			file.read((char *)&tag, sizeof(unsigned int));
			if (tag != (('S' << 24) | ('L' << 16) | ('G' << 8) | '0'))
				throw runtime_error("Unknown film file format");

			unsigned int w;
			file.read((char *)&w, sizeof(unsigned int));
			if (w != width)
				throw runtime_error("Film file width doesn't match internal width");

			unsigned int h;
			file.read((char *)&h, sizeof(unsigned int));
			if (h != height)
				throw runtime_error("Film file height doesn't match internal height");

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
			cerr << "Film file doesn't exist: " << filmFile << endl;
	}

	void SaveFilm(const string &filmFile) {
		cerr << "Saving film file: " << filmFile << endl;

		const SampleFrameBuffer *sbe = GetSampleFrameBuffer();

		ofstream file;
		file.exceptions(ifstream::eofbit | ifstream::failbit | ifstream::badbit);
		file.open(filmFile.c_str(), ios::out | ios::binary);

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
	virtual void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
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

	virtual void Save(const string &fileName) = 0;

protected:
	void SaveImpl(const string &fileName) {
		cerr << "Saving " << fileName << endl;

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
						cerr << "Failed image save: " << fileName << endl;

					FreeImage_Unload(dib);
				} else
					cerr << "Unable to allocate FreeImage HDR image: " << fileName << endl;
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
						cerr << "Failed image save: " << fileName << endl;

					FreeImage_Unload(dib);
				} else
					cerr << "Unable to allocate FreeImage image: " << fileName << endl;
			}
		} else
			cerr << "Image type unknown: " << fileName << endl;
	}

	float Radiance2PixelFloat(const float x) const {
		// Very slow !
		//return powf(Clamp(x, 0.f, 1.f), 1.f / 2.2f);

		const unsigned int index = min<unsigned int>(
			Floor2UInt(GAMMA_TABLE_SIZE * Clamp(x, 0.f, 1.f)),
				GAMMA_TABLE_SIZE - 1);
		return gammaTable[index];
	}

	Spectrum Radiance2Pixel(const Spectrum& c) const {
		return Spectrum(Radiance2PixelFloat(c.r), Radiance2PixelFloat(c.g), Radiance2PixelFloat(c.b));
	}

	virtual const SampleFrameBuffer *GetSampleFrameBuffer() = 0;
	virtual void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) = 0;

	unsigned int width, height;
	unsigned int pixelCount;

	unsigned int statsTotalSampleCount;
	double statsStartSampleTime, statsAvgSampleSec;

	float gammaTable[GAMMA_TABLE_SIZE];

	bool lowLatency;
};

//------------------------------------------------------------------------------
// Film implementations
//------------------------------------------------------------------------------

#define FILTER_TABLE_SIZE 16

class StandardFilm : public Film {
public:
	StandardFilm(const bool lowLatencyMode, const unsigned int w,
			const unsigned int h) : Film(lowLatencyMode, w, h) {
		sampleFrameBuffer = NULL;
		frameBuffer = NULL;
		freeSampleBuffers.resize(0);
		usedSampleBuffers.resize(0);

		Init(w, h);
	}

	virtual ~StandardFilm() {
		for (size_t i = 0; i < freeSampleBuffers.size(); ++i)
			delete freeSampleBuffers[i];
		for (size_t i = 0; i < usedSampleBuffers.size(); ++i)
			delete usedSampleBuffers[i];

		delete sampleFrameBuffer;
		delete frameBuffer;
	}

	virtual void Init(const unsigned int w, const unsigned int h) {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (sampleFrameBuffer)
			delete sampleFrameBuffer;
		if (frameBuffer)
			delete frameBuffer;

		pixelCount = w * h;

		sampleFrameBuffer = new SampleFrameBuffer(w, h);
		sampleFrameBuffer->Clear();
		frameBuffer = new FrameBuffer(w, h);
		frameBuffer->Clear();

		Film::Init(w, h);
	}

	virtual void Reset() {
		boost::mutex::scoped_lock lock(radianceMutex);

		sampleFrameBuffer->Clear();

		Film::Reset();
	}

	void UpdateScreenBuffer() {
		boost::mutex::scoped_lock lock(radianceMutex);

		UpdateScreenBufferImpl();
	}

	const float *GetScreenBuffer() const {
		return (const float *)frameBuffer->GetPixels();
	}

	SampleBuffer *GetFreeSampleBuffer() {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (freeSampleBuffers.size() > 0) {
			SampleBuffer *sb = freeSampleBuffers.back();
			freeSampleBuffers.pop_back();
			usedSampleBuffers.push_back(sb);
			sb->Reset();

			return sb;
		} else {
			SampleBuffer *sb = new SampleBuffer(4096);
			usedSampleBuffers.push_back(sb);

			return sb;
		}
	}

	void FreeSampleBuffer(SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		FreeSampleBufferImpl(sampleBuffer);
	}

	void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
			SplatSampleBufferElem(&sbe[i]);

		Film::SplatSampleBuffer(sampler, sampleBuffer);

		FreeSampleBufferImpl(sampleBuffer);
	}

	void Save(const string &fileName) {
		boost::mutex::scoped_lock lock(radianceMutex);

		// Update pixels
		UpdateScreenBufferImpl();

		SaveImpl(fileName);
	}

protected:
	void FreeSampleBufferImpl(SampleBuffer *sampleBuffer) {
		for (vector<SampleBuffer *>::iterator i = usedSampleBuffers.begin(); i < usedSampleBuffers.end(); ++i) {
			if (*i == sampleBuffer) {
				usedSampleBuffers.erase(i);
				freeSampleBuffers.push_back(sampleBuffer);
				return;
			}
		}

		throw std::runtime_error("Internal error: unable to find the SampleBuffer in StandardFilm::FreeSampleBufferImpl()");
	}

	void SplatSampleBufferElem(const SampleBufferElem *sampleElem) {
		sampleFrameBuffer->AddPixel((unsigned int)sampleElem->screenX, (unsigned int)sampleElem->screenY,
				sampleElem->radiance, 1.f);
	}

	virtual void UpdateScreenBufferImpl() {
		for (unsigned int i = 0; i < pixelCount; ++i) {
			const SamplePixel *sp = sampleFrameBuffer->GetPixel(i);
			const float weight = sp->weight;

			if (weight > 0.f)
				frameBuffer->SetPixel(i, Radiance2Pixel(sp->radiance / weight));
		}
	}

	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return sampleFrameBuffer;
	}

	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) {
		for (unsigned int i = 0; i < pixelCount; ++i) {
			const SamplePixel *sp = sfb->GetPixel(i);
			sampleFrameBuffer->AddPixel(i, sp->radiance, sp->weight);
		}
	}

	boost::mutex radianceMutex;
	SampleFrameBuffer *sampleFrameBuffer;
	FrameBuffer *frameBuffer;

	vector<SampleBuffer *> freeSampleBuffers;
	vector<SampleBuffer *> usedSampleBuffers;
};

class BluredStandardFilm : public StandardFilm {
public:
	BluredStandardFilm(const bool lowLatencyMode, const unsigned int w,
			const unsigned int h) : StandardFilm(lowLatencyMode, w, h) {
	}

	~BluredStandardFilm() {
	}

	void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (sampler->IsPreviewOver()) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i]);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				BluredSplatSampleBufferElem(&sbe[i]);
		}

		Film::SplatSampleBuffer(sampler, sampleBuffer);
		FreeSampleBufferImpl(sampleBuffer);
	}

private:
	void BluredSplatSampleBufferElem(const SampleBufferElem *sampleElem) {
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
				sampleFrameBuffer->AddPixel(x, y, 0.01f * sampleElem->radiance,  0.01f);
	}
};

class GaussianFilm : public StandardFilm {
public:
	GaussianFilm(const bool lowLatencyMode, const unsigned int w, const unsigned int h) :
		StandardFilm(lowLatencyMode, w, h),  filter2x2(2.f, 2.f, 2.f),  filter4x4(4.f, 4.f, 0.05f) {
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
	}

	virtual ~GaussianFilm() {
		delete[] filterTable2x2;
		delete[] filterTable4x4;
	}

	void Reset() {
		boost::mutex::scoped_lock lock(radianceMutex);

		if (lowLatency) {
			for (unsigned int i = 0; i < pixelCount; ++i) {
				const SamplePixel *sp = sampleFrameBuffer->GetPixel(i);
				const float weight = sp->weight;

				if (weight > 0.f)
					sampleFrameBuffer->SetPixel(i, sp->radiance / (100.0f * weight), 0.01f);
			}
		} else
			sampleFrameBuffer->Clear();

		Film::Reset();
	}

	virtual void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (sampler->IsPreviewOver()) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter2x2, filterTable2x2);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter4x4, filterTable4x4);
		}

		Film::SplatSampleBuffer(sampler, sampleBuffer);
		FreeSampleBufferImpl(sampleBuffer);
	}

protected:
	void SplatSampleBufferElem(const SampleBufferElem *sampleElem, const GaussianFilter &filter, const float *filterTable) {
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

				sampleFrameBuffer->AddPixel(x, y, sampleElem->radiance * filterWt, filterWt);
			}
		}
	}

	virtual void UpdateScreenBufferImpl() {
		for (unsigned int i = 0; i < pixelCount; ++i) {
			const SamplePixel *sp = sampleFrameBuffer->GetPixel(i);
			const float weight = sp->weight;

			if (weight == 0.f)
				frameBuffer->SetPixel(i, Spectrum());
			else
				frameBuffer->SetPixel(i, Radiance2Pixel(sp->radiance / weight));
		}
	}

	GaussianFilter filter2x2, filter4x4;
	float *filterTable2x2, *filterTable4x4;
};

class FastGaussianFilm : public GaussianFilm {
public:
	FastGaussianFilm(const bool lowLatencyMode, const unsigned int w,
			unsigned int h) : GaussianFilm(lowLatencyMode, w, h) {
	}

	~FastGaussianFilm() {
	}

	void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
		boost::mutex::scoped_lock lock(radianceMutex);

		const SampleBufferElem *sbe = sampleBuffer->GetSampleBuffer();
		if (sampler->IsPreviewOver()) {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				SplatSampleBufferElem(&sbe[i], filter2x2, filterTable2x2);
		} else {
			for (size_t i = 0; i < sampleBuffer->GetSampleCount(); ++i)
				FastSplatSampleBufferElem(&sbe[i]);
		}

		Film::SplatSampleBuffer(sampler, sampleBuffer);
		FreeSampleBufferImpl(sampleBuffer);
	}

private:
	void FastSplatSampleBufferElem(const SampleBufferElem *sampleElem) {
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
				sampleFrameBuffer->AddPixel(x, y, sampleElem->radiance * 0.01f, 0.01f);
	}
};

class LuxRaysFilm : public Film {
public:
	LuxRaysFilm(Context *context, const bool lowLatencyMode, const unsigned int w,
			const unsigned int h, FilterType filter = FILTER_GAUSSIAN) : Film(lowLatencyMode, w, h) {
		ctx = context;
		filterType = filter;

		vector<DeviceDescription *> descs = ctx->GetAvailableDeviceDescriptions();
		//DeviceDescription::Filter(DEVICE_TYPE_NATIVE_THREAD, descs);
		DeviceDescription::Filter(DEVICE_TYPE_OPENCL, descs);
		OpenCLDeviceDescription::Filter(OCL_DEVICE_TYPE_GPU, descs);
		descs.resize(1);
		pixelDevice = ctx->AddPixelDevices(descs)[0];
		pixelDevice->AllocateSampleBuffers(32);
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
		pixelDevice->UpdateFrameBuffer();
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

	void SplatSampleBuffer(const Sampler *sampler, SampleBuffer *sampleBuffer) {
		Film::SplatSampleBuffer(sampler, sampleBuffer);

		if (sampler->IsPreviewOver())
			pixelDevice->AddSampleBuffer(filterType, sampleBuffer);
		else
			pixelDevice->AddSampleBuffer(FILTER_PREVIEW, sampleBuffer);
	}

	void Save(const string &fileName) {
		// Update pixels
		pixelDevice->UpdateFrameBuffer();
		SaveImpl(fileName);
	}

protected:
	const SampleFrameBuffer *GetSampleFrameBuffer() {
		return pixelDevice->GetSampleFrameBuffer();
	}

	void AddSampleFrameBuffer(const SampleFrameBuffer *sfb) {
		pixelDevice->Merge(sfb);
	}

	Context *ctx;
	PixelDevice *pixelDevice;
	FilterType filterType;
};

#endif	/* _FILM_H */
