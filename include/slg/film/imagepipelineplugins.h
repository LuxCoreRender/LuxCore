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
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

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
	GammaCorrectionPlugin(const float gamma = 2.2f, const u_int tableSize = 4096);
	virtual ~GammaCorrectionPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float gamma;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & gammaTable;
	}

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

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
	}
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
	u_int index;

	friend class boost::serialization::access;

private:
	// Used by serialization
	OutputSwitcherPlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & type;
		ar & index;
	}
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

	float weight;

	friend class boost::serialization::access;

private:
	// Used by serialization
	GaussianBlur3x3FilterPlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & weight;
	}

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

	friend class boost::serialization::access;

private:
	// Used by Copy() and serialization
	CameraResponsePlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & RedI;
		ar & RedB;
		ar & GreenI;
		ar & GreenB;
		ar & BlueI;
		ar & GreenI;
		ar & BlueB;
	}

	bool LoadPreset(const std::string &filmName);
	void LoadFile(const std::string &filmName);

	void Map(luxrays::RGBColor &rgb) const;
	float ApplyCrf(float point, const vector<float> &from, const vector<float> &to) const;

	vector<float> RedI; // image irradiance (on the image plane)
	vector<float> RedB; // measured intensity
	vector<float> GreenI; // image irradiance (on the image plane)
	vector<float> GreenB; // measured intensity
	vector<float> BlueI; // image irradiance (on the image plane)
	vector<float> BlueB; // measured intensity
	bool color;
};

//------------------------------------------------------------------------------
// Contour lines plugin
//------------------------------------------------------------------------------

class ContourLinesPlugin : public ImagePipelinePlugin {
public:
	ContourLinesPlugin(const float scale, const float range, const u_int steps,
			const int zeroGridSize);
	virtual ~ContourLinesPlugin() { }

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	friend class boost::serialization::access;

	float scale, range;
	u_int steps;
	int zeroGridSize;

private:
	// Used by Copy() and serialization
	ContourLinesPlugin() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
	}

	float GetLuminance(const Film &film, const u_int x, const u_int y) const;
	int GetStep(const Film &film, std::vector<bool> &pixelsMask,
			const int x, const int y, const int defaultValue,
			float *normalizedValue = NULL) const;
};

}

BOOST_CLASS_VERSION(slg::GammaCorrectionPlugin, 1)
BOOST_CLASS_VERSION(slg::NopPlugin, 1)
BOOST_CLASS_VERSION(slg::GaussianBlur3x3FilterPlugin, 1)
BOOST_CLASS_VERSION(slg::CameraResponsePlugin, 1)
BOOST_CLASS_VERSION(slg::ContourLinesPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::GammaCorrectionPlugin)
BOOST_CLASS_EXPORT_KEY(slg::NopPlugin)
BOOST_CLASS_EXPORT_KEY(slg::GaussianBlur3x3FilterPlugin)
BOOST_CLASS_EXPORT_KEY(slg::CameraResponsePlugin)
BOOST_CLASS_EXPORT_KEY(slg::ContourLinesPlugin)

#endif	/*  _SLG_IMAGEPIPELINE_PLUGINS_H */
