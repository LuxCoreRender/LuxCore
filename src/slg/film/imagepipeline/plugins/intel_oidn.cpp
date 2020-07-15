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

#include <math.h>

#include <boost/format.hpp>

#include "slg/film/imagepipeline/plugins/intel_oidn.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
//Intel Open Image Denoise
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IntelOIDN)

IntelOIDN::IntelOIDN(const string ft, const int m, const float s) {
	filterType = ft;
	oidnMemLimit = m;
	sharpness = s;
}

IntelOIDN::IntelOIDN() {
	filterType = "RT";
	oidnMemLimit = 6000;
	sharpness = 0.f;
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
	return new IntelOIDN(filterType, oidnMemLimit, sharpness);
}

void IntelOIDN::Apply(Film &film, const u_int index) {
	const double totalStartTime = WallClockTime();
	SLG_LOG("[IntelOIDNPlugin] Applying single OIDN");
    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const u_int width = film.GetWidth();
    const u_int height = film.GetHeight();
    const u_int pixelCount = width * height;

    vector<float> outputBuffer(3 * pixelCount);
    vector<float> albedoBuffer;
    vector<float> normalBuffer;

    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter(filterType.c_str());

    filter.set("hdr", true);
	filter.set("maxMemoryMB", oidnMemLimit);
    filter.setImage("color", (float *)pixels, oidn::Format::Float3, width, height);
    if (film.HasChannel(Film::ALBEDO)) {
		albedoBuffer.resize(3 * pixelCount);
		for (u_int i = 0; i < pixelCount; ++i)
			film.channel_ALBEDO->GetWeightedPixel(i, &albedoBuffer[i * 3]);
		
        filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, width, height);

        // Normals can only be used if albedo is supplied as well
        if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
            normalBuffer.resize(3 * pixelCount);
            for (u_int i = 0; i < pixelCount; ++i)
                film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, &normalBuffer[i * 3]);

            filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, width, height);
        } else
            SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
    } else
		SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");
    
    filter.setImage("output", &outputBuffer[0], oidn::Format::Float3, width, height);
    filter.commit();
    
    SLG_LOG("IntelOIDNPlugin executing filter");
	const double startTime = WallClockTime();
    filter.execute();
	SLG_LOG("IntelOIDNPlugin apply took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

    const char *errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None)
         SLG_LOG("IntelOIDNPlugin error: " << errorMessage);

    SLG_LOG("IntelOIDNPlugin copying output buffer");
    for (u_int i = 0; i < pixelCount; ++i) {
        const u_int i3 = i * 3;
        pixels[i].c[0] = Lerp(sharpness, outputBuffer[i3], pixels[i].c[0]);
        pixels[i].c[1] = Lerp(sharpness, outputBuffer[i3 + 1], pixels[i].c[1]);
        pixels[i].c[2] = Lerp(sharpness, outputBuffer[i3 + 2], pixels[i].c[2]);
	}

	SLG_LOG("IntelOIDNPlugin single execution took a total of " << (boost::format("%.1f") % (WallClockTime() - totalStartTime)) << "secs");
}