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

#ifndef _SLG_GAMMACORRECTION_PLUGIN_H
#define	_SLG_GAMMACORRECTION_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/core/color/color.h"
#include "luxrays/core/hardwaredevice.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

class Film;

//------------------------------------------------------------------------------
// Gamma correction plugin
//------------------------------------------------------------------------------

class GammaCorrectionPlugin : public ImagePipelinePlugin {
public:
	GammaCorrectionPlugin(const float gamma = 2.2f, const u_int tableSize = 16384);
	virtual ~GammaCorrectionPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(std::unordered_set<Film::FilmChannelType> &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	float gamma;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & gamma;
		ar & gammaTable;
	}

	float Radiance2PixelFloat(const float x) const;

	std::vector<float> gammaTable;

	// Used inside the object destructor to free buffers
	luxrays::HardwareDevice *hardwareDevice;
	luxrays::HardwareDeviceBuffer *hwGammaTable;

	luxrays::HardwareDeviceKernel *applyKernel;
};

}

BOOST_CLASS_VERSION(slg::GammaCorrectionPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::GammaCorrectionPlugin)

#endif	/*  _SLG_GAMMACORRECTION_PLUGIN_H */
