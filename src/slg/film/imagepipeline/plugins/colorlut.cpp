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

#include <stdexcept>
#include <boost/foreach.hpp>
#include <boost/regex.hpp>
#include <boost/lexical_cast.hpp>

#include "slg/film/film.h"
#include "slg/film/imagepipeline/plugins/colorlut.h"
#include "slg/utils/filenameresolver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// ColorLUT filter plugin
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::ColorLUTPlugin)

ColorLUTPlugin::ColorLUTPlugin(const string &filmName, const float str) {
	const string resolvedFileName = SLG_FileNameResolver.ResolveFile(filmName);

	lut = octoon::image::detail::basic_lut<float>::parse(resolvedFileName);
	strength = str;
}

ColorLUTPlugin::~ColorLUTPlugin() {
}

ColorLUTPlugin::ColorLUTPlugin() {
}

ImagePipelinePlugin *ColorLUTPlugin::Copy() const {
	ColorLUTPlugin *clp = new ColorLUTPlugin();

	const string dump = lut.dump();
	istringstream is(dump);
	clp->lut.create(is);
	
	return clp;
}

//------------------------------------------------------------------------------
// CPU version
//------------------------------------------------------------------------------

void ColorLUTPlugin::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	const u_int pixelCount = film.GetWidth() * film.GetHeight();

	const bool hasPN = film.HasChannel(Film::RADIANCE_PER_PIXEL_NORMALIZED);
	const bool hasSN = film.HasChannel(Film::RADIANCE_PER_SCREEN_NORMALIZED);

	#pragma omp parallel for
	for (
		// Visual C++ 2013 supports only OpenMP 2.5
#if _OPENMP >= 200805
		unsigned
#endif
		int i = 0; i < pixelCount; ++i) {
		if (film.HasSamples(hasPN, hasSN, i)) {
			Spectrum color = pixels[i].Clamp(0.f, 1.f);
			auto transformedColor = lut.lookup(color.c[0], color.c[1], color.c[2]);
			pixels[i].c[0] = Lerp(strength, color.c[0], transformedColor[0]);
			pixels[i].c[1] = Lerp(strength, color.c[1], transformedColor[1]);
			pixels[i].c[2] = Lerp(strength, color.c[2], transformedColor[2]);
		}
	}
}
