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

#include <memory>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film serialization
//------------------------------------------------------------------------------

Film *Film::LoadSerialized(const std::string &fileName) {
	ifstream inFile;
	inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);

	inFile.open(fileName.c_str());

	boost::archive::binary_iarchive inArchive(inFile);

	Film *film;
	inArchive >> film;

	return film;
}

void Film::SaveSerialized(const std::string &fileName, const Film &film) {
	// Serialize the film
	ofstream outFile;
	outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);

	outFile.open(fileName.c_str());

	boost::archive::binary_oarchive outArchive(outFile);
	outArchive << film;
}

template<> void Film::load<boost::archive::binary_iarchive>(boost::archive::binary_iarchive &ar,
		const u_int version) {
	ar >> channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	ar >> channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	ar >> channel_ALPHA;
	ar >> channel_RGB_TONEMAPPED;
	ar >> channel_DEPTH;
	ar >> channel_POSITION;
	ar >> channel_GEOMETRY_NORMAL;
	ar >> channel_SHADING_NORMAL;
	ar >> channel_MATERIAL_ID;
	ar >> channel_DIRECT_DIFFUSE;
	ar >> channel_DIRECT_GLOSSY;
	ar >> channel_EMISSION;
	ar >> channel_INDIRECT_DIFFUSE;
	ar >> channel_INDIRECT_GLOSSY;
	ar >> channel_INDIRECT_SPECULAR;
	ar >> channel_MATERIAL_ID_MASKs;
	ar >> channel_DIRECT_SHADOW_MASK;
	ar >> channel_INDIRECT_SHADOW_MASK;
	ar >> channel_UV;
	ar >> channel_RAYCOUNT;
	ar >> channel_BY_MATERIAL_IDs;
	ar >> channel_IRRADIANCE;

	ar >> channels;
	ar >> width;
	ar >> height;
	ar >> pixelCount;
	ar >> radianceGroupCount;
	ar >> maskMaterialIDs;
	ar >> byMaterialIDs;

	ar >> statsTotalSampleCount;
	ar >> statsStartSampleTime;
	ar >> statsAvgSampleSec;

	ar >> imagePipeline;
	ar >> convTest;

	ar >> radianceChannelScales;

	ar >> initialized;
	ar >> enabledOverlappedScreenBufferUpdate;
	ar >> rgbTonemapUpdate;
}

template<> void Film::save<boost::archive::binary_oarchive>(boost::archive::binary_oarchive &ar,
		const u_int version) const {
	ar << channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	ar << channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	ar << channel_ALPHA;
	ar << channel_RGB_TONEMAPPED;
	ar << channel_DEPTH;
	ar << channel_POSITION;
	ar << channel_GEOMETRY_NORMAL;
	ar << channel_SHADING_NORMAL;
	ar << channel_MATERIAL_ID;
	ar << channel_DIRECT_DIFFUSE;
	ar << channel_DIRECT_GLOSSY;
	ar << channel_EMISSION;
	ar << channel_INDIRECT_DIFFUSE;
	ar << channel_INDIRECT_GLOSSY;
	ar << channel_INDIRECT_SPECULAR;
	ar << channel_MATERIAL_ID_MASKs;
	ar << channel_DIRECT_SHADOW_MASK;
	ar << channel_INDIRECT_SHADOW_MASK;
	ar << channel_UV;
	ar << channel_RAYCOUNT;
	ar << channel_BY_MATERIAL_IDs;
	ar << channel_IRRADIANCE;

	ar << channels;
	ar << width;
	ar << height;
	ar << pixelCount;
	ar << radianceGroupCount;
	ar << maskMaterialIDs;
	ar << byMaterialIDs;

	ar << statsTotalSampleCount;
	ar << statsStartSampleTime;
	ar << statsAvgSampleSec;

	ar << imagePipeline;
	ar << convTest;
	
	ar << radianceChannelScales;

	ar << initialized;
	ar << enabledOverlappedScreenBufferUpdate;
	ar << rgbTonemapUpdate;
}
