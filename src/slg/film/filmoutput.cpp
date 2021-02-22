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

#include <limits>
#include <algorithm>
#include <exception>

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include <OpenImageIO/imageio.h>
#include <OpenImageIO/imagebuf.h>

#include "luxrays/core/geometry/point.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/safesave.h"
#include "luxrays/utils/fileext.h"
#include "slg/editaction.h"
#include "slg/film/film.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "slg/film/imagepipeline/radiancechannelscale.h"
#include "slg/film/sampleresult.h"

using namespace std;
using namespace luxrays;
using namespace slg;
OIIO_NAMESPACE_USING

typedef unsigned char BYTE;

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

size_t Film::GetOutputSize(const FilmOutputs::FilmOutputType type) const {
	switch (type) {
		case FilmOutputs::RGB:
			return 3 * pixelCount;
		case FilmOutputs::RGBA:
			return 4 * pixelCount;
		case FilmOutputs::RGB_IMAGEPIPELINE:
			return 3 * pixelCount;
		case FilmOutputs::RGBA_IMAGEPIPELINE:
			return 4 * pixelCount;
		case FilmOutputs::ALPHA:
			return pixelCount;
		case FilmOutputs::DEPTH:
			return pixelCount;
		case FilmOutputs::POSITION:
			return 3 * pixelCount;
		case FilmOutputs::GEOMETRY_NORMAL:
			return 3 * pixelCount;
		case FilmOutputs::SHADING_NORMAL:
			return 3 * pixelCount;
		case FilmOutputs::MATERIAL_ID:
			return pixelCount;
		case FilmOutputs::DIRECT_DIFFUSE:
			return 3 * pixelCount;
		case FilmOutputs::DIRECT_DIFFUSE_REFLECT:
			return 3 * pixelCount;
		case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT:
			return 3 * pixelCount;
		case FilmOutputs::DIRECT_GLOSSY:
			return 3 * pixelCount;
		case FilmOutputs::DIRECT_GLOSSY_REFLECT:
			return 3 * pixelCount;
		case FilmOutputs::DIRECT_GLOSSY_TRANSMIT:
			return 3 * pixelCount;
		case FilmOutputs::EMISSION:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_DIFFUSE:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_DIFFUSE_REFLECT:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_GLOSSY:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_GLOSSY_REFLECT:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_SPECULAR:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_SPECULAR_REFLECT:
			return 3 * pixelCount;
		case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT:
			return 3 * pixelCount;
		case FilmOutputs::MATERIAL_ID_MASK:
			return pixelCount;
		case FilmOutputs::DIRECT_SHADOW_MASK:
			return pixelCount;
		case FilmOutputs::INDIRECT_SHADOW_MASK:
			return pixelCount;
		case FilmOutputs::RADIANCE_GROUP:
			return 3 * pixelCount;
		case FilmOutputs::UV:
			return 2 * pixelCount;
		case FilmOutputs::RAYCOUNT:
			return pixelCount;
		case FilmOutputs::BY_MATERIAL_ID:
			return 3 * pixelCount;
		case FilmOutputs::IRRADIANCE:
			return 3 * pixelCount;
		case FilmOutputs::OBJECT_ID:
			return pixelCount;
		case FilmOutputs::OBJECT_ID_MASK:
			return pixelCount;
		case FilmOutputs::BY_OBJECT_ID:
			return 3 * pixelCount;
		case FilmOutputs::SAMPLECOUNT:
			return pixelCount;
		case FilmOutputs::CONVERGENCE:
			return pixelCount;
		case FilmOutputs::MATERIAL_ID_COLOR:
			return 3 * pixelCount;
		case FilmOutputs::ALBEDO:
			return 3 * pixelCount;
		case FilmOutputs::AVG_SHADING_NORMAL:
			return 3 * pixelCount;
		case FilmOutputs::NOISE:
			return pixelCount;
		case FilmOutputs::USER_IMPORTANCE:
			return pixelCount;
		case FilmOutputs::CAUSTIC:
			return 3 * pixelCount;
		default:
			throw runtime_error("Unknown FilmOutputType in Film::GetOutputSize(): " + ToString(type));
	}
}

