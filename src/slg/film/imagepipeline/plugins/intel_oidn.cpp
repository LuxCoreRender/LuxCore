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

IntelOIDN::IntelOIDN(const u_int n, const u_int o, const u_int t, const bool b) {
	nTiles = n;
	pixelOverlap = o;
	pixelThreshold = t;
	benchMode = b;
}

IntelOIDN::IntelOIDN() {
	nTiles = 4;
	pixelOverlap = 48;
	pixelThreshold = 8294400;
	benchMode = false;
}

ImagePipelinePlugin *IntelOIDN::Copy() const {
    return new IntelOIDN(nTiles, pixelOverlap, pixelThreshold, benchMode);
}

void IntelOIDN::Apply(Film &film, const u_int index) {
	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();
	const u_int pixelCount = width * height;
	if (benchMode) {
		ApplyTiled(film, index);
		ApplySingle(film, index);
	}
	else {
		if (pixelCount >= pixelThreshold) {
			ApplyTiled(film, index);
		}
		else {
			ApplySingle(film, index);
		}
	}
	
}

void IntelOIDN::ApplyTiled(Film &film, const u_int index) {
	const double SuperStartTime = WallClockTime();
	SLG_LOG("[IntelOIDNPlugin] Applying tiled OIDN!");
	Spectrum *pixels = (Spectrum *)film.channel_IMAGEPIPELINEs[index]->GetPixels();

	const u_int width = film.GetWidth();
	const u_int height = film.GetHeight();

	u_int bufInd = 0;
	u_int outInd = 0;
	u_int pixInd = 0;

	u_int rowPixelsCumu = 0; //index offset to account for where along a row of pixels the current tile starts

	float multi1 = 0.0;
	float multi2 = 0.0;

	const u_int overlap2 = 2 * pixelOverlap; // needed often later on

	vector<float> overlapBufferTile;
	vector<float> overlapBufferTop;
	vector<float> overlapBufferTop2;
	vector<float> overlapBufferBot;
	vector<float> inputBuffer;
	vector<float> outputBuffer;
	vector<float> albedoBuffer;
	vector<float> normalBuffer;
	overlapBufferTop.resize(3 * width * overlap2);
	overlapBufferTop2.resize(3 * width * overlap2);
	overlapBufferBot.resize(3 * width * overlap2);

	//create Filter
	SLG_LOG("[IntelOIDNPlugin] Creating OIDN filter instance");
	oidn::DeviceRef device = oidn::newDevice();
	device.commit();
	oidn::FilterRef filter = device.newFilter("RT");
	filter.set("hdr", true);

	//denoise the stripes
	//note that some paramenters are (only) defined inside the loop to be safe from rounding errors if users use odd resolutions
	//i.e. the tile width not being constant
	for (u_int iTile = 0; iTile < nTiles; ++iTile) {
		//set row  parameters
		u_int boundaryFront = height * iTile / nTiles;
		u_int boundaryBack = height * (iTile + 1) / nTiles;

		u_int overlapBack = iTile == 0 ? 0 : pixelOverlap;
		u_int overlapFront = iTile == nTiles - 1 ? 0 : pixelOverlap;
		u_int overlapBack2 = 2 * overlapBack;
		u_int overlapFront2 = 2 * overlapFront;
		u_int tileHeight = boundaryBack - boundaryFront + overlapFront + overlapBack;

		u_int tileWidthCumu = 0; //cumulative width of columns processed, needed for pixel array offsets.

		//resize buffers
		overlapBufferTile.resize(3 * overlap2 * tileHeight);

		for (u_int jTile = 0; jTile < nTiles; ++jTile) {
			SLG_LOG("Tile: " << nTiles * iTile + jTile + 1);
			//set column parameters
			u_int boundaryLeft = width * jTile / nTiles;
			u_int boundaryRight = width * (jTile + 1) / nTiles;

			u_int overlapLeft = jTile == 0 ? 0 : pixelOverlap;
			u_int overlapRight = jTile == nTiles - 1 ? 0 : pixelOverlap;
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
					//if (i == 40) {
					//	SLG_LOG("i: " << i);
					//}
					bufInd = 3 * (i * tileWidth + j);
					pixInd = rowPixelsCumu + tileWidthCumu + i * width + j;
					//if (i == 40) {
					//	SLG_LOG("j: " << j << " bufInd: " << bufInd << " pixInd: " << pixInd);
					//}
					inputBuffer[bufInd] = pixels[pixInd].c[0];
					inputBuffer[bufInd + 1] = pixels[pixInd].c[1];
					inputBuffer[bufInd + 2] = pixels[pixInd].c[2];
				}
			}
			filter.setImage("color", &inputBuffer[0], oidn::Format::Float3, tileWidth, tileHeight);

			//float *start = ((float *)pixels) + 3 * pixelStripeStart;
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

			SLG_LOG("IntelOIDNPlugin executing filter (stripe " << nTiles * iTile + jTile + 1 << ")");
			const double startTime = WallClockTime();
			filter.execute();
			SLG_LOG("IntelOIDNPlugin apply (stripe " << nTiles * iTile + jTile + 1 << ") took: " << (boost::format("%.1f") % (WallClockTime() - startTime)) << "secs");

			const char *errorMessage;
			if (device.getError(errorMessage) != oidn::Error::None)
				SLG_LOG("IntelOIDNPlugin error (stripe " << nTiles * iTile + jTile + 1 << "): " << errorMessage);

			SLG_LOG("IntelOIDNPlugin copying output buffer (stripe " << nTiles * iTile + jTile + 1 << ")");

			//merge column blocks

			//copy direct-denoise pixels to final array
			for (u_int i = overlapBack2; i < tileHeight - overlapFront2; ++i) {
				for (u_int j = overlapLeft2; j < tileWidth - overlapRight2; ++j) {
					bufInd = 3 * (i * tileWidth + j);
					pixInd = i * width + tileWidthCumu + rowPixelsCumu + j;
					pixels[pixInd].c[0] = outputBuffer[bufInd];
					pixels[pixInd].c[1] = outputBuffer[bufInd + 1];
					pixels[pixInd].c[2] = outputBuffer[bufInd + 2];
				}
			}

			//copy direct top-row-buffer pixels for all but last row
			if (iTile < nTiles - 1) {
				for (u_int i = tileHeight - overlapFront2; i < tileHeight; ++i) {
					for (u_int j = overlapLeft2; j < tileWidth - overlapRight2; ++j) {
						bufInd = 3 * (i * tileWidth + j);
						pixInd = 3 * ((i - tileHeight + overlapFront2) * width + tileWidthCumu + j);
						overlapBufferTop[pixInd] = outputBuffer[bufInd];
						overlapBufferTop[pixInd + 1] = outputBuffer[bufInd + 1];
						overlapBufferTop[pixInd + 2] = outputBuffer[bufInd + 2];
					}
				}
			}

			//copy direct bottom-row-buffer pixels for all but last row
			if (iTile > 0) {
				for (u_int i = 0; i < overlapBack2; ++i) {
					for (u_int j = overlapLeft2; j < tileWidth - overlapRight2; ++j) {
						bufInd = 3 * (i * tileWidth + j);
						pixInd = 3 * (i * width + tileWidthCumu + j);
						overlapBufferBot[pixInd] = outputBuffer[bufInd];
						overlapBufferBot[pixInd + 1] = outputBuffer[bufInd + 1];
						overlapBufferBot[pixInd + 2] = outputBuffer[bufInd + 2];
					}
				}
			}

			//overlap region with crossover for 2nd tile onwards and copy relevant region to row-buffer
			if (jTile > 0) {
				for (u_int j = 0; j < overlapLeft2; ++j) { //attention: i-j-loops swapped order
					multi1 = (1.0 * overlapLeft2 - j) / (overlapLeft2);
					multi2 = 1.0 * j / (overlapLeft2);
					for (u_int i = overlapBack2; i < tileHeight - overlapFront2; ++i) { //direct overlap
						bufInd = 3 * (i * overlapLeft2 + j);
						outInd = 3 * (i * tileWidth + j);
						pixInd = i * width + tileWidthCumu + rowPixelsCumu + j;
						pixels[pixInd].c[0] = overlapBufferTile[bufInd] * multi1 +outputBuffer[outInd] * multi2;
						pixels[pixInd].c[1] = overlapBufferTile[bufInd + 1] * multi1 +outputBuffer[outInd + 1] * multi2;
						pixels[pixInd].c[2] = overlapBufferTile[bufInd + 2] * multi1 +outputBuffer[outInd + 2] * multi2;
					}
					if (iTile > 0) {
						for (u_int i = 0; i < overlapBack2; ++i) {
							bufInd = 3 * (i * overlapLeft2 + j);
							outInd = 3 * (i * tileWidth + j);
							pixInd = 3 * (i * width + tileWidthCumu + j);
							overlapBufferBot[pixInd] = overlapBufferTile[bufInd] * multi1 + outputBuffer[outInd] * multi2;
							overlapBufferBot[pixInd + 1] = overlapBufferTile[bufInd + 1] * multi1 + outputBuffer[outInd + 1] * multi2;
							overlapBufferBot[pixInd + 2] = overlapBufferTile[bufInd + 2] * multi1 + outputBuffer[outInd + 2] * multi2;
						}
					}
					if (iTile < nTiles - 1) {
						for (u_int i = tileHeight - overlapFront2; i < tileHeight; ++i) {
							bufInd = 3 * (i * overlapLeft2 + j);
							outInd = 3 * (i * tileWidth + j);
							pixInd = 3 * ((i - tileHeight + overlapFront2) * width + tileWidthCumu + j);
							overlapBufferTop[pixInd] = overlapBufferTile[bufInd] * multi1 + outputBuffer[outInd] * multi2;
							overlapBufferTop[pixInd + 1] = overlapBufferTile[bufInd + 1] * multi1 + outputBuffer[outInd + 1] * multi2;
							overlapBufferTop[pixInd + 2] = overlapBufferTile[bufInd + 2] * multi1 + outputBuffer[outInd + 2] * multi2;
						}
					}
				}
			}

			//set column-buffer pixels for all but last column
			if (jTile < nTiles - 1) {
				for (u_int i = 0; i < tileHeight; ++i) {
					for (u_int j = tileWidth - overlapRight2; j < tileWidth; ++j) {
						bufInd = 3 * (i * tileWidth + j);
						pixInd = 3 * (i * overlapRight2 + j - tileWidth + overlapRight2);
						overlapBufferTile[pixInd] = outputBuffer[bufInd];
						overlapBufferTile[pixInd + 1] = outputBuffer[bufInd + 1];
						overlapBufferTile[pixInd + 2] = outputBuffer[bufInd + 2];
					}
				}
			}

			tileWidthCumu += tileWidth - overlapRight2;

		}

		//merge rows

		//overlap with previous overlapBufferRow for iTile > 0

		if (iTile > 0) {
			for (u_int i = 0; i < overlapBack2; ++i) {
				for (u_int j = 0; j < width; ++j) {
					multi1 = (1.0 * overlapBack2 - i) / (overlapBack2);
					multi2 = 1.0 * i / (overlapBack2);
					bufInd = 3 * (i * width + j);
					pixInd = i* width + j + rowPixelsCumu;
					pixels[pixInd].c[0] = overlapBufferTop2[bufInd] * multi1 + overlapBufferBot[bufInd] * multi2;
					pixels[pixInd].c[1] = overlapBufferTop2[bufInd + 1] * multi1 + overlapBufferBot[bufInd + 1] * multi2;
					pixels[pixInd].c[2] = overlapBufferTop2[bufInd + 2] * multi1 + overlapBufferBot[bufInd + 2] * multi2;
				}
			}
		}

		for (u_int i = 0; i < 3 * width * overlap2; ++i) {
			overlapBufferTop2[i] = overlapBufferTop[i];
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
