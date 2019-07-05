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
#include <math.h>

#include "slg/film/imagepipeline/plugins/intel_oidn.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
//Intel Open Image Denoise
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(slg::IntelOIDN)

IntelOIDN::IntelOIDN(const u_int n, const bool b) {
	nPixels = n;
	benchMode = b;
}

IntelOIDN::IntelOIDN() {
	nPixels = 1000;
	benchMode = false;
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN(nPixels, benchMode);
}

void IntelOIDN::Apply(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	bool splitCondition = false;
	if ((width > nPixels) || (height > nPixels)) {
		iTileCount = ceil(1.0 * height / nPixels);
		jTileCount = ceil(1.0 * width / nPixels);
		splitCondition = true;
	}

	if (benchMode) {
		ApplyTiled(film, index, iTileCount, jTileCount);
		ApplySingle(film, index);
	}
	else {
		if (splitCondition) {
			ApplyTiled(film, index, iTileCount, jTileCount);
		}
		else {
			ApplySingle(film, index);
		}
	}

}

void IntelOIDN::ApplyTiled(Film &film, const u_int index, const u_int iTileCount, const u_int jTileCount) {
	const double SuperStartTime = WallClockTime();
	SLG_LOG("[IntelOIDNPlugin] Applying tiled OIDN!");
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();
	
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const u_int pixelOverlap = 50;
	
	u_int bufInd = 0;
	u_int pixInd = 0;
	
	u_int rowPixelsCumu = 0; //index offset to account for where along a row of pixels the current tile starts
	
	float multi_i = 0.0;
	float multi_j = 0.0;

	vector<float> inputBuffer;
	vector<float> outputBuffer;
	vector<float> albedoBuffer;
	vector<float> normalBuffer;

	//copy pixels into pixelbuffer and zero pixels
	vector<float> pixelBuffer(3 * width * height);
	for (u_int i = 0; i < height; ++i) {
		for (u_int j = 0; j < width; ++j) {
			pixInd = i * width + j;
			pixelBuffer[3 * pixInd] = pixels[pixInd].c[0];
			pixelBuffer[3 * pixInd + 1] = pixels[pixInd].c[1];
			pixelBuffer[3 * pixInd + 2] = pixels[pixInd].c[2];
			pixels[pixInd].c[0] = 0;
			pixels[pixInd].c[1] = 0;
			pixels[pixInd].c[2] = 0;
		}
	}

	//create Filter
	SLG_LOG("[IntelOIDNPlugin] Creating OIDN filter instance");
	oidn::DeviceRef device = oidn::newDevice();
	device.commit();
	oidn::FilterRef filter = device.newFilter("RT");
	filter.set("hdr", true);

	//denoise the stripes
	//note that some paramenters are (only) defined inside the loop to be safe from rounding errors if users use odd resolutions
	//i.e. the tile width not being constant
	for (u_int iTile = 0; iTile < iTileCount; ++iTile) {
		//set row  parameters
		u_int boundaryFront = height * iTile / iTileCount;
		u_int boundaryBack = height * (iTile + 1) / iTileCount;

		u_int overlapBack = iTile == 0 ? 0 : pixelOverlap;
		u_int overlapFront = iTile == iTileCount - 1 ? 0 : pixelOverlap;
		u_int overlapBack2 = 2 * overlapBack;
		u_int overlapFront2 = 2 * overlapFront;
		u_int tileHeight = boundaryBack - boundaryFront + overlapFront + overlapBack;

		u_int tileWidthCumu = 0; //cumulative width of columns processed, needed for pixel array offsets.

		for (u_int jTile = 0; jTile < jTileCount; ++jTile) {
			SLG_LOG("Tile: " << jTileCount * iTile + jTile + 1);
			//set column parameters
			u_int boundaryLeft = width * jTile / jTileCount;
			u_int boundaryRight = width * (jTile + 1) / jTileCount;

			u_int overlapLeft = jTile == 0 ? 0 : pixelOverlap;
			u_int overlapRight = jTile == jTileCount - 1 ? 0 : pixelOverlap;
			u_int overlapLeft2 = 2 * overlapLeft;
			u_int overlapRight2 = 2 * overlapRight;
			u_int tileWidth = boundaryRight - boundaryLeft + overlapLeft + overlapRight;

			u_int pixelCountTile = tileHeight * tileWidth;

			//check if ALBEDO and AVG_Shading normal are present
			bool hasAlb = false;
			bool hasNorm = false;
			if (film.HasChannel(Film::ALBEDO)) {
				hasAlb = true;
				if (film.HasChannel(Film::AVG_SHADING_NORMAL)) {
					hasNorm = true;
				}
				else {
					SLG_LOG("[IntelOIDNPlugin] Warning: AVG_SHADING_NORMAL AOV not found");
				}
			}
			else {
				SLG_LOG("[IntelOIDNPlugin] Warning: ALBEDO AOV not found");
			}

			//resize buffers
			outputBuffer.resize(3 * pixelCountTile);
			inputBuffer.resize(3 * pixelCountTile);

			//fill input buffer(s)
			for (u_int i = 0; i < tileHeight; ++i) {
				for (u_int j = 0; j < tileWidth; ++j) {
					bufInd = 3 * (i * tileWidth + j);
					pixInd = 3 * (rowPixelsCumu + tileWidthCumu + i * width + j);
					inputBuffer[bufInd] = pixelBuffer[pixInd];
					inputBuffer[bufInd + 1] = pixelBuffer[pixInd + 1];
					inputBuffer[bufInd + 2] = pixelBuffer[pixInd + 2];
				}
			}
			filter.setImage("color", &inputBuffer[0], oidn::Format::Float3, tileWidth, tileHeight);

			//
			if (hasAlb) {
				albedoBuffer.resize(3 * pixelCountTile);
				for (u_int i = 0; i < tileHeight; ++i) {
					for (u_int j = 0; j < tileWidth; ++j) {
						bufInd = 3 * (i * tileWidth + j);
						pixInd = rowPixelsCumu + tileWidthCumu + i * width + j;
						film.channel_ALBEDO->GetWeightedPixel(pixInd, &albedoBuffer[bufInd]);
					}
				}
				filter.setImage("albedo", &albedoBuffer[0], oidn::Format::Float3, tileWidth, tileHeight);
			}

			if (hasNorm) {
				normalBuffer.resize(3 * pixelCountTile);
				for (u_int i = 0; i < tileHeight; ++i) {
					for (u_int j = 0; j < tileWidth; ++j) {
						bufInd = 3 * (i * tileWidth + j);
						pixInd = rowPixelsCumu + tileWidthCumu + i * width + j;
						film.channel_AVG_SHADING_NORMAL->GetWeightedPixel(pixInd, &normalBuffer[bufInd]);
					}
				}
				filter.setImage("normal", &normalBuffer[0], oidn::Format::Float3, tileWidth, tileHeight);
			}

			filter.setImage("output", &outputBuffer[0], oidn::Format::Float3, tileWidth, tileHeight);
			filter.commit();

			SLG_LOG("IntelOIDNPlugin executing filter (stripe " << jTileCount * iTile + jTile + 1 << ")");
			const double startTime = WallClockTime();
			filter.execute();
			SLG_LOG("IntelOIDNPlugin apply (stripe " << jTileCount * iTile + jTile + 1 << ") took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

			const char *errorMessage;
			if (device.getError(errorMessage) != oidn::Error::None)
				SLG_LOG("IntelOIDNPlugin error (stripe " << jTileCount * iTile + jTile + 1 << "): " << errorMessage);

			SLG_LOG("IntelOIDNPlugin copying output buffer (stripe " << jTileCount * iTile + jTile + 1 << ")");

			//write back to pixel array
			//multipliers applied for overlap region
			for (u_int i = 0; i < tileHeight; ++i) {
				multi_i = fmin(fmin(1.0, 1.0 * i / (overlapBack2)), fmin(1.0, (1.0 * tileHeight - i) / (overlapFront2)));
				for (u_int j = 0; j < tileWidth; ++j) {
					multi_j = fmin(fmin(1.0, 1.0 * j / (overlapLeft2)), fmin(1.0, (1.0 * tileWidth - j) / (overlapRight2)));
					bufInd = 3 * (i * tileWidth + j);
					pixInd = i * width + tileWidthCumu + rowPixelsCumu + j;
					pixels[pixInd].c[0] += outputBuffer[bufInd] * multi_i * multi_j;
					pixels[pixInd].c[1] += outputBuffer[bufInd + 1] * multi_i * multi_j;
					pixels[pixInd].c[2] += outputBuffer[bufInd + 2] * multi_i * multi_j;
				}
			}

			tileWidthCumu += tileWidth - overlapRight2;

		}

		rowPixelsCumu += (tileHeight - overlapFront2) * width;

	}

	SLG_LOG("IntelOIDNPlugin tiled execution took a total of " << (boost::format("%.1f") % (WallClockTime() - SuperStartTime)) << "secs");
}

void IntelOIDN::ApplySingle(Film &film, const u_int index) {
	const double SuperStartTime = WallClockTime();
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

	SLG_LOG("IntelOIDNPlugin single execution took a total of " << (boost::format("%.1f") % (WallClockTime() - SuperStartTime)) << "secs");
}