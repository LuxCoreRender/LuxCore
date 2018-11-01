/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

// This a test/debug plugin to generate patterns, etc.

#include <stdexcept>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/patterns.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Patterns plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::PatternsPlugin)

PatternsPlugin::PatternsPlugin(const u_int t) : type(t) {
	if (t != 0)
		throw runtime_error("Unknown pattern type in PatternsPlugin: " + ToString(t));
}

ImagePipelinePlugin *PatternsPlugin::Copy() const {
	return new PatternsPlugin(type);
}

void PatternsPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	switch (type) {
		case 0: {
#pragma omp parallel for
			for (
				// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
					unsigned
#endif
					int y = 0; y < height; ++y) {
				for (u_int x = 0; x < width; ++x) {
					const u_int index = x + y *width;
					if (x % 6 < 3 || y % 6 < 3) {
						pixels[index].c[0] = 0.f;
						pixels[index].c[1] = 0.f;
						pixels[index].c[2] = 0.f;
					} else {
						pixels[index].c[0] = 1.f;
						pixels[index].c[1] = 1.f;
						pixels[index].c[2] = 1.f;						
					}
				}
			}
			break;
		}
		default:
			throw runtime_error("Unknown pattern type in PatternsPlugin: " + ToString(type));
	}
}
