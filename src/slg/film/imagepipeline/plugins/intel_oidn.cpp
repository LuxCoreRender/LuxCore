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

#include <OpenImageDenoise/oidn.hpp>
#include <oneapi/tbb.h>

#include "slg/film/imagepipeline/plugins/intel_oidn.h"
#include "slg/film/framebuffer.h"

using namespace std;
using namespace luxrays;
using namespace slg;
using namespace oneapi::tbb;


// Nota: We take advantage of one of oidn's build features (cmake option
// OIDN_API_NAMESPACE), which allows us to define a specific namespace for a
// build. This ensures clear isolation between oidn for Luxcore and oidn for
// Blender.

//------------------------------------------------------------------------------
//Intel Open Image Denoise
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IntelOIDN)

void errorCallback(void* userPtr, lux::oidn::Error error, const char* message) {
  throw std::runtime_error(message);
}

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
		const float *srcBuffer, float * dstBuffer,
		const float *albedoBuffer, const float *normalBuffer,
		const u_int width, const u_int height, const bool cleanAux) const {

    lux::oidn::DeviceRef device = lux::oidn::newDevice(lux::oidn::DeviceType::CPU);

    const char* errorMessage2;

    if (device.getError(errorMessage2) != lux::oidn::Error::None) {
      throw std::runtime_error(errorMessage2);
    }

    device.setErrorFunction(errorCallback);
    device.set("verbose", 3);
    device.commit();

    lux::oidn::FilterRef filter = device.newFilter(filterType.c_str());

    lux::oidn::BufferRef colorBuf = device.newBuffer((float*)srcBuffer, width * height * 3 * sizeof(float));
    lux::oidn::BufferRef albedoBuf = device.newBuffer((float*)albedoBuffer, width * height * 3 * sizeof(float));
    lux::oidn::BufferRef normalBuf = device.newBuffer((float*)normalBuffer, width * height * 3 * sizeof(float));
    lux::oidn::BufferRef dstBuf = device.newBuffer((float*)dstBuffer, width * height * 3 * sizeof(float));


    filter.set("hdr", true);
	filter.set("cleanAux", cleanAux);
	filter.set("maxMemoryMB", oidnMemLimit);
    filter.setImage("color", colorBuf, lux::oidn::Format::Float3, width, height);
    if (albedoBuffer) {
        filter.setImage("albedo", albedoBuf, lux::oidn::Format::Float3, width, height);

        // Normals can only be used if albedo is supplied as well
        if (normalBuffer)
            filter.setImage("normal", normalBuf, lux::oidn::Format::Float3, width, height);
    }
    
    filter.setImage("output", dstBuf, lux::oidn::Format::Float3, width, height);
    filter.commit();

    SLG_LOG("IntelOIDNPlugin executing " + imageName + " filter");
	const double startTime = WallClockTime();
    filter.execute();
	SLG_LOG("IntelOIDNPlugin " + imageName + " filter took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

    const char *errorMessage;
    if (device.getError(errorMessage) != lux::oidn::Error::None)
         SLG_LOG("IntelOIDNPlugin " + imageName + " filtering error: " << errorMessage);
}


IntelOIDN::float_buffer IntelOIDN::PrepareBuffer (
	const std::string& imageName,
	const GenericFrameBuffer<4, 1, float>& channel,
	const u_int width,
	const u_int height,
	bool enablePrefiltering
) const {
	IntelOIDN::float_buffer outBuffer(width * height * 3);
	IntelOIDN::float_buffer tmpBuffer(outBuffer.size());
	IntelOIDN::float_buffer dummy1(outBuffer.size());
	IntelOIDN::float_buffer dummy2(outBuffer.size());

	// Extract channel
	tbb::parallel_for(
		tbb::blocked_range<u_int>(0, width * height),
		[&](tbb::blocked_range<u_int>& r) {
			for (u_int i = r.begin(); i < r.end(); ++i)
				channel.GetWeightedPixel(i, &tmpBuffer[i * 3]);
		}
	);

	// Prefilter
	if (enablePrefiltering) {
		FilterImage(
			imageName,
			&tmpBuffer[0],
			&outBuffer[0],
			&dummy1[0],
			&dummy2[0],
			width, height,
			false
		);
	} else {
		return tmpBuffer;
	}

	return outBuffer;

}



void IntelOIDN::Apply(Film &film, const u_int index) {
	const double totalStartTime = WallClockTime();

	SLG_LOG("[IntelOIDNPlugin] Applying single OIDN");

    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const u_int width = film.GetWidth();
    const u_int height = film.GetHeight();
    const u_int pixelCount = width * height;

	float_buffer outputBuffer(3 * pixelCount);
	float_buffer albedoBuffer;
	float_buffer normalBuffer;

	// Prepare functions
	auto prepareAlbedo = [&]() {
		albedoBuffer = PrepareBuffer(
			"Albedo",
			*film.channel_ALBEDO,
			width,
			height,
			enablePrefiltering
		);
	};

	auto prepareNormal = [&]() {
		normalBuffer = PrepareBuffer(
			"Normal",
			*film.channel_AVG_SHADING_NORMAL,
			width,
			height,
			enablePrefiltering
		);
	};

	SLG_LOG("IntelOIDNPlugin preparing inputs");
    if (film.HasChannel(Film::ALBEDO)) {
        if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
			// Prepare both (and parallelize)
			tbb::parallel_invoke(prepareAlbedo, prepareNormal);
		} else {
			// Prepare only albedo
			SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
			prepareAlbedo();
			normalBuffer = float_buffer(3 * pixelCount);
		}
	} else {
		SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");
		albedoBuffer = float_buffer(3 * pixelCount);
		normalBuffer = float_buffer(3 * pixelCount);
	}


	SLG_LOG("IntelOIDNPlugin filtering image");
	FilterImage("Image Pipeline", (float *)pixels, &outputBuffer[0],
			(albedoBuffer.size() > 0) ? &albedoBuffer[0] : nullptr,
			(normalBuffer.size() > 0) ? &normalBuffer[0] : nullptr,
			width, height, enablePrefiltering);

    SLG_LOG("IntelOIDNPlugin copying output buffer");
	tbb::affinity_partitioner aff_p;
	tbb::parallel_for(
		tbb::blocked_range2d<size_t, size_t>(0, pixelCount, 0, 3),
		[&](const blocked_range2d<size_t, size_t>& r) {
			for (size_t i = r.rows().begin(); i < r.rows().end(); ++i) {
				for (size_t j = r.cols().begin(); j < r.cols().end(); ++j) {
					pixels[i].c[j] = std::lerp(
						outputBuffer[i * 3 + j],
						pixels[i].c[j],
						sharpness
					);
				}
			}
		},
		aff_p
	);

	SLG_LOG("IntelOIDNPlugin single execution took a total of " << (boost::format("%.3f") % (WallClockTime() - totalStartTime)) << "secs");
}

#endif
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
