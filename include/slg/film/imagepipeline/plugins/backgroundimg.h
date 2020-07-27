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

#ifndef _SLG_BACKGROUND_IMG_PLUGIN_H
#define	_SLG_BACKGROUND_IMG_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/device.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/imagemap/imagemap.h"

namespace slg {

//------------------------------------------------------------------------------
// Background image plugin
//------------------------------------------------------------------------------

class BackgroundImgPlugin : public ImagePipelinePlugin {
public:
	BackgroundImgPlugin(ImageMap *map);
	virtual ~BackgroundImgPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(std::unordered_set<Film::FilmChannelType, std::hash<int> > &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	// Used by serialization
	BackgroundImgPlugin();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		// TODO: implement serialization for ImageMap
		throw std::runtime_error("BackgroundImgPlugin serialization not yet supported");
	}

	void UpdateFilmImageMap(const Film &film);

	ImageMap *imgMap;
	ImageMap *filmImageMap;

	// Used inside the object destructor to free buffers
	luxrays::HardwareDevice *hardwareDevice;
	luxrays::HardwareDeviceBuffer *hwFilmImageMapDesc;
	luxrays::HardwareDeviceBuffer *hwFilmImageMap;

	luxrays::HardwareDeviceKernel *applyKernel;
};

}

BOOST_CLASS_VERSION(slg::BackgroundImgPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::BackgroundImgPlugin)

#endif	/*  _SLG_BACKGROUND_IMG_PLUGIN_H */
