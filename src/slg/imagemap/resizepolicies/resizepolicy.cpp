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

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/format.hpp>

#include "slg/core/sdl.h"
#include "slg/imagemap/resizepolicies/resizepolicies.h"
#include "slg/textures/imagemaptex.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ImageMapResizePolicy
//------------------------------------------------------------------------------

ImageMapResizePolicy *ImageMapResizePolicy::FromProperties(const luxrays::Properties &props) {
	// For compatibility with the past
	if (!props.IsDefined("scene.images.resizepolicy.type") && props.IsDefined("images.scale")) {
		const float imageScale = Max(.01f, props.Get(Property("images.scale")(1.f)).Get<float>());
		return new ImageMapResizeFixedPolicy(imageScale, 128);
	}
	
	const ImageMapResizePolicyType type = String2ImageMapResizePolicyType(props.Get(Property("scene.images.resizepolicy.type")("NONE")).Get<string>());
	switch (type) {
		case POLICY_NONE:
			return new ImageMapResizeNonePolicy();
		case POLICY_FIXED: {
			const float scale = Max(.001f, props.Get(Property("scene.images.resizepolicy.scale")(1.f)).Get<float>());
			const u_int minSize = Max(2u, props.Get(Property("scene.images.resizepolicy.minsize")(64)).Get<u_int>());

			return new ImageMapResizeFixedPolicy(scale, minSize);
		}
		case POLICY_MINMEM: {
			const float scale = Max(.001f, props.Get(Property("scene.images.resizepolicy.scale")(1.f)).Get<float>());
			const u_int minSize = Max(2u, props.Get(Property("scene.images.resizepolicy.minsize")(64)).Get<u_int>());

			return new ImageMapResizeMinMemPolicy(scale, minSize);
		}
		case POLICY_MIPMAPMEM: {
			const float scale = Max(.001f, props.Get(Property("scene.images.resizepolicy.scale")(1.f)).Get<float>());
			const u_int minSize = Max(2u, props.Get(Property("scene.images.resizepolicy.minsize")(64)).Get<u_int>());

			return new ImageMapResizeMipMapMemPolicy(scale, minSize);
		}
		default:
			throw runtime_error("Unknown image map resize policy type in ImageMapResizePolicy::FromProperties(): " + ToString(type));
	}
}

ImageMapResizePolicyType ImageMapResizePolicy::String2ImageMapResizePolicyType(const string &type) {
	if (type == "NONE")
		return POLICY_NONE;
	else if (type == "FIXED")
		return POLICY_FIXED;
	else if (type == "MINMEM")
		return POLICY_MINMEM;
	else if (type == "MIPMAPMEM")
		return POLICY_MIPMAPMEM;	else
		throw runtime_error("Unknown image map resize policy type in ImageMapResizePolicy::String2ImageMapResizePolicyType(): " + type);
}

string ImageMapResizePolicy::ImageMapResizePolicyType2String(const ImageMapResizePolicyType type) {
	switch (type) {
		case POLICY_NONE:
			return "NONE";
		case POLICY_FIXED:
			return "FIXED";
		case POLICY_MINMEM:
			return "MINMEM";
		case POLICY_MIPMAPMEM:
			return "MIPMAPMEM";
		default:
			throw runtime_error("Unknown image map resize policy type in ImageMapResizePolicy::ImageMapResizePolicyType2String(): " + ToString(type));
	}
}
