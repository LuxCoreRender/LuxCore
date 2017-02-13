/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_IMAGEPIPELINE_H
#define	_SLG_IMAGEPIPELINE_H

#include <vector>
#include <memory>
#include <typeinfo> 
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/vector.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "luxrays/utils/ocl.h"

namespace slg {

class Film;
	
//------------------------------------------------------------------------------
// ImagePipelinePlugin
//------------------------------------------------------------------------------

class ImagePipelinePlugin {
public:
	ImagePipelinePlugin() { }
	virtual ~ImagePipelinePlugin() { }

	virtual bool CanUseOpenCL() const { return false; }
	virtual ImagePipelinePlugin *Copy() const = 0;

	virtual void Apply(Film &film, const u_int index) = 0;
	virtual void ApplyOCL(Film &film, const u_int index) {
		throw std::runtime_error("Internal error in ImagePipelinePlugin::ApplyOCL()");
	};

#if !defined(LUXRAYS_DISABLE_OPENCL)
	static cl::Program *CompileProgram(Film &film, const std::string &kernelsParameters,
		const std::string &kernelSource, const std::string &name);
#endif

	static float GetGammaCorrectionValue(const Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
	}
};

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

class ImagePipeline {
public:
	ImagePipeline();
	virtual ~ImagePipeline();

	bool CanUseOpenCL() const { return canUseOpenCL; }

	const std::vector<ImagePipelinePlugin *> &GetPlugins() const { return pipeline; }
	// An utility method
	const ImagePipelinePlugin *GetPlugin(const std::type_info &type) const;

	ImagePipeline *Copy() const;

	void AddPlugin(ImagePipelinePlugin *plugin);
	void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & pipeline;
	}

	std::vector<ImagePipelinePlugin *> pipeline;

	bool canUseOpenCL;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::ImagePipelinePlugin)

BOOST_CLASS_VERSION(slg::ImagePipeline, 1)
		
BOOST_CLASS_EXPORT_KEY(slg::ImagePipeline)

#endif	/*  _SLG_IMAGEPIPELINE_H */
