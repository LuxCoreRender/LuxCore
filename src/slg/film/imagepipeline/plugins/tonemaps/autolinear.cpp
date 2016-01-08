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
#include "slg/film/imagepipeline/plugins/gammacorrection.h"
#include "slg/film/imagepipeline/plugins/tonemaps/autolinear.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Auto-linear tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::AutoLinearToneMap)

float AutoLinearToneMap::CalcLinearToneMapScale(const Film &film, const float Y) {
	float gamma = 2.2f;
	const ImagePipeline *ip = film.GetImagePipeline();
	if (ip) {
		const GammaCorrectionPlugin *gc = (const GammaCorrectionPlugin *)ip->GetPlugin(typeid(GammaCorrectionPlugin));
		if (gc)
			gamma = gc->gamma;
	}
	
	// Substitute exposure, fstop and sensitivity cancel out; collect constants
	const float scale = (1.25f / Y * powf(118.f / 255.f, gamma));

	return scale;
}

void AutoLinearToneMap::Apply(const Film &film, Spectrum *pixels) const {
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float Y = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i))) {
			const float y = pixels[i].Y();
			if ((y <= 0.f) || isinf(y))
				continue;

			Y += y;
		}
	}
	Y /= pixelCount;

	if (Y <= 0.f)
		return;

	const float scale = CalcLinearToneMapScale(film, Y);

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i)))
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			pixels[i] = scale * pixels[i];
	}
}
