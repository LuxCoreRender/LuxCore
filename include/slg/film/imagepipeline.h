/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"

namespace slg {

class Film;
	
//------------------------------------------------------------------------------
// ImagePipelinePlugin
//------------------------------------------------------------------------------

class ImagePipelinePlugin {
public:
	ImagePipelinePlugin() { }
	virtual ~ImagePipelinePlugin() { }

	virtual ImagePipelinePlugin *Copy() const = 0;

	virtual void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const = 0;
};

//------------------------------------------------------------------------------
// ImagePipeline
//------------------------------------------------------------------------------

class ImagePipeline {
public:
	ImagePipeline() {}
	virtual ~ImagePipeline();

	const std::vector<ImagePipelinePlugin *> &GetPlugins() const { return pipeline; }
	// An utility method
	const ImagePipelinePlugin *GetPlugin(const std::type_info &type) const;

	ImagePipeline *Copy() const;

	void AddPlugin(ImagePipelinePlugin *plugin);
	void Apply(const Film &film, luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask) const;

private:
	std::vector<ImagePipelinePlugin *> pipeline;
};

}

#endif	/*  _SLG_IMAGEPIPELINE_H */
