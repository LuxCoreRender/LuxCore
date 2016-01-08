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
#include "slg/film/imagepipeline/plugins/tonemaps/reinhard02.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Reinhard02 tone mapping
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::Reinhard02ToneMap)

void Reinhard02ToneMap::Apply(const Film &film, Spectrum *pxls) const {
	RGBColor *rgbPixels = (RGBColor *)pxls;

	const float alpha = .1f;
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	float Ywa = 0.f;
	for (u_int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i)) && !rgbPixels[i].IsInf())
			Ywa += logf(max(rgbPixels[i].Y(), 1e-6f));
	}
	if (pixelCount > 0)
		Ywa = expf(Ywa / pixelCount);

	// Avoid division by zero
	if (Ywa == 0.f)
		Ywa = 1.f;

	const float invB2 = burn > 0.f ? 1.f / (burn * burn) : 1e5f;
	const float scale = alpha / Ywa;
	const float preS = scale / preScale;
	const float postS = scale * postScale;

	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
			unsigned
#endif
			int i = 0; i < pixelCount; ++i) {
		if (*(film.channel_FRAMEBUFFER_MASK->GetPixel(i))) {
			const float ys = rgbPixels[i].Y() * preS;
			// Note: I don't need to convert to XYZ and back because I'm only
			// scaling the value.
			rgbPixels[i] *= postS * (1.f + ys * invB2) / (1.f + ys);
		}
	}
}