bool Film::HasOutput(const FilmOutputs::FilmOutputType type) const {
	switch (type) {
		case FilmOutputs::RGB:
			return HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) || HasChannel(RADIANCE_PER_SCREEN_NORMALIZED);
		case FilmOutputs::RGB_IMAGEPIPELINE:
			return HasChannel(IMAGEPIPELINE);
		case FilmOutputs::RGBA:
			return (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) || HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) && HasChannel(ALPHA);
		case FilmOutputs::RGBA_IMAGEPIPELINE:
			return HasChannel(IMAGEPIPELINE) && HasChannel(ALPHA);
		case FilmOutputs::ALPHA:
			return HasChannel(ALPHA);
		case FilmOutputs::DEPTH:
			return HasChannel(DEPTH);
		case FilmOutputs::POSITION:
			return HasChannel(POSITION);
		case FilmOutputs::GEOMETRY_NORMAL:
			return HasChannel(GEOMETRY_NORMAL);
		case FilmOutputs::SHADING_NORMAL:
			return HasChannel(SHADING_NORMAL);
		case FilmOutputs::MATERIAL_ID:
			return HasChannel(MATERIAL_ID);
		case FilmOutputs::DIRECT_DIFFUSE:
			return HasChannel(DIRECT_DIFFUSE);
		case FilmOutputs::DIRECT_DIFFUSE_REFLECT:
			return HasChannel(DIRECT_DIFFUSE_REFLECT);
		case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT:
			return HasChannel(DIRECT_DIFFUSE_TRANSMIT);
		case FilmOutputs::DIRECT_GLOSSY:
			return HasChannel(DIRECT_GLOSSY);
		case FilmOutputs::DIRECT_GLOSSY_REFLECT:
			return HasChannel(DIRECT_GLOSSY_REFLECT);
		case FilmOutputs::DIRECT_GLOSSY_TRANSMIT:
			return HasChannel(DIRECT_GLOSSY_TRANSMIT);
		case FilmOutputs::EMISSION:
			return HasChannel(EMISSION);
		case FilmOutputs::INDIRECT_DIFFUSE:
			return HasChannel(INDIRECT_DIFFUSE);
		case FilmOutputs::INDIRECT_DIFFUSE_REFLECT:
			return HasChannel(INDIRECT_DIFFUSE_REFLECT);
		case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT:
			return HasChannel(INDIRECT_DIFFUSE_TRANSMIT);
		case FilmOutputs::INDIRECT_GLOSSY:
			return HasChannel(INDIRECT_GLOSSY);
		case FilmOutputs::INDIRECT_GLOSSY_REFLECT:
			return HasChannel(INDIRECT_GLOSSY_REFLECT);
		case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT:
			return HasChannel(INDIRECT_GLOSSY_TRANSMIT);
		case FilmOutputs::INDIRECT_SPECULAR:
			return HasChannel(INDIRECT_SPECULAR);
		case FilmOutputs::INDIRECT_SPECULAR_REFLECT:
			return HasChannel(INDIRECT_SPECULAR_REFLECT);
		case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT:
			return HasChannel(INDIRECT_SPECULAR_TRANSMIT);
		case FilmOutputs::MATERIAL_ID_MASK:
			return HasChannel(MATERIAL_ID_MASK);
		case FilmOutputs::DIRECT_SHADOW_MASK:
			return HasChannel(DIRECT_SHADOW_MASK);
		case FilmOutputs::INDIRECT_SHADOW_MASK:
			return HasChannel(INDIRECT_SHADOW_MASK);
		case FilmOutputs::RADIANCE_GROUP:
			return true;
		case FilmOutputs::UV:
			return HasChannel(UV);
		case FilmOutputs::RAYCOUNT:
			return HasChannel(RAYCOUNT);
		case FilmOutputs::BY_MATERIAL_ID:
			return HasChannel(BY_MATERIAL_ID);
		case FilmOutputs::IRRADIANCE:
			return HasChannel(IRRADIANCE);
		case FilmOutputs::OBJECT_ID:
			return HasChannel(OBJECT_ID);
		case FilmOutputs::OBJECT_ID_MASK:
			return HasChannel(OBJECT_ID_MASK);
		case FilmOutputs::BY_OBJECT_ID:
			return HasChannel(BY_OBJECT_ID);
		case FilmOutputs::SAMPLECOUNT:
			return HasChannel(SAMPLECOUNT);
		case FilmOutputs::CONVERGENCE:
			return HasChannel(CONVERGENCE);
		case FilmOutputs::SERIALIZED_FILM:
			return filmOutputs.HasType(FilmOutputs::SERIALIZED_FILM);
		case FilmOutputs::MATERIAL_ID_COLOR:
			return HasChannel(MATERIAL_ID_COLOR);
		case FilmOutputs::ALBEDO:
			return HasChannel(ALBEDO);
		case FilmOutputs::AVG_SHADING_NORMAL:
			return HasChannel(AVG_SHADING_NORMAL);
		case FilmOutputs::NOISE:
			return HasChannel(NOISE);
		case FilmOutputs::USER_IMPORTANCE:
			return HasChannel(USER_IMPORTANCE);
		case FilmOutputs::CAUSTIC:
			// Caustic has not dedicated channel but is RADIANCE_PER_SCREEN_NORMALIZED
			// when doing hybrid rendering
			return HasChannel(RADIANCE_PER_SCREEN_NORMALIZED);
		default:
			throw runtime_error("Unknown film output type in Film::HasOutput(): " + ToString(type));
	}
}

