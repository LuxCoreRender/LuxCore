/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

#ifndef _SLG_COLORLUT_PLUGIN_H
#define	_SLG_COLORLUT_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include <lut.hpp>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// CameraResponse filter plugin
//------------------------------------------------------------------------------

class ColorLUTPlugin : public ImagePipelinePlugin {
public:
	ColorLUTPlugin(const std::string &name, const float strength);
	virtual ~ColorLUTPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	// Used by Copy() and serialization
	ColorLUTPlugin();

	template<class Archive> void save(Archive &ar, const unsigned int version) const {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);

		ar & lut.dump();
		ar & strength;
	}

	template<class Archive>	void load(Archive &ar, const unsigned int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);

		std::string lutStr;
		ar & lutStr;
		std::istringstream is(lutStr);
		lut.create(is);
		
		ar & strength;
	}
	BOOST_SERIALIZATION_SPLIT_MEMBER()
	
	octoon::image::detail::basic_lut<float> lut;
	float strength;
};

}

BOOST_CLASS_VERSION(slg::ColorLUTPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::ColorLUTPlugin)

#endif	/*  _SLG_COLORLUT_PLUGIN_H */
