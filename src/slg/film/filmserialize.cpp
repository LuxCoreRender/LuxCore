/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Film)

Film *Film::LoadSerialized(const std::string &fileName) {
	SerializationInputFile sif(fileName);

	Film *film;
	sif.GetArchive() >> film;

	if (!sif.IsGood())
		throw runtime_error("Error while loading serialized film: " + fileName);

	return film;
}

void Film::SaveSerialized(const std::string &fileName, const Film *film) {
	SerializationOutputFile sof(fileName);

	sof.GetArchive() << film;

	if (!sof.IsGood())
		throw runtime_error("Error while saving serialized film: " + fileName);

	sof.Flush();
	SLG_LOG("Film saved: " << (sof.GetPosition() / 1024) << " Kbytes");
}

template<class Archive> void Film::load(Archive &ar, const u_int version) {
	ar & channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	ar & channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	ar & channel_ALPHA;
	ar & channel_IMAGEPIPELINEs;
	ar & channel_DEPTH;
	ar & channel_POSITION;
	ar & channel_GEOMETRY_NORMAL;
	ar & channel_SHADING_NORMAL;
	ar & channel_MATERIAL_ID;
	ar & channel_DIRECT_DIFFUSE;
	ar & channel_DIRECT_GLOSSY;
	ar & channel_EMISSION;
	ar & channel_INDIRECT_DIFFUSE;
	ar & channel_INDIRECT_GLOSSY;
	ar & channel_INDIRECT_SPECULAR;
	ar & channel_MATERIAL_ID_MASKs;
	ar & channel_DIRECT_SHADOW_MASK;
	ar & channel_INDIRECT_SHADOW_MASK;
	ar & channel_UV;
	ar & channel_RAYCOUNT;
	ar & channel_BY_MATERIAL_IDs;
	ar & channel_IRRADIANCE;
	ar & channel_OBJECT_ID;
	ar & channel_OBJECT_ID_MASKs;
	ar & channel_BY_OBJECT_IDs;
	ar & channel_FRAMEBUFFER_MASK;
	ar & channel_SAMPLECOUNT;
	ar & channel_CONVERGENCE;

	ar & channels;
	ar & width;
	ar & height;
	ar & subRegion[0];
	ar & subRegion[1];
	ar & subRegion[2];
	ar & subRegion[3];
	ar & pixelCount;
	ar & radianceGroupCount;
	ar & maskMaterialIDs;
	ar & byMaterialIDs;

	ar & statsTotalSampleCount;
	ar & statsStartSampleTime;

	ar & imagePipelines;

	ar & convTest;
	ar & haltTime;
	ar & haltSPP;

	ar & radianceChannelScales;
	ar & filmOutputs;

	ar & initialized;
	ar & enabledOverlappedScreenBufferUpdate;

	SetUpOCL();
}

template<class Archive> void Film::save(Archive &ar, const u_int version) const {
	ar & channel_RADIANCE_PER_PIXEL_NORMALIZEDs;
	ar & channel_RADIANCE_PER_SCREEN_NORMALIZEDs;
	ar & channel_ALPHA;
	ar & channel_IMAGEPIPELINEs;
	ar & channel_DEPTH;
	ar & channel_POSITION;
	ar & channel_GEOMETRY_NORMAL;
	ar & channel_SHADING_NORMAL;
	ar & channel_MATERIAL_ID;
	ar & channel_DIRECT_DIFFUSE;
	ar & channel_DIRECT_GLOSSY;
	ar & channel_EMISSION;
	ar & channel_INDIRECT_DIFFUSE;
	ar & channel_INDIRECT_GLOSSY;
	ar & channel_INDIRECT_SPECULAR;
	ar & channel_MATERIAL_ID_MASKs;
	ar & channel_DIRECT_SHADOW_MASK;
	ar & channel_INDIRECT_SHADOW_MASK;
	ar & channel_UV;
	ar & channel_RAYCOUNT;
	ar & channel_BY_MATERIAL_IDs;
	ar & channel_IRRADIANCE;
	ar & channel_OBJECT_ID;
	ar & channel_OBJECT_ID_MASKs;
	ar & channel_BY_OBJECT_IDs;
	ar & channel_FRAMEBUFFER_MASK;
	ar & channel_SAMPLECOUNT;
	ar & channel_CONVERGENCE;

	ar & channels;
	ar & width;
	ar & height;
	ar & subRegion[0];
	ar & subRegion[1];
	ar & subRegion[2];
	ar & subRegion[3];
	ar & pixelCount;
	ar & radianceGroupCount;
	ar & maskMaterialIDs;
	ar & byMaterialIDs;

	ar & statsTotalSampleCount;
	ar & statsStartSampleTime;

	ar & imagePipelines;

	ar & convTest;
	ar & haltTime;
	ar & haltSPP;

	ar & radianceChannelScales;
	ar & filmOutputs;

	ar & initialized;
	ar & enabledOverlappedScreenBufferUpdate;
}

namespace slg {
// Explicit instantiations for portable archives
template void Film::save(LuxOutputArchive &ar, const u_int version) const;
template void Film::load(LuxInputArchive &ar, const u_int version);
// The following 2 lines shouldn't be required but they are with GCC 5
template void Film::save(boost::archive::polymorphic_oarchive &ar, const u_int version) const;
template void Film::load(boost::archive::polymorphic_iarchive &ar, const u_int version);
}

//------------------------------------------------------------------------------
// Film serialization
//------------------------------------------------------------------------------

template<class Archive> void Film::RadianceChannelScale::serialize(Archive &ar, const u_int version) {
	ar & globalScale;
	ar & temperature;
	ar & rgbScale;
	ar & enabled;

	Init();
}

namespace slg {
// Explicit instantiations for portable archives
template void Film::RadianceChannelScale::serialize(LuxOutputArchive &ar, const u_int version);
template void Film::RadianceChannelScale::serialize(LuxInputArchive &ar, const u_int version);
}
