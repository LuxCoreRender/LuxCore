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

#ifndef _SLG_WHITE_BALANCE_H
#define	_SLG_WHITE_BALANCE_H

#include <boost/format.hpp>

#include "luxrays/core/color/color.h"
#include "luxrays/core/hardwaredevice.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"

//------------------------------------------------------------------------------
// White balance filter using Piccante library
//------------------------------------------------------------------------------

namespace slg {

class WhiteBalance : public ImagePipelinePlugin {
public:
	WhiteBalance();
	WhiteBalance(const float temperature);
	~WhiteBalance();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	virtual bool CanUseHW() const { return true; }
	virtual void AddHWChannelsUsed(Film::FilmChannels &hwChannelsUsed) const;
	virtual void ApplyHW(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	WhiteBalance(const luxrays::Spectrum &whitePoint);

	luxrays::Spectrum whitePoint;

	static luxrays::Spectrum TemperatureToWhitePoint(const float temperature);

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & whitePoint;
	}

	luxrays::HardwareDeviceKernel *applyKernel;
};

}

BOOST_CLASS_VERSION(slg::WhiteBalance, 1)

BOOST_CLASS_EXPORT_KEY(slg::WhiteBalance)

#endif