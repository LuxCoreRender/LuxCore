/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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

#ifndef _SLG_IMAGEPIPELINE_PLUGINS_H
#define	_SLG_IMAGEPIPELINE_PLUGINS_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "slg/film/film.h"

namespace slg {

class Film;

//------------------------------------------------------------------------------
// Gamma correction plugin
//------------------------------------------------------------------------------

class GammaCorrectionPlugin : public ImagePipelinePlugin {
public:
	GammaCorrectionPlugin(const float gamma, const u_int tableSize);
	virtual ~GammaCorrectionPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float gamma;

private:
	float Radiance2PixelFloat(const float x) const;

	std::vector<float> gammaTable;
};

//------------------------------------------------------------------------------
// Nop plugin
//------------------------------------------------------------------------------

class NopPlugin : public ImagePipelinePlugin {
public:
	NopPlugin() { }
	virtual ~NopPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;
};

//------------------------------------------------------------------------------
// OutputSwitcher plugin
//------------------------------------------------------------------------------

class OutputSwitcherPlugin : public ImagePipelinePlugin {
public:
	OutputSwitcherPlugin(const Film::FilmChannelType t, const u_int i) : type(t), index(i) { }
	virtual ~OutputSwitcherPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	Film::FilmChannelType type;
	const u_int index;
};

//------------------------------------------------------------------------------
// GaussianBlur filter plugin
//------------------------------------------------------------------------------

class GaussianBlur3x3FilterPlugin : public ImagePipelinePlugin {
public:
	GaussianBlur3x3FilterPlugin(const float w) : weight(w), tmpBuffer(NULL), tmpBufferSize(0) { }
	virtual ~GaussianBlur3x3FilterPlugin() { delete tmpBuffer; }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	const float weight;

private:
	void ApplyBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst,
		const float aF, const float bF, const float cF) const;
	void ApplyBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst,
		const float aF, const float bF, const float cF) const;
	void ApplyGaussianBlurFilterXR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst) const;
	void ApplyGaussianBlurFilterYR1(
		const u_int filmWidth, const u_int filmHeight,
		const luxrays::Spectrum *src, luxrays::Spectrum *dst) const;

	mutable luxrays::Spectrum *tmpBuffer;
	mutable size_t tmpBufferSize;
};

//------------------------------------------------------------------------------
// CameraResponse filter plugin
//------------------------------------------------------------------------------

class CameraResponsePlugin : public ImagePipelinePlugin {
public:
	CameraResponsePlugin(const std::string &name);
	virtual ~CameraResponsePlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

private:
	// Used by Copy()
	CameraResponsePlugin() { }

	bool LoadPreset(const std::string &filmName);
	void LoadFile(const std::string &filmName);

	void Map(luxrays::RGBColor &rgb) const;
	float ApplyCrf(float point, const vector<float> &from, const vector<float> &to) const;

	bool color;
	vector<float> RedI; // image irradiance (on the image plane)
	vector<float> RedB; // measured intensity
	vector<float> GreenI; // image irradiance (on the image plane)
	vector<float> GreenB; // measured intensity
	vector<float> BlueI; // image irradiance (on the image plane)
	vector<float> BlueB; // measured intensity
};

}

#endif	/*  _SLG_IMAGEPIPELINE_PLUGINS_H */
