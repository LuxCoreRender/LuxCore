/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#include <boost/lexical_cast.hpp>
#include <boost/serialization/export.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/tonemaps/luxlinear.h"
#include "slg/film/imagepipeline/plugins/gammacorrection.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LuxRender Linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::LuxLinearToneMap)

void LuxLinearToneMap::Apply(const Film &film, Spectrum *pixels, std::vector<bool> &pixelsMask) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float gamma = 2.2f;
	const ImagePipeline *ip = film.GetImagePipeline();
	if (ip) {
		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}

	const float scale = exposure / (fstop * fstop) * sensitivity * 0.65f / 10.f * powf(118.f / 255.f, gamma);
	for (u_int i = 0; i < pixelCount; ++i) {
		if (pixelsMask[i])
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
	}
}
