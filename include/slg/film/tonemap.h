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

#ifndef _SLG_TONEMAPPING_H
#define	_SLG_TONEMAPPING_H

#include <cmath>
#include <string>
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>

#include "luxrays/luxrays.h"
#include "slg/film/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Tone mapping
//------------------------------------------------------------------------------

typedef enum {
	TONEMAP_LINEAR, TONEMAP_REINHARD02, TONEMAP_AUTOLINEAR, TONEMAP_LUXLINEAR
} ToneMapType;

extern std::string ToneMapType2String(const ToneMapType type);
extern ToneMapType String2ToneMapType(const std::string &type);

class Film;

class ToneMap : public ImagePipelinePlugin {
public:
	ToneMap() {}
	virtual ~ToneMap() {}

	virtual ToneMapType GetType() const = 0;
	virtual ToneMap *Copy() const = 0;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const = 0;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ImagePipelinePlugin>(*this);
	}
};

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

class AutoLinearToneMap : public ToneMap {
public:
	AutoLinearToneMap() { }

	ToneMapType GetType() const { return TONEMAP_AUTOLINEAR; }

	ToneMap *Copy() const {
		return new AutoLinearToneMap();
	}

	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
	}
};

//------------------------------------------------------------------------------
// Linear tone mapping
//------------------------------------------------------------------------------

class LinearToneMap : public ToneMap {
public:
	LinearToneMap() {
		scale = 1.f;
	}

	LinearToneMap(const float s) {
		scale = s;
	}

	ToneMapType GetType() const { return TONEMAP_LINEAR; }

	ToneMap *Copy() const {
		return new LinearToneMap(scale);
	}

	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float scale;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
		ar & scale;
	}
};

//------------------------------------------------------------------------------
// LuxRender Linear tone mapping
//------------------------------------------------------------------------------

class LuxLinearToneMap : public ToneMap {
public:
	LuxLinearToneMap() {
		sensitivity = 100.f;
		exposure = 1.f / 1000.f;
		fstop = 2.8f;
	}

	LuxLinearToneMap(const float s, const float e, const float f) {
		sensitivity = s;
		exposure = e;
		fstop = f;
	}

	ToneMapType GetType() const { return TONEMAP_LUXLINEAR; }

	ToneMap *Copy() const {
		return new LuxLinearToneMap(sensitivity, exposure, fstop);
	}

	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float sensitivity, exposure, fstop;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
		ar & sensitivity;
		ar & exposure;
		ar & fstop;
	}
};

//------------------------------------------------------------------------------
// Reinhard02 tone mapping
//------------------------------------------------------------------------------

class Reinhard02ToneMap : public ToneMap {
public:
	Reinhard02ToneMap() {
		preScale = 1.f;
		postScale = 1.2f;
		burn = 3.75f;
	}
	Reinhard02ToneMap(const float preS, const float postS, const float b) {
		preScale = preS;
		postScale = postS;
		burn = b;
	}

	ToneMapType GetType() const { return TONEMAP_REINHARD02; }

	ToneMap *Copy() const {
		return new Reinhard02ToneMap(preScale, postScale, burn);
	}

	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

	float preScale, postScale, burn;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & boost::serialization::base_object<ToneMap>(*this);
		ar & preScale;
		ar & postScale;
		ar & burn;
	}
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::ToneMap)

BOOST_CLASS_VERSION(slg::ToneMap, 1)
BOOST_CLASS_VERSION(slg::AutoLinearToneMap, 1)
BOOST_CLASS_VERSION(slg::LinearToneMap, 1)
BOOST_CLASS_VERSION(slg::LuxLinearToneMap, 1)
BOOST_CLASS_VERSION(slg::Reinhard02ToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::AutoLinearToneMap)
BOOST_CLASS_EXPORT_KEY(slg::LinearToneMap)
BOOST_CLASS_EXPORT_KEY(slg::LuxLinearToneMap)
BOOST_CLASS_EXPORT_KEY(slg::Reinhard02ToneMap)

#endif	/* _SLG_TONEMAPPING_H */
