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

#include "slg/film/imagepipeline/plugins/whitebalance.h"
#include "luxrays/core/color/spds/blackbodyspd.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// White balance filter
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::WhiteBalance)

WhiteBalance::WhiteBalance(): whitePoint(TemperatureToWhitePoint(6500.f)) {
}

WhiteBalance::WhiteBalance(float tmp): whitePoint(TemperatureToWhitePoint(tmp)) {
}

WhiteBalance::WhiteBalance(Spectrum wht_pt): whitePoint(wht_pt) {
}

ImagePipelinePlugin *WhiteBalance::Copy() const {
    return new WhiteBalance(whitePoint);
}

Spectrum WhiteBalance::TemperatureToWhitePoint(const float temperature) {

    // Same code as in the RadianceChannelScale class
    BlackbodySPD spd(temperature);
    XYZColor colorTemp = spd.ToXYZ();
    colorTemp /= colorTemp.Y();

    ColorSystem colorSpace;
    Spectrum scale = colorSpace.ToRGBConstrained(colorTemp).Clamp(0.f, 1.f);
    return scale;
}

void WhiteBalance::Apply(Film &film, const u_int index) {

    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const u_int width = film.GetWidth();
    const u_int height = film.GetHeight();

    const u_int pixelCount = width * height;
	#pragma omp parallel for
	for (
			// Visual C++ 2013 supports only OpenMP 2.5
    #if _OPENMP >= 200805
		unsigned
    #endif
        int i = 0; i < pixelCount; ++i) {
            pixels[i].c[0] *= whitePoint.c[0];
            pixels[i].c[1] *= whitePoint.c[1];
            pixels[i].c[2] *= whitePoint.c[2];
        }

}
