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

#include <boost/format.hpp>

#include "slg/film/imagepipeline/plugins/intel_oidn.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
//Intel Open Image Denoise
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IntelOIDN)

IntelOIDN::IntelOIDN() {
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN();
}

void IntelOIDN::Apply(Film &film, const u_int index) {
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const u_int pixelCount = width * height;

	vector<float> outputBuffer(3 * pixelCount);
	vector<float> albedoBuffer;
	vector<float> normalBuffer;

	oidn::DeviceRef device = oidn::newDevice();
	device.commit();
	oidn::FilterRef filter = device.newFilter("RT");
	filter.set("hdr", true);


	//settings for tiled oidn:
	const u_int nTiles = 4;
	const u_int pixelOverlap = 50;
	const u_int pixelOverlap2 = 2 * pixelOverlap; // needed often in a loop later on

	//denoise first stripe
	u_int stripeHeight = height / nTiles + pixelOverlap;
	u_int pixelCountStripe = width * stripeHeight;
	u_int cumulativeHeight = stripeHeight - pixelOverlap; //Defines the array start offset

	float *start = ((float *)pixels);
	filter.setImage("color", start, oidn::Format::Float3, width, stripeHeight);
	if (film.HasChannel(Film::ALBEDO)) {
		albedoBuffer.resize(3 * pixelCountStripe);
		for (u_int i = 0; i < pixelCountStripe; ++i)
			film.channel_ALBEDO->GetWeightedPixel(i, &albedoBuffer[i * 3]);

		filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, width, stripeHeight);

		// Normals can only be used if albedo is supplied as well
		if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
			normalBuffer.resize(3 * pixelCountStripe);
			for (u_int i = 0; i < pixelCountStripe; ++i)
				film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, &normalBuffer[i * 3]);

			filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, width, stripeHeight);
		}
		else
			SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
	}
	else
		SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");

	filter.setImage("output", &outputBuffer[0], oidn::Format::Float3, width, stripeHeight);
	filter.commit();

	SLG_LOG("IntelOIDNPlugin executing filter (stripe 1)");
	const double startTime = WallClockTime();
	filter.execute();
	SLG_LOG("IntelOIDNPlugin apply (stripe 1) took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

	const char *errorMessage;
	if (device.getError(errorMessage) != oidn::Error::None)
		SLG_LOG("IntelOIDNPlugin error (stripe 1): " << errorMessage);

	SLG_LOG("IntelOIDNPlugin copying output buffer (stripe 1)");
	for (u_int i = 0; i < pixelCountStripe; ++i) {
		const u_int i3 = i * 3;
		pixels[i].c[0] = outputBuffer[i3];
		pixels[i].c[1] = outputBuffer[i3 + 1];
		pixels[i].c[2] = outputBuffer[i3 + 2];
	}

	//denoise further stripes
	u_int overlap2;
	for (int iTile = 1; iTile < nTiles; ++iTile) {
		//compute height of current stripe. Overlap in both directions for middle stripes, only beginning for last stripe.
		iTile == nTiles - 1 ? overlap2 = 0 : overlap2 = pixelOverlap;
		stripeHeight = height * (iTile + 1) / nTiles - cumulativeHeight + pixelOverlap + overlap2;
		u_int pixOffset = width * (cumulativeHeight - pixelOverlap);
		pixelCountStripe = width * stripeHeight;

		float *start = ((float *)pixels) + 3 * pixOffset;

		filter.setImage("color", start, oidn::Format::Float3, width, stripeHeight);
		if (film.HasChannel(Film::ALBEDO)) {
			albedoBuffer.resize(3 * pixelCountStripe);
			for (u_int i = 0; i < pixelCountStripe; ++i)
				film.channel_ALBEDO->GetWeightedPixel(i + pixOffset, &albedoBuffer[i * 3]);

			filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, width, stripeHeight);

			// Normals can only be used if albedo is supplied as well
			if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
				normalBuffer.resize(3 * pixelCountStripe);
				for (u_int i = 0; i < pixelCountStripe; ++i)
					film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i + pixOffset, &normalBuffer[i * 3]);

				filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, width, stripeHeight);
			}
			else
				SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
		}
		else
			SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");

		filter.setImage("output", &outputBuffer[0], oidn::Format::Float3, width, stripeHeight);
		filter.commit();

		SLG_LOG("IntelOIDNPlugin executing filter (stripe " << iTile + 1 << ")");
		const double startTime = WallClockTime();
		filter.execute();
		SLG_LOG("IntelOIDNPlugin apply (stripe " << iTile + 1 << ") took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

		const char *errorMessage;
		if (device.getError(errorMessage) != oidn::Error::None)
			SLG_LOG("IntelOIDNPlugin error (stripe " << iTile + 1 << "): " << errorMessage);

		SLG_LOG("IntelOIDNPlugin copying output buffer (stripe " << iTile + 1 << ")");


		for (u_int irow = 0; irow < 2 * pixelOverlap; ++irow) {//need the row as a variable for the overlap function, not pixel as in loop before
			float multi1 = (pixelOverlap2 - irow) / (pixelOverlap2);
			float multi2 = irow / (pixelOverlap2);
			for (u_int j = 0; j < width; ++j) {
				u_int i = (irow * width) + j;
				const u_int i3 = i * 3;
				u_int i2 = i - (2 * pixelOverlap * width) + pixOffset;
				pixels[i2].c[0] = pixels[i2].c[0] * multi1 + outputBuffer[i3] * multi2;
				pixels[i2].c[1] = pixels[i2].c[1] * multi1 + outputBuffer[i3 + 1] * multi2;
				pixels[i2].c[2] = pixels[i2].c[2] * multi1 + outputBuffer[i3 + 2] * multi2;
			}
		}

		for (u_int i = 2 * pixelOverlap * width; i < pixelCountStripe; ++i) {
			const u_int i3 = i * 3;
			pixels[i + pixOffset].c[0] = outputBuffer[i3];
			pixels[i + pixOffset].c[1] = outputBuffer[i3 + 1];
			pixels[i + pixOffset].c[2] = outputBuffer[i3 + 2];
		}

		cumulativeHeight += stripeHeight - pixelOverlap - overlap2;

	}

}

/**************************************************************************
**************************************************************************/

void IntelOIDN::ApplySplit(Film &film, const u_int index) {
    Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

    const u_int width = film.GetWidth();
    const u_int height = film.GetHeight();
    const u_int pixelCount = width * height;

    vector<float> outputBuffer(3 * pixelCount);
    vector<float> albedoBuffer;
    vector<float> normalBuffer;

    oidn::DeviceRef device = oidn::newDevice();
    device.commit();

    oidn::FilterRef filter = device.newFilter("RT");

    filter.set("hdr", true);
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
        pixels[i].c[0] = outputBuffer[i3];
        pixels[i].c[1] = outputBuffer[i3 + 1];
        pixels[i].c[2] = outputBuffer[i3 + 2];
	}
}
