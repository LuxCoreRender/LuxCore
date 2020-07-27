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

#ifndef _SLG_AUTOLINEAR_TONEMAP_H
#define	_SLG_AUTOLINEAR_TONEMAP_H

#include <cmath>
#include <string>

#include "luxrays/core/hardwaredevice.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/plugins/tonemaps/tonemap.h"

namespace slg {

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

class AutoLinearToneMap : public ToneMap {
public:
	AutoLinearToneMap();
	virtual ~AutoLinearToneMap();

	virtual ToneMapType GetType() const { return TONEMAP_AUTOLINEAR; }

	virtual ToneMap *Copy() const {
		return new AutoLinearToneMap();
	}

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(std::unordered_set<Film::FilmChannelType, std::hash<int> > &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	static float CalcLinearToneMapScale(const Film &film, const u_int index, const float Y);
	
	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ToneMap);
	}

	// Used inside the object destructor to free buffers
	luxrays::HardwareDevice *hardwareDevice;
	luxrays::HardwareDeviceBuffer *hwAccumBuffer;

	luxrays::HardwareDeviceKernel *opRGBValuesReduceKernel;
	luxrays::HardwareDeviceKernel *opRGBValueAccumulateKernel;
	luxrays::HardwareDeviceKernel *applyKernel;
};

}

BOOST_CLASS_VERSION(slg::AutoLinearToneMap, 1)

BOOST_CLASS_EXPORT_KEY(slg::AutoLinearToneMap)

#endif	/* _SLG_AUTOLINEAR_TONEMAP_H */
