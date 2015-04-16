/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_CAMERARESPONSE_PLUGIN_H
#define	_SLG_CAMERARESPONSE_PLUGIN_H

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
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

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

}

BOOST_CLASS_VERSION(slg::CameraResponsePlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::CameraResponsePlugin)

#endif	/*  _SLG_CAMERARESPONSE_PLUGIN_H */
