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

#ifndef _SLG_OPENCOLOR_TONEMAP_H
#define	_SLG_OPENCOLOR_TONEMAP_H

#include <cmath>
#include <string>

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/plugins/tonemaps/tonemap.h"

namespace slg {

//------------------------------------------------------------------------------
// OpenColorIO tone mapping
//------------------------------------------------------------------------------

class OpenColorIOToneMap : public ToneMap {
public:
	OpenColorIOToneMap();

	virtual ~OpenColorIOToneMap();

	virtual ToneMapType GetType() const { return TONEMAP_OPENCOLORIO; }

	virtual ToneMap *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	static OpenColorIOToneMap *CreateColorSpaceConversion(const std::string &configFileName,
			const std::string &inputColorSpace, const std::string &outputColorSpace);
	static OpenColorIOToneMap *CreateLUTConversion(const std::string &lutFileName);
	static OpenColorIOToneMap *CreateDisplayConversion(const std::string &configFileName,
			const std::string &inputColorSpace, const std::string &displayName,
			const std::string &viewName);
	static OpenColorIOToneMap *CreateLookConversion(const std::string &configFileName,
			const std::string &lookInputColorSpace, const std::string &lookName);

	friend class boost::serialization::access;

private:
	typedef enum {
		COLORSPACE_CONVERSION,
		LUT_CONVERSION,
		DISPLAY_CONVERSION,
		LOOK_CONVERSION
	} OCIOConversionType;

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ToneMap);
		ar & conversionType;
		ar & configFileName;
		ar & inputColorSpace;
		ar & outputColorSpace;
		ar & lutFileName;
		ar & displayName;
		ar & viewName;
	}
	
	OCIOConversionType conversionType;
	
	std::string configFileName;

	// COLORSPACE_CONVERSION
	std::string inputColorSpace;
	std::string outputColorSpace;

	// LUT_CONVERSION
	std::string lutFileName;
	
	// DISPLAY_CONVERSION
	std::string displayName;
	std::string viewName;
	
	// LOOK_CONVERSION
	std::string lookInputColorSpace;
	std::string lookName;
};

}

BOOST_CLASS_VERSION(slg::OpenColorIOToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::OpenColorIOToneMap)

#endif	/* _SLG_LINEAR_TONEMAP_H */