void Film::Output() {
	for (u_int i = 0; i < filmOutputs.GetCount(); ++i)
		Output(filmOutputs.GetFileName(i), filmOutputs.GetType(i),&filmOutputs.GetProperties(i));
}

void Film::Output(const string &fileName,const FilmOutputs::FilmOutputType type,
		const Properties *props, const bool executeImagePipeline) { 
	// Handle the special case of the serialized film output
	if (type == FilmOutputs::SERIALIZED_FILM) {
		if (!filmOutputs.HasType(FilmOutputs::SERIALIZED_FILM))
			throw runtime_error("SERIALIZED_FILM has not been configured as output in Film::Output()");

		SLG_LOG("Outputting film: " << fileName << " type: " << ToString(type));

		if (filmOutputs.UseSafeSave()) {
			SafeSave safeSave(fileName);

			Film::SaveSerialized(safeSave.GetSaveFileName(), this);

			safeSave.Process();
		} else
			Film::SaveSerialized(fileName, this);
		
		return;
	}

	u_int maskMaterialIDsIndex = 0;
	u_int byMaterialIDsIndex = 0;
	u_int maskObjectIDsIndex = 0;
	u_int byObjectIDsIndex = 0;
	u_int radianceGroupIndex = 0;
	u_int imagePipelineIndex = 0;
	u_int channelCount = 3;

	switch (type) {
		case FilmOutputs::RGB:
			if (!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED))
				return;
			break;
		case FilmOutputs::RGB_IMAGEPIPELINE:
			if (!HasChannel(IMAGEPIPELINE))
				return;
			imagePipelineIndex = props ? props->Get(Property("index")(0)).Get<u_int>() : 0;
			if (imagePipelineIndex >= imagePipelines.size())
				return;
			if (executeImagePipeline)
				ExecuteImagePipeline(imagePipelineIndex);
			break;
		case FilmOutputs::RGBA:
			if ((!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) || !HasChannel(ALPHA))
				return;
			channelCount = 4;
			break;
		case FilmOutputs::RGBA_IMAGEPIPELINE:
			if (!HasChannel(IMAGEPIPELINE) || !HasChannel(ALPHA))
				return;
			imagePipelineIndex = props ? props->Get(Property("index")(0)).Get<u_int>() : 0;
			if (imagePipelineIndex >= imagePipelines.size())
				return;
			if (executeImagePipeline)
				ExecuteImagePipeline(imagePipelineIndex);
			channelCount = 4;
			break;
		case FilmOutputs::ALPHA:
			if (!HasChannel(ALPHA))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::DEPTH:
			if (!HasChannel(DEPTH))
				return;
			channelCount = 1;		
			break;
		case FilmOutputs::POSITION:
			if (!HasChannel(POSITION))
				return;	
			break;
		case FilmOutputs::GEOMETRY_NORMAL:
			if (!HasChannel(GEOMETRY_NORMAL))
				return;
			break;
		case FilmOutputs::SHADING_NORMAL:
			if (!HasChannel(SHADING_NORMAL))
				return;
			break;
		case FilmOutputs::MATERIAL_ID:
			if (!HasChannel(MATERIAL_ID))
				return;
			break;
		case FilmOutputs::DIRECT_DIFFUSE:
			if (!HasChannel(DIRECT_DIFFUSE))
				return;
			break;
		case FilmOutputs::DIRECT_DIFFUSE_REFLECT:
			if (!HasChannel(DIRECT_DIFFUSE_REFLECT))
				return;
			break;
		case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT:
			if (!HasChannel(DIRECT_DIFFUSE_TRANSMIT))
				return;
			break;
		case FilmOutputs::DIRECT_GLOSSY:
			if (!HasChannel(DIRECT_GLOSSY))
				return;
			break;
		case FilmOutputs::DIRECT_GLOSSY_REFLECT:
			if (!HasChannel(DIRECT_GLOSSY_REFLECT))
				return;
			break;
		case FilmOutputs::DIRECT_GLOSSY_TRANSMIT:
			if (!HasChannel(DIRECT_GLOSSY_TRANSMIT))
				return;
			break;
		case FilmOutputs::EMISSION:
			if (!HasChannel(EMISSION))
				return;
			break;
		case FilmOutputs::INDIRECT_DIFFUSE:
			if (!HasChannel(INDIRECT_DIFFUSE))
				return;
			break;
		case FilmOutputs::INDIRECT_DIFFUSE_REFLECT:
			if (!HasChannel(INDIRECT_DIFFUSE_REFLECT))
				return;
			break;
		case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT:
			if (!HasChannel(INDIRECT_DIFFUSE_TRANSMIT))
				return;
			break;
		case FilmOutputs::INDIRECT_GLOSSY:
			if (!HasChannel(INDIRECT_GLOSSY))
				return;
			break;
		case FilmOutputs::INDIRECT_GLOSSY_REFLECT:
			if (!HasChannel(INDIRECT_GLOSSY_REFLECT))
				return;
			break;
		case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT:
			if (!HasChannel(INDIRECT_GLOSSY_TRANSMIT))
				return;
			break;
		case FilmOutputs::INDIRECT_SPECULAR:
			if (!HasChannel(INDIRECT_SPECULAR))
				return;
			break;
		case FilmOutputs::INDIRECT_SPECULAR_REFLECT:
			if (!HasChannel(INDIRECT_SPECULAR_REFLECT))
				return;
			break;
		case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT:
			if (!HasChannel(INDIRECT_SPECULAR_TRANSMIT))
				return;
			break;
		case FilmOutputs::MATERIAL_ID_MASK:
			if (HasChannel(MATERIAL_ID_MASK) && props) {
				channelCount = 1;		
				// Look for the material mask ID index
				const u_int id = props->Get(Property("id")(255)).Get<u_int>();
				bool found = false;
				for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
					if (maskMaterialIDs[i] == id) {
						maskMaterialIDsIndex = i;
						found = true;
						break;
					}
				}
				if (!found)
					return;
			} else
				return;
			break;
		case FilmOutputs::DIRECT_SHADOW_MASK:
			if (!HasChannel(DIRECT_SHADOW_MASK))
			      return;
			channelCount = 1;
			break;
		case FilmOutputs::INDIRECT_SHADOW_MASK:
			if (!HasChannel(INDIRECT_SHADOW_MASK))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::RADIANCE_GROUP:
			if (!props)
				return;		
			radianceGroupIndex = props->Get(Property("id")(0)).Get<u_int>();
			if (radianceGroupIndex >= radianceGroupCount)
				return;
			break;
		case FilmOutputs::UV:
			if (!HasChannel(UV))
				return;
			break;
		case FilmOutputs::RAYCOUNT:
			if (!HasChannel(RAYCOUNT))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::BY_MATERIAL_ID:
			if (HasChannel(BY_MATERIAL_ID) && props) {
				// Look for the material mask ID index
				const u_int id = props->Get(Property("id")(255)).Get<u_int>();
				bool found = false;
				for (u_int i = 0; i < byMaterialIDs.size(); ++i) {
					if (byMaterialIDs[i] == id) {
						byMaterialIDsIndex = i;
						found = true;
						break;
					}
				}
				if (!found)
					return;
			} else
				return;
			break;
		case FilmOutputs::IRRADIANCE:
			if (!HasChannel(IRRADIANCE))
				return;
			break;
		case FilmOutputs::OBJECT_ID:
			if (!HasChannel(OBJECT_ID))
				return;
			break;
		case FilmOutputs::OBJECT_ID_MASK:
			if (HasChannel(OBJECT_ID_MASK) && props) {
				channelCount = 1;		
				// Look for the object mask ID index
				const u_int id = props->Get(Property("id")(255)).Get<u_int>();
				bool found = false;
				for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
					if (maskObjectIDs[i] == id) {
						maskObjectIDsIndex = i;
						found = true;
						break;
					}
				}
				if (!found)
					return;
			} else
				return;
			break;
		case FilmOutputs::BY_OBJECT_ID:
			if (HasChannel(BY_OBJECT_ID) && props) {
				// Look for the object mask ID index
				const u_int id = props->Get(Property("id")(255)).Get<u_int>();
				bool found = false;
				for (u_int i = 0; i < byObjectIDs.size(); ++i) {
					if (byObjectIDs[i] == id) {
						byObjectIDsIndex = i;
						found = true;
						break;
					}
				}
				if (!found)
					return;
			} else
				return;
			break;
		case FilmOutputs::SAMPLECOUNT:
			if (!HasChannel(SAMPLECOUNT))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::CONVERGENCE:
			if (!HasChannel(CONVERGENCE))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::MATERIAL_ID_COLOR:
			if (!HasChannel(MATERIAL_ID_COLOR))
				return;
			break;
		case FilmOutputs::ALBEDO:
			if (!HasChannel(ALBEDO))
				return;
			break;
		case FilmOutputs::AVG_SHADING_NORMAL:
			if (!HasChannel(AVG_SHADING_NORMAL))
				return;
			break;
		case FilmOutputs::NOISE:
			if (!HasChannel(NOISE))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::USER_IMPORTANCE:
			if (!HasChannel(USER_IMPORTANCE))
				return;
			channelCount = 1;
			break;
		case FilmOutputs::CAUSTIC:
			// Caustic has not dedicated channel but is RADIANCE_PER_SCREEN_NORMALIZED
			// when doing hybrid rendering
			if (!HasChannel(RADIANCE_PER_SCREEN_NORMALIZED))
				return;
			break;
		default:
			throw runtime_error("Unknown film output type in Film::Output(): " + ToString(type));
	}

	ImageBuf buffer;
	
	SLG_LOG("Outputting film: " << fileName << " type: " << ToString(type));

	bool hdrImage = false;
	const string fileExtension = GetFileNameExt(fileName);
	if (fileExtension == ".exr" || fileExtension == ".hdr")
		hdrImage = true;

	if ((type == FilmOutputs::MATERIAL_ID) || (type == FilmOutputs::OBJECT_ID) ||
			((type == FilmOutputs::SAMPLECOUNT) && (!hdrImage))) {
		// For IDs we must copy into int buffer first or risk screwing up the IDs
		ImageSpec spec(width, height, channelCount, TypeDesc::UINT8);
		buffer.reset(spec);

		GenericFrameBuffer<1, 0, u_int> *channel;
		switch (type) {
			case FilmOutputs::MATERIAL_ID:
				channel = channel_MATERIAL_ID;
				break;
			case FilmOutputs::OBJECT_ID:
				channel = channel_OBJECT_ID;
				break;
			case FilmOutputs::SAMPLECOUNT:
				channel = channel_SAMPLECOUNT;
				break;
			default:
				throw runtime_error("Unknown film output type in Film::Output(): " + ToString(type));
		}

		for (ImageBuf::ConstIterator<BYTE> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			BYTE *pixel = (BYTE *)buffer.pixeladdr(x, y, 0);
			y = height - y - 1;
			
			if (pixel == NULL)
				throw runtime_error("Error while unpacking film data, could not address buffer!");

			const u_int src = *(channel->GetPixel(x, y));
			pixel[0] = (BYTE)(src & 0x0000ffu);
			pixel[1] = (BYTE)((src & 0x00ff00u) >> 8);
			pixel[2] = (BYTE)((src & 0xff0000u) >> 16);
		}
	} else {
		// OIIO 1 channel EXR output is apparently not working, I write 3 channels as
		// temporary workaround
		//
		// Note: they are working is just that 1 channel EXR is supposed to be alpha
		// and not grey so it is not shown by some program like exrdisplay

		// For all others copy into float buffer first and let OIIO figure out the conversion on write
		ImageSpec spec(width, height, (channelCount == 1) ? 3 : channelCount, TypeDesc::FLOAT);
		buffer.reset(spec);
	
		const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();
		
		for (ImageBuf::ConstIterator<float> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			float *pixel = (float *)buffer.pixeladdr(x, y, 0);
			y = height - y - 1;

			if (pixel == NULL)
				throw runtime_error("Error while unpacking film data, could not address buffer!");

			switch (type) {
				case FilmOutputs::RGB: {
					// Accumulate all light groups			
					GetPixelFromMergedSampleBuffers(0, RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
							x, y, pixel);
					break;
				}
				case FilmOutputs::RGB_IMAGEPIPELINE: {
					channel_IMAGEPIPELINEs[imagePipelineIndex]->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::RGBA: {
					// Accumulate all light groups
					GetPixelFromMergedSampleBuffers(0, RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
							x, y, pixel);
					channel_ALPHA->GetWeightedPixel(x, y, &pixel[3]);
					break;
				}
				case FilmOutputs::RGBA_IMAGEPIPELINE: {
					channel_IMAGEPIPELINEs[imagePipelineIndex]->GetWeightedPixel(x, y, pixel);
					channel_ALPHA->GetWeightedPixel(x, y, &pixel[3]);
					break;
				}
				case FilmOutputs::ALPHA: {
					channel_ALPHA->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DEPTH: {
					channel_DEPTH->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::POSITION: {
					channel_POSITION->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::GEOMETRY_NORMAL: {
					channel_GEOMETRY_NORMAL->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::SHADING_NORMAL: {
					channel_SHADING_NORMAL->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_DIFFUSE: {
					channel_DIRECT_DIFFUSE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_DIFFUSE_REFLECT: {
					channel_DIRECT_DIFFUSE_REFLECT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT: {
					channel_DIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_GLOSSY: {
					channel_DIRECT_GLOSSY->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_GLOSSY_REFLECT: {
					channel_DIRECT_GLOSSY_REFLECT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_GLOSSY_TRANSMIT: {
					channel_DIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::EMISSION: {
					channel_EMISSION->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_DIFFUSE: {
					channel_INDIRECT_DIFFUSE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_DIFFUSE_REFLECT: {
					channel_INDIRECT_DIFFUSE_REFLECT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT: {
					channel_INDIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_GLOSSY: {
					channel_INDIRECT_GLOSSY->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_GLOSSY_REFLECT: {
					channel_INDIRECT_GLOSSY_REFLECT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT: {
					channel_INDIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_SPECULAR: {
					channel_INDIRECT_SPECULAR->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_SPECULAR_REFLECT: {
					channel_INDIRECT_SPECULAR_REFLECT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT: {
					channel_INDIRECT_SPECULAR_TRANSMIT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::MATERIAL_ID_MASK: {
					channel_MATERIAL_ID_MASKs[maskMaterialIDsIndex]->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::DIRECT_SHADOW_MASK: {
					channel_DIRECT_SHADOW_MASK->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_SHADOW_MASK: {
					channel_INDIRECT_SHADOW_MASK->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::RADIANCE_GROUP: {
					// Clear the pixel
					pixel[0] = 0.f;
					pixel[1] = 0.f;
					pixel[2] = 0.f;

					// Accumulate all light groups
					if (radianceGroupIndex < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()) {
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[radianceGroupIndex]->AccumulateWeightedPixel(x, y, pixel);

						// Normalize the value
						const float factor = RADIANCE_PER_SCREEN_NORMALIZED_SampleCount / pixelCount;
						pixel[0] *= factor;
						pixel[1] *= factor;
						pixel[2] *= factor;
					}
					if (radianceGroupIndex < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size())
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[radianceGroupIndex]->AccumulateWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::UV: {
					channel_UV->GetWeightedPixel(x, y, pixel);
					pixel[2] = 0.f;
					break;
				}
				case FilmOutputs::RAYCOUNT: {
					channel_RAYCOUNT->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::BY_MATERIAL_ID: {
					channel_BY_MATERIAL_IDs[byMaterialIDsIndex]->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::IRRADIANCE: {
					channel_IRRADIANCE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::OBJECT_ID_MASK: {
					channel_OBJECT_ID_MASKs[maskObjectIDsIndex]->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::BY_OBJECT_ID: {
					channel_BY_OBJECT_IDs[byObjectIDsIndex]->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::SAMPLECOUNT: {
					u_int val;
					channel_SAMPLECOUNT->GetWeightedPixel(x, y, &val);
					pixel[0] = val;
					break;
				}
				case FilmOutputs::CONVERGENCE: {
					channel_CONVERGENCE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::MATERIAL_ID_COLOR: {
					channel_MATERIAL_ID_COLOR->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::ALBEDO: {
					channel_ALBEDO->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::AVG_SHADING_NORMAL: {
					channel_AVG_SHADING_NORMAL->GetWeightedPixel(x, y, pixel);
					// Normalize the value (should I ?)
					/*const float length2 = pixel[0] * pixel[0] + pixel[1] * pixel[1] + pixel[2] * pixel[2];
					if (length2 > 0.f) {
						const float k = 1.f / sqrtf(length2);
						pixel[0] *= k;
						pixel[1] *= k;
						pixel[2] *= k;
					}*/
					break;
				}
				case FilmOutputs::NOISE: {
					channel_NOISE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::USER_IMPORTANCE: {
					channel_USER_IMPORTANCE->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::CAUSTIC: {
					// Accumulate all light groups			
					GetPixelFromMergedSampleBuffers(0, false, true,
							RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
							x, y, pixel);
					break;
				}
				default:
					throw runtime_error("Unknown film output type in Film::Output(): " + ToString(type));
			}

			// OIIO 1 channel EXR output is apparently not working, I write 3 channels as
			// temporary workaround
			if (channelCount == 1) {
				pixel[1] = pixel[0];
				pixel[2] = pixel[0];
			}
		}
	}

	if (filmOutputs.UseSafeSave()) {
		SafeSave safeSave(fileName);

		if (!buffer.write(safeSave.GetSaveFileName()))
			throw runtime_error("Error while writing an output type in Film::Output(): " +
					safeSave.GetSaveFileName() + " (error = " + buffer.geterror() + ")");

		safeSave.Process();
	} else {
		if (!buffer.write(fileName))
			throw runtime_error("Error while writing an output type in Film::Output(): " +
					fileName + " (error = " + buffer.geterror() + ")");
	}
}

u_int Film::GetOutputCount(const FilmOutputs::FilmOutputType type) const {
	switch (type) {
		case FilmOutputs::RGB_IMAGEPIPELINE:
			return channel_IMAGEPIPELINEs.size();
		case FilmOutputs::RGBA_IMAGEPIPELINE:
			return channel_IMAGEPIPELINEs.size();
		case FilmOutputs::MATERIAL_ID_MASK:
			return channel_MATERIAL_ID_MASKs.size();
		case FilmOutputs::RADIANCE_GROUP:
			return channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size();
		case FilmOutputs::BY_MATERIAL_ID:
			return channel_BY_MATERIAL_IDs.size();
		case FilmOutputs::OBJECT_ID_MASK:
			return channel_OBJECT_ID_MASKs.size();
		case FilmOutputs::BY_OBJECT_ID:
			return channel_BY_OBJECT_IDs.size();
		default:
			if (HasOutput(type))
				return 1;
			else
				return 0;
	}
}

template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer,
		const u_int index, const bool executeImagePipeline) {
	if (!HasOutput(type))
		throw runtime_error("Film output not defined in Film::GetOutput<float>(): " + ToString(type));

	if (index > GetOutputCount(type))
		throw runtime_error("Film output index not defined in Film::GetOutput<float>(): " + ToString(type) + "/" + ToString(index));

	switch (type) {
		case FilmOutputs::RGB: {
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();

			for (u_int i = 0; i < pixelCount; ++i)
				GetPixelFromMergedSampleBuffers(0,
						RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
						i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::RGB_IMAGEPIPELINE:
			if (executeImagePipeline)
				ExecuteImagePipeline(index);

			copy(channel_IMAGEPIPELINEs[index]->GetPixels(), channel_IMAGEPIPELINEs[index]->GetPixels() + pixelCount * 3, buffer);
			break;
		case FilmOutputs::RGBA: {
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();

			for (u_int i = 0; i < pixelCount; ++i) {
				const u_int offset = i * 4;
				GetPixelFromMergedSampleBuffers(0,
						RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
						i, &buffer[offset]);
				channel_ALPHA->GetWeightedPixel(i, &buffer[offset + 3]);
			}
			break;
		}
		case FilmOutputs::RGBA_IMAGEPIPELINE: {
			if (executeImagePipeline)
				ExecuteImagePipeline(index);

			float *srcRGB = channel_IMAGEPIPELINEs[index]->GetPixels();
			float *dst = buffer;
			for (u_int i = 0; i < pixelCount; ++i) {
				*dst++ = *srcRGB++;
				*dst++ = *srcRGB++;
				*dst++ = *srcRGB++;
				channel_ALPHA->GetWeightedPixel(i, dst++);
			}
			break;
		}
		case FilmOutputs::ALPHA: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_ALPHA->GetWeightedPixel(i, &buffer[i]);
			break;
		}
		case FilmOutputs::DEPTH:
			copy(channel_DEPTH->GetPixels(), channel_DEPTH->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::POSITION:
			copy(channel_POSITION->GetPixels(), channel_POSITION->GetPixels() + pixelCount * 3, buffer);
			break;
		case FilmOutputs::GEOMETRY_NORMAL:
			copy(channel_GEOMETRY_NORMAL->GetPixels(), channel_GEOMETRY_NORMAL->GetPixels() + pixelCount * 3, buffer);
			break;
		case FilmOutputs::SHADING_NORMAL:
			copy(channel_SHADING_NORMAL->GetPixels(), channel_SHADING_NORMAL->GetPixels() + pixelCount * 3, buffer);
			break;
		case FilmOutputs::DIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_DIFFUSE->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::DIRECT_DIFFUSE_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_DIFFUSE_REFLECT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::DIRECT_DIFFUSE_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::DIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_GLOSSY->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::DIRECT_GLOSSY_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_GLOSSY_REFLECT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::DIRECT_GLOSSY_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::EMISSION: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_EMISSION->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_DIFFUSE: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_DIFFUSE->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_DIFFUSE_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_DIFFUSE_REFLECT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_DIFFUSE_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_DIFFUSE_TRANSMIT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_GLOSSY->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_GLOSSY_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_GLOSSY_REFLECT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_GLOSSY_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_GLOSSY_TRANSMIT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_SPECULAR: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_SPECULAR->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_SPECULAR_REFLECT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_SPECULAR_REFLECT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_SPECULAR_TRANSMIT: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_SPECULAR_TRANSMIT->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::MATERIAL_ID_MASK: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_MATERIAL_ID_MASKs[index]->GetWeightedPixel(i, &buffer[i]);
			break;
		}
		case FilmOutputs::DIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_SHADOW_MASK->GetWeightedPixel(i, &buffer[i]);
			break;
		}
		case FilmOutputs::INDIRECT_SHADOW_MASK: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_SHADOW_MASK->GetWeightedPixel(i, &buffer[i]);
			break;
		}
		case FilmOutputs::RADIANCE_GROUP: {
			fill(buffer, buffer + 3 * pixelCount, 0.f);

			if (index < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()) {
				const ImagePipeline *ip = (imagePipelines.size() > 0) ? imagePipelines[0] : NULL;

				float *dst = buffer;
				for (u_int i = 0; i < pixelCount; ++i) {
					float c[3];
					channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->GetWeightedPixel(i, c);
					if (ip)
						ip->radianceChannelScales[index].Scale(c);

					*dst++ += c[0];
					*dst++ += c[1];
					*dst++ += c[2];
				}
			}

			if (index < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()) {
				const ImagePipeline *ip = (imagePipelines.size() > 0) ? imagePipelines[0] : NULL;

				float *dst = buffer;
				for (u_int i = 0; i < pixelCount; ++i) {
					float c[3];
					channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->GetWeightedPixel(i, c);
					if (ip)
						ip->radianceChannelScales[index].Scale(c);

					*dst++ += c[0];
					*dst++ += c[1];
					*dst++ += c[2];
				}
			}
			break;
		}
		case FilmOutputs::UV:
			copy(channel_UV->GetPixels(), channel_UV->GetPixels() + pixelCount * 2, buffer);
			break;
		case FilmOutputs::RAYCOUNT:
			copy(channel_RAYCOUNT->GetPixels(), channel_RAYCOUNT->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::BY_MATERIAL_ID: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_BY_MATERIAL_IDs[index]->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::IRRADIANCE: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_IRRADIANCE->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::OBJECT_ID_MASK: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_OBJECT_ID_MASKs[index]->GetWeightedPixel(i, &buffer[i]);
			break;
		}
		case FilmOutputs::BY_OBJECT_ID: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_BY_OBJECT_IDs[index]->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::CONVERGENCE:
			copy(channel_CONVERGENCE->GetPixels(), channel_CONVERGENCE->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::MATERIAL_ID_COLOR: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_MATERIAL_ID_COLOR->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::ALBEDO: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_ALBEDO->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::AVG_SHADING_NORMAL: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_AVG_SHADING_NORMAL->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::NOISE:
			copy(channel_NOISE->GetPixels(), channel_NOISE->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::USER_IMPORTANCE:
			copy(channel_USER_IMPORTANCE->GetPixels(), channel_USER_IMPORTANCE->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::CAUSTIC: {
			const double RADIANCE_PER_SCREEN_NORMALIZED_SampleCount = samplesCounts.GetSampleCount_RADIANCE_PER_SCREEN_NORMALIZED();

			for (u_int i = 0; i < pixelCount; ++i)
				GetPixelFromMergedSampleBuffers(0, false, true,
						RADIANCE_PER_SCREEN_NORMALIZED_SampleCount,
						i, &buffer[i * 3]);
			break;
		}
		default:
			throw runtime_error("Unknown film output type in Film::GetOutput<float>(): " + ToString(type));
	}
}

template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer,
		const u_int index, const bool executeImagePipeline) {
	if (!HasOutput(type))
		throw runtime_error("Film output not defined in Film::GetOutput<u_int>(): " + ToString(type));

	if (index > GetOutputCount(type))
		throw runtime_error("Film output index not defined in Film::GetOutput<float>(): " + ToString(type) + "/" + ToString(index));

	switch (type) {
		case FilmOutputs::MATERIAL_ID:
			copy(channel_MATERIAL_ID->GetPixels(), channel_MATERIAL_ID->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::OBJECT_ID:
			copy(channel_OBJECT_ID->GetPixels(), channel_OBJECT_ID->GetPixels() + pixelCount, buffer);
			break;
		case FilmOutputs::SAMPLECOUNT:
			copy(channel_SAMPLECOUNT->GetPixels(), channel_SAMPLECOUNT->GetPixels() + pixelCount, buffer);
			break;
		default:
			throw runtime_error("Unknown film output type in Film::GetOutput<u_int>(): " + ToString(type));
	}
}
