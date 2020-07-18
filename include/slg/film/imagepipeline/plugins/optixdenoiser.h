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

#ifndef _SLG_OPTIX_DENOISER_H
#define	_SLG_OPTIX_DENOISER_H

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <vector>

#include <boost/serialization/export.hpp>

#include <OpenImageDenoise/oidn.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/devices/cudadevice.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// Optix Denoiser
//------------------------------------------------------------------------------

class OptixDenoiserPlugin : public ImagePipelinePlugin {
public:
	OptixDenoiserPlugin(const float sharpness = 0.f);
	virtual ~OptixDenoiserPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual bool CanUseNative() const { return false; }
	virtual bool CanUseHW() const { return true; }
	virtual void Apply(Film &film, const u_int index) {
		throw std::runtime_error("Called OptixDenoiserPlugin::Apply()");
	}

	virtual void ApplyHW(Film &film, const u_int index);
	
	friend class boost::serialization::access;

//private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & sharpness;
	}

	float sharpness;

	// Used inside the object destructor to free buffers
	luxrays::CUDADevice *cudaDevice;
	OptixDenoiser denoiserHandle;
	luxrays::HardwareDeviceBuffer *denoiserStateBuff;
	luxrays::HardwareDeviceBuffer *denoiserScratchBuff;
	luxrays::HardwareDeviceBuffer *denoiserTmpBuff;
};

}

BOOST_CLASS_VERSION(slg::OptixDenoiserPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::OptixDenoiserPlugin)

#endif
	
#endif /* _SLG_OPTIX_DENOISER_H */