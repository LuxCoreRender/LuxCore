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

#if !defined(LUXRAYS_DISABLE_CUDA)

#include <boost/format.hpp>

#include "luxrays/utils/cuda.h"

#include "slg/film/imagepipeline/plugins/optixdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Optix Denoiser
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::OptixDenoiserPlugin)

OptixDenoiserPlugin::OptixDenoiserPlugin(const float s) {
	sharpness = s;
}

OptixDenoiserPlugin::OptixDenoiserPlugin() {
	sharpness = 0.f;
}

ImagePipelinePlugin *OptixDenoiserPlugin::Copy() const {
	return new OptixDenoiserPlugin(sharpness);
}

void OptixDenoiserPlugin::Apply(Film &film, const u_int index) {
	const double totalStartTime = WallClockTime();
	SLG_LOG("[OptixDenoiserPlugin] Applying Optix denoiser");
    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const u_int width = film.GetWidth();
    const u_int height = film.GetHeight();
    const u_int pixelCount = width * height;

    vector<float> outputBuffer(3 * pixelCount);
//    vector<float> albedoBuffer;
//    vector<float> normalBuffer;

//    if (film.HasChannel(Film::ALBEDO)) {
//		albedoBuffer.resize(3 * pixelCount);
//		for (u_int i = 0; i < pixelCount; ++i)
//			film.channel_ALBEDO->GetWeightedPixel(i, &albedoBuffer[i * 3]);
//		
//        filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, width, height);
//
//        // Normals can only be used if albedo is supplied as well
//        if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
//            normalBuffer.resize(3 * pixelCount);
//            for (u_int i = 0; i < pixelCount; ++i)
//                film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, &normalBuffer[i * 3]);
//
//            filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, width, height);
//        } else
//            SLG_LOG("[OptixDenoiserPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
//    } else
//		SLG_LOG("[OptixDenoiserPlugin] Warning: ALBEDO AOV not found");
    
	const double startTime = WallClockTime();
	// TODO
	SLG_LOG("OptixDenoiserPlugin apply took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

    SLG_LOG("OptixDenoiserPlugin copying output buffer");
    for (u_int i = 0; i < pixelCount; ++i) {
        const u_int i3 = i * 3;
        pixels[i].c[0] = Lerp(sharpness, outputBuffer[i3], pixels[i].c[0]);
        pixels[i].c[1] = Lerp(sharpness, outputBuffer[i3 + 1], pixels[i].c[1]);
        pixels[i].c[2] = Lerp(sharpness, outputBuffer[i3 + 2], pixels[i].c[2]);
	}

	SLG_LOG("OptixDenoiserPlugin execution took a total of " << (boost::format("%.1f") % (WallClockTime() - totalStartTime)) << "secs");
}

#endif
