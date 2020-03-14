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

#ifndef _SLG_CAMERARESPONSE_PLUGIN_H
#define	_SLG_CAMERARESPONSE_PLUGIN_H

#include <vector>
#include <memory>
#include <typeinfo> 

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/devices/ocldevice.h"
#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"

namespace slg {

//------------------------------------------------------------------------------
// CameraResponse filter plugin
//------------------------------------------------------------------------------

class CameraResponsePlugin : public ImagePipelinePlugin {
public:
	CameraResponsePlugin(const std::string &name);
	virtual ~CameraResponsePlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

#if !defined(LUXRAYS_DISABLE_OPENCL)
	virtual bool CanUseOpenCL() const { return true; }
	virtual void ApplyOCL(Film &film, const u_int index);
#endif

	friend class boost::serialization::access;

private:
	// Used by Copy() and serialization
	CameraResponsePlugin();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & BOOST_SERIALIZATION_BASE_OBJECT_NVP(ImagePipelinePlugin);
		ar & redI;
		ar & redB;
		ar & greenI;
		ar & greenB;
		ar & blueI;
		ar & blueB;
		ar & color;
	}

	bool LoadPreset(const std::string &filmName);
	void LoadFile(const std::string &filmName);

	void Map(luxrays::RGBColor &rgb) const;
	float ApplyCrf(float point, const std::vector<float> &from, const std::vector<float> &to) const;

	std::vector<float> redI; // image irradiance (on the image plane)
	std::vector<float> redB; // measured intensity
	std::vector<float> greenI; // image irradiance (on the image plane)
	std::vector<float> greenB; // measured intensity
	std::vector<float> blueI; // image irradiance (on the image plane)
	std::vector<float> blueB; // measured intensity
	bool color;

#if !defined(LUXRAYS_DISABLE_OPENCL)
	// Used inside the object destructor to free buffers
	luxrays::OpenCLIntersectionDevice *oclIntersectionDevice;
	cl::Buffer *oclRedI;
	cl::Buffer *oclRedB;
	cl::Buffer *oclGreenI;
	cl::Buffer *oclGreenB;
	cl::Buffer *oclBlueI;
	cl::Buffer *oclBlueB;

	cl::Kernel *applyKernel;
#endif
};

}

BOOST_CLASS_VERSION(slg::CameraResponsePlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::CameraResponsePlugin)

#endif	/*  _SLG_CAMERARESPONSE_PLUGIN_H */
