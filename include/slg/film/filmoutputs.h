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

#ifndef _SLG_FILMOUTPUTS_H
#define	_SLG_FILMOUTPUTS_H

#include <vector>
#include <set>

#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"
#include "luxrays/utils/serializationutils.h"

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
		GEOMETRY_NORMAL, SHADING_NORMAL, MATERIAL_ID,
		DIRECT_DIFFUSE, DIRECT_DIFFUSE_REFLECT, DIRECT_DIFFUSE_TRANSMIT,
		DIRECT_GLOSSY, DIRECT_GLOSSY_REFLECT, DIRECT_GLOSSY_TRANSMIT,
		EMISSION,
		INDIRECT_DIFFUSE, INDIRECT_DIFFUSE_REFLECT, INDIRECT_DIFFUSE_TRANSMIT,
		INDIRECT_GLOSSY, INDIRECT_GLOSSY_REFLECT, INDIRECT_GLOSSY_TRANSMIT,
		INDIRECT_SPECULAR, INDIRECT_SPECULAR_REFLECT, INDIRECT_SPECULAR_TRANSMIT,
		MATERIAL_ID_MASK, DIRECT_SHADOW_MASK, INDIRECT_SHADOW_MASK,
		RADIANCE_GROUP, UV, RAYCOUNT, BY_MATERIAL_ID, IRRADIANCE,
		OBJECT_ID, OBJECT_ID_MASK, BY_OBJECT_ID, SAMPLECOUNT,
		CONVERGENCE, SERIALIZED_FILM, MATERIAL_ID_COLOR, ALBEDO, AVG_SHADING_NORMAL,
		NOISE, USER_IMPORTANCE,
		FILMOUTPUT_TYPE_COUNT
	} FilmOutputType;

	FilmOutputs();
	~FilmOutputs();

	void Reset();
	u_int GetCount() const { return types.size(); }
	bool HasType(const FilmOutputType type) const { return (std::count(types.begin(), types.end(), type) > 0); }
	FilmOutputType GetType(const u_int index) const { return types[index]; }
	const std::string &GetFileName(const u_int index) const { return fileNames[index]; }
	const luxrays::Properties &GetProperties(const u_int index) const { return outputProps[index]; }

	void Add(const FilmOutputType type, const std::string &fileName,
		const luxrays::Properties *prop = NULL);

	bool UseSafeSave() const { return safeSave; }
	void SetSafeSave(const bool v) { safeSave = v; }
	
	static luxrays::Properties ToProperties(const luxrays::Properties &cfg);
	static FilmOutputType String2FilmOutputType(const std::string &type);
	static const std::string FilmOutputType2String(const FilmOutputType type);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & types;
		ar & fileNames;
		ar & outputProps;
		ar & safeSave;
	}

	std::vector<FilmOutputType> types;
	std::vector<std::string> fileNames;
	std::vector<luxrays::Properties> outputProps;

	bool safeSave;
};

}

BOOST_CLASS_VERSION(slg::FilmOutputs, 2)

#endif	/* _SLG_FILMOUTPUTS_H */
