/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_FILMOUTPUTS_H
#define	_SLG_FILMOUTPUTS_H

#include <vector>
#include <set>

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/set.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/utils/properties.h"

namespace slg {

//------------------------------------------------------------------------------
// FilmOutputs
//------------------------------------------------------------------------------

class FilmOutputs {
public:
	typedef enum {
		RGB, RGBA,
		// RGBA_TONEMAPPED is deprecated and replaced by RGB_IMAGEPIPELINE
		RGB_IMAGEPIPELINE,
		// RGBA_TONEMAPPED is deprecated and replaced by RGBA_IMAGEPIPELINE
		RGBA_IMAGEPIPELINE,
		ALPHA, DEPTH, POSITION,
		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID, DIRECT_DIFFUSE,
		DIRECT_GLOSSY, EMISSION, INDIRECT_DIFFUSE, INDIRECT_GLOSSY,
		INDIRECT_SPECULAR, MATERIAL_ID_MASK, DIRECT_SHADOW_MASK, INDIRECT_SHADOW_MASK,
		RADIANCE_GROUP, UV, RAYCOUNT, BY_MATERIAL_ID, IRRADIANCE,
		OBJECT_ID, OBJECT_ID_MASK, BY_OBJECT_ID, FRAMEBUFFER_MASK,
		FILMOUTPUT_TYPE_COUNT
	} FilmOutputType;

	FilmOutputs() { }
	~FilmOutputs() { }

	void Clear();
	u_int GetCount() const { return types.size(); }
	FilmOutputType GetType(const u_int index) const { return types[index]; }
	const std::string &GetFileName(const u_int index) const { return fileNames[index]; }
	const luxrays::Properties &GetProperties(const u_int index) const { return outputProps[index]; }

	void Add(const FilmOutputType type, const std::string &fileName,
		const luxrays::Properties *prop = NULL);

	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static FilmOutputType String2FilmOutputType(const std::string &type);
	static const std::string FilmOutputType2String(const FilmOutputType type);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & types;
		ar & fileNames;
		ar & outputProps;
	}

	std::vector<FilmOutputType> types;
	std::vector<std::string> fileNames;
	std::vector<luxrays::Properties> outputProps;
};

}

BOOST_CLASS_VERSION(slg::FilmOutputs, 1)

#endif	/* _SLG_FILMOUTPUTS_H */
