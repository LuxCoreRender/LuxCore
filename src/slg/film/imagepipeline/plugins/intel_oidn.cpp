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

IntelOIDN::IntelOIDN(u_int n = 8, u_int o = 50, u_int t = 8294400) {
	nTiles = n;
	pixelOverlap = o;
	pixelThreshold = t;
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN();
}

void IntelOIDN::Apply(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const u_int pixelCount = width * height;
	//const u_int pixelThreshold = 1920 * 1080 * 4; //4K (UHD)

	if (pixelCount >= pixelThreshold) {
		ApplySplit(film, index);
	}
	else {
		ApplySingle(film, index);
	}
}

void IntelOIDN::ApplySplit(Film &film, const u_int index) {
	SLG_LOG("[IntelOIDNPlugin] Applying sliced OIDN!");
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const u_int pixelCount = width * height;

	//settings for tiled oidn:
	//const u_int nTiles = 8;
	//const u_int pixelOverlap = 50;
	const u_int pixelOverlap2 = 2 * pixelOverlap; // needed often in a loop later on

	vector<float> overlapBuffer(3 * pixelOverlap2 * width);
	vector<float> outputBuffer;// (3 * pixelCount);
	vector<float> albedoBuffer;
	vector<float> normalBuffer;

	//create Filter
	SLG_LOG("[IntelOIDNPlugin] Creating OIDN filter instance");
	oidn::DeviceRef device = oidn::newDevice();
	device.commit();
	oidn::FilterRef filter = device.newFilter("RT");
	filter.set("hdr", true);

	//denoise the stripes
	for (int iTile = 0; iTile < nTiles; ++iTile) {
		SLG_LOG("Stripe: " << iTile);
		//set stripe parameters
		u_int boundaryFront = height * iTile / nTiles;
		u_int boundaryBack = height * (iTile + 1) / nTiles;

		u_int overlapFront = iTile == 0 ? 0: pixelOverlap;
		u_int overlapBack = iTile == nTiles - 1 ? 0 : pixelOverlap;
		u_int overlapFrontPixels = 2 * overlapFront * width;

		u_int heightStripeStart = boundaryFront - overlapFront;
		u_int heightStripeStartNO = boundaryFront + overlapFront; //NO = Non-Overlap
		u_int heightStripeEndNO = boundaryBack - overlapBack;
		u_int heightStripeEnd = boundaryBack + overlapBack;

		u_int pixelStripeStart = width * heightStripeStart;
		u_int pixelStripeStartNO = width * heightStripeStartNO;
		u_int pixelStripeEndNO = width * heightStripeEndNO;
		u_int pixelStripeEnd = width * heightStripeEnd;

		u_int stripeHeight = heightStripeEnd - heightStripeStart;
		u_int pixelCountStripe = pixelStripeEnd - pixelStripeStart;
		u_int pixelCountNO = pixelStripeEndNO - pixelStripeStartNO;
		u_int beginEndOverlap = pixelCountNO + 2 * overlapFront * width;

		/* variables actually used in code below
		pixelOverlap2

		overlapFrontPixels

		pixelStripeStart
		pixelStripeStartNO

		stripeHeight
		pixelCountStripe
		pixelCountNO
		beginEndOverlap
		*/

		float *start = ((float *)pixels) + 3 * pixelStripeStart;

		filter.setImage("color", start, oidn::Format::Float3, width, stripeHeight);
		if (film.HasChannel(Film::ALBEDO)) {
			albedoBuffer.resize(3 * pixelCountStripe);
			for (u_int i = 0; i < pixelCountStripe; ++i)
				film.channel_ALBEDO->GetWeightedPixel(i + pixelStripeStart, &albedoBuffer[i * 3]);
			filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, width, stripeHeight);
			if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
				normalBuffer.resize(3 * pixelCountStripe);
				for (u_int i = 0; i < pixelCountStripe; ++i)
					film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(i + pixelStripeStart, &normalBuffer[i * 3]);
				filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, width, stripeHeight);
			}
			else
				SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
		}
		else
			SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");

		outputBuffer.resize(3 * pixelCountStripe);
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

		//overlap region with crossover for 2nd tile onwards
		if (iTile > 0) {
			float multi1 = 0.0;
			float multi2 = 0.0;
			for (u_int irow = 0; irow < pixelOverlap2; ++irow) {//need the row as a variable for the overlap function
				multi1 = (1.0 * pixelOverlap2 - irow) / (pixelOverlap2);
				multi2 = 1.0 * irow / (pixelOverlap2);
				for (u_int j = 0; j < width; ++j) {
					u_int i = (irow * width) + j;
					u_int i2 = i + pixelStripeStart;
					const u_int i3 = i * 3;
					pixels[i2].c[0] = overlapBuffer[i3] * multi1 + outputBuffer[i3] * multi2;
					pixels[i2].c[1] = overlapBuffer[i3 + 1] * multi1 + outputBuffer[i3 + 1] * multi2;
					pixels[i2].c[2] = overlapBuffer[i3 + 2] * multi1 + outputBuffer[i3 + 2] * multi2;
				}
			}
		}

		// fill non-overlap region
		for (u_int i = 0; i < pixelCountNO; ++i) {
			const u_int i3 = (i + overlapFrontPixels) * 3;
			pixels[i + pixelStripeStartNO].c[0] = outputBuffer[i3];
			pixels[i + pixelStripeStartNO].c[1] = outputBuffer[i3 + 1];
			pixels[i + pixelStripeStartNO].c[2] = outputBuffer[i3 + 2];
		}

		//set overlap buffer for all but last stripe
		if (iTile < nTiles - 1) {
			for (u_int i = 0; i < pixelOverlap2 * width; ++i) {
				const u_int i3 = i * 3;
				const u_int i32 = (i + beginEndOverlap) * 3;
				overlapBuffer[i3] = outputBuffer[i32];
				overlapBuffer[i3 + 1] = outputBuffer[i32 + 1];
				overlapBuffer[i3 + 2] = outputBuffer[i32 + 2];
			}
		}

	}

}

/**************************************************************************
**************************************************************************/

void IntelOIDN::ApplySingle(Film &film, const u_int index) {
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
