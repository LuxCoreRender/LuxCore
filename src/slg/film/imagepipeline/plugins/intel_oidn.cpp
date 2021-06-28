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

#if !defined(LUXCORE_DISABLE_OIDN)

#include <math.h>

#include <boost/format.hpp>

#include "slg/film/imagepipeline/plugins/intel_oidn.h"
#include "slg/film/framebuffer.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
//Intel Open Image Denoise
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IntelOIDN)

IntelOIDN::IntelOIDN(const string ft, const int m, const float s, const bool pref) {
	filterType = ft;
	oidnMemLimit = m;
	sharpness = s;
	enablePrefiltering = pref;
}

IntelOIDN::IntelOIDN() {
	filterType = "RT";
	oidnMemLimit = 6000;
	sharpness = 0.f;
	enablePrefiltering = true;
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
	return new IntelOIDN(filterType, oidnMemLimit, sharpness, enablePrefiltering);
}

void IntelOIDN::FilterImage(const string &imageName,
		const float *srcBuffer, float *dstBuffer,
		const float *albedoBuffer, const float *normalBuffer,
		const u_int width, const u_int height) const {
    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter(filterType.c_str());

    filter.set("hdr", true);
	filter.set("cleanAux", enablePrefiltering);
	filter.set("maxMemoryMB", oidnMemLimit);
    filter.setImage("color", (float *)srcBuffer, oidn::Format::Float3, width, height);
    if (albedoBuffer) {	
        filter.setImage("albedo", (float *)albedoBuffer, oidn::Format::Float3, width, height);

        // Normals can only be used if albedo is supplied as well
        if (normalBuffer)
            filter.setImage("normal", (float *)normalBuffer, oidn::Format::Float3, width, height);
    }
    
    filter.setImage("output", dstBuffer, oidn::Format::Float3, width, height);
    filter.commit();

    SLG_LOG("IntelOIDNPlugin executing " + imageName + " filter");
	const double startTime = WallClockTime();
    filter.execute();
	SLG_LOG("IntelOIDNPlugin " + imageName + " filter took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

    const char *errorMessage;
    if (device.getError(errorMessage) != oidn::Error::None)
         SLG_LOG("IntelOIDNPlugin " + imageName + " filtering error: " << errorMessage);
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

    if (film.HasChannel(Film::ALBEDO)) {
		albedoBuffer.resize(3 * pixelCount);
		for (u_int i = 0; i < pixelCount; ++i)
			film.channel_ALBEDO->GetWeightedPixel(i, &albedoBuffer[i * 3]);
		
		GenericFrameBuffer<3, 0, float>::SaveHDR("debug-albedo0.exr", albedoBuffer, width, height);

		if (enablePrefiltering) {
			vector<float> albedoBufferTmp(3 * pixelCount);
			FilterImage("Albedo", &albedoBuffer[0], &albedoBufferTmp[0],
				nullptr, nullptr, width, height);
			for (u_int i = 0; i < albedoBuffer.size(); ++i)
				albedoBuffer[i] = albedoBufferTmp[i];

			GenericFrameBuffer<3, 0, float>::SaveHDR("debug-albedo1.exr", albedoBuffer, width, height);
		}

        // Normals can only be used if albedo is supplied as well
        if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
            normalBuffer.resize(3 * pixelCount);
            for (u_int i = 0; i < pixelCount; ++i)
                film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, &normalBuffer[i * 3]);
			
			//GenericFrameBuffer<3, 0, float>::SaveHDR("debug-normal.exr", normalBuffer, width, height);
        } else
            SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
    } else
		SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");

	FilterImage("Image Pipeline", (float *)pixels, &outputBuffer[0],
			(albedoBuffer.size() > 0) ? &albedoBuffer[0] : nullptr,
			(normalBuffer.size() > 0) ? &normalBuffer[0] : nullptr,
			width, height);

    SLG_LOG("IntelOIDNPlugin copying output buffer");
    for (u_int i = 0; i < pixelCount; ++i) {
        const u_int i3 = i * 3;
        pixels[i].c[0] = Lerp(sharpness, outputBuffer[i3], pixels[i].c[0]);
        pixels[i].c[1] = Lerp(sharpness, outputBuffer[i3 + 1], pixels[i].c[1]);
        pixels[i].c[2] = Lerp(sharpness, outputBuffer[i3 + 2], pixels[i].c[2]);
	}

	SLG_LOG("IntelOIDNPlugin single execution took a total of " << (boost::format("%.3f") % (WallClockTime() - totalStartTime)) << "secs");
}

#endif
