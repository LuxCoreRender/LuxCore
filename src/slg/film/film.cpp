/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxRender.                                       *
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
OIIO_NAMESPACE_USING

#include "luxrays/core/geometry/point.h"
#include "luxrays/utils/properties.h"
#include "slg/film/film.h"
#include "slg/film/sampleresult.h"
#include "slg/editaction.h"
#include "slg/utils/varianceclamping.h"

using namespace std;
using namespace luxrays;
using namespace slg;

typedef unsigned char BYTE;

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film() {
	initialized = false;

	width = 0;
	height = 0;
	subRegion[0] = 0;
	subRegion[1] = 0;
	subRegion[2] = 0;
	subRegion[3] = 0;
	radianceGroupCount = 0;

	channel_ALPHA = NULL;
	channel_RGB_TONEMAPPED = NULL;
	channel_DEPTH = NULL;
	channel_POSITION = NULL;
	channel_GEOMETRY_NORMAL = NULL;
	channel_SHADING_NORMAL = NULL;
	channel_MATERIAL_ID = NULL;
	channel_DIRECT_DIFFUSE = NULL;
	channel_DIRECT_GLOSSY = NULL;
	channel_EMISSION = NULL;
	channel_INDIRECT_DIFFUSE = NULL;
	channel_INDIRECT_GLOSSY = NULL;
	channel_INDIRECT_SPECULAR = NULL;
	channel_DIRECT_SHADOW_MASK = NULL;
	channel_INDIRECT_SHADOW_MASK = NULL;
	channel_UV = NULL;
	channel_RAYCOUNT = NULL;
	channel_IRRADIANCE = NULL;

	convTest = NULL;

	enabledOverlappedScreenBufferUpdate = true;
	rgbTonemapUpdate = true;

	imagePipeline = NULL;
}

Film::Film(const u_int w, const u_int h, const u_int *sr) {
	initialized = false;

	width = w;
	height = h;
	if (sr) {
		subRegion[0] = sr[0];
		subRegion[1] = sr[1];
		subRegion[2] = sr[2];
		subRegion[3] = sr[3];
	} else {
		subRegion[0] = 0;
		subRegion[1] = w - 1;
		subRegion[2] = 0;
		subRegion[3] = h - 1;
	}
	radianceGroupCount = 1;

	channel_ALPHA = NULL;
	channel_RGB_TONEMAPPED = NULL;
	channel_DEPTH = NULL;
	channel_POSITION = NULL;
	channel_GEOMETRY_NORMAL = NULL;
	channel_SHADING_NORMAL = NULL;
	channel_MATERIAL_ID = NULL;
	channel_DIRECT_DIFFUSE = NULL;
	channel_DIRECT_GLOSSY = NULL;
	channel_EMISSION = NULL;
	channel_INDIRECT_DIFFUSE = NULL;
	channel_INDIRECT_GLOSSY = NULL;
	channel_INDIRECT_SPECULAR = NULL;
	channel_DIRECT_SHADOW_MASK = NULL;
	channel_INDIRECT_SHADOW_MASK = NULL;
	channel_UV = NULL;
	channel_RAYCOUNT = NULL;
	channel_IRRADIANCE = NULL;

	convTest = NULL;

	enabledOverlappedScreenBufferUpdate = true;
	rgbTonemapUpdate = true;

	imagePipeline = NULL;
}

Film::~Film() {
	delete imagePipeline;

	delete convTest;

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size(); ++i)
		delete channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i];
	for (u_int i = 0; i < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i)
		delete channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i];
	delete channel_ALPHA;
	delete channel_RGB_TONEMAPPED;
	delete channel_DEPTH;
	delete channel_POSITION;
	delete channel_GEOMETRY_NORMAL;
	delete channel_SHADING_NORMAL;
	delete channel_MATERIAL_ID;
	delete channel_DIRECT_DIFFUSE;
	delete channel_DIRECT_GLOSSY;
	delete channel_EMISSION;
	delete channel_INDIRECT_DIFFUSE;
	delete channel_INDIRECT_GLOSSY;
	delete channel_INDIRECT_SPECULAR;
	for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i)
		delete channel_MATERIAL_ID_MASKs[i];
	delete channel_DIRECT_SHADOW_MASK;
	delete channel_INDIRECT_SHADOW_MASK;
	delete channel_UV;
	delete channel_RAYCOUNT;
	for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i)
		delete channel_BY_MATERIAL_IDs[i];
	delete channel_IRRADIANCE;
}

void Film::CopyDynamicSettings(const Film &film) {
	channels = film.channels;
	maskMaterialIDs = film.maskMaterialIDs;
	byMaterialIDs = film.byMaterialIDs;
	radianceGroupCount = film.radianceGroupCount;
	radianceChannelScales = film.radianceChannelScales;
	SetRGBTonemapUpdateFlag(film.rgbTonemapUpdate);
	SetImagePipeline(film.GetImagePipeline()->Copy());
	SetOverlappedScreenBufferUpdateFlag(film.IsOverlappedScreenBufferUpdate());
}

void Film::AddChannel(const FilmChannelType type, const Properties *prop) {
	if (initialized)
		throw runtime_error("It is only possible to add a channel to a Film before initialization");

	channels.insert(type);
	switch (type) {
		case MATERIAL_ID_MASK: {
			const u_int id = prop->Get(Property("id")(255)).Get<u_int>();
			if (count(maskMaterialIDs.begin(), maskMaterialIDs.end(), id) == 0)
				maskMaterialIDs.push_back(id);
			break;
		}
		case BY_MATERIAL_ID: {
			const u_int id = prop->Get(Property("id")(255)).Get<u_int>();
			if (count(byMaterialIDs.begin(), byMaterialIDs.end(), id) == 0)
				byMaterialIDs.push_back(id);
			break;
		}
		default:
			break;
	}
}

void Film::RemoveChannel(const FilmChannelType type) {
	if (initialized)
		throw runtime_error("It is only possible to remove a channel from a Film before initialization");

	channels.erase(type);
}

void Film::Init() {
	if (initialized)
		throw runtime_error("A Film can not be initialized multiple times");

	initialized = true;

	Resize(width, height);
}

void Film::Resize(const u_int w, const u_int h) {
	width = w;
	height = h;
	pixelCount = w * h;

	delete convTest;
	convTest = NULL;

	// Delete all already allocated channels
	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size(); ++i)
		delete channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i];
	channel_RADIANCE_PER_PIXEL_NORMALIZEDs.clear();
	for (u_int i = 0; i < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i)
		delete channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i];
	channel_RADIANCE_PER_SCREEN_NORMALIZEDs.clear();
	delete channel_ALPHA;
	delete channel_RGB_TONEMAPPED;
	delete channel_DEPTH;
	delete channel_POSITION;
	delete channel_GEOMETRY_NORMAL;
	delete channel_SHADING_NORMAL;
	delete channel_MATERIAL_ID;
	delete channel_DIRECT_DIFFUSE;
	delete channel_DIRECT_GLOSSY;
	delete channel_EMISSION;
	delete channel_INDIRECT_DIFFUSE;
	delete channel_INDIRECT_GLOSSY;
	delete channel_INDIRECT_SPECULAR;
	for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i)
		delete channel_MATERIAL_ID_MASKs[i];
	channel_MATERIAL_ID_MASKs.clear();
	delete channel_DIRECT_SHADOW_MASK;
	delete channel_INDIRECT_SHADOW_MASK;
	delete channel_UV;
	delete channel_RAYCOUNT;
	for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i)
		delete channel_BY_MATERIAL_IDs[i];
	channel_BY_MATERIAL_IDs.clear();
	delete channel_IRRADIANCE;

	// Allocate all required channels
	hasDataChannel = false;
	hasComposingChannel = false;
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		channel_RADIANCE_PER_PIXEL_NORMALIZEDs.resize(radianceGroupCount, NULL);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i] = new GenericFrameBuffer<4, 1, float>(width, height);
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
		}		
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		channel_RADIANCE_PER_SCREEN_NORMALIZEDs.resize(radianceGroupCount, NULL);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i] = new GenericFrameBuffer<3, 0, float>(width, height);
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->Clear();
		}
	}
	radianceChannelScales.resize(radianceGroupCount);

	if (HasChannel(ALPHA)) {
		channel_ALPHA = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_ALPHA->Clear();
	}
	if (HasChannel(RGB_TONEMAPPED)) {
		channel_RGB_TONEMAPPED = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_RGB_TONEMAPPED->Clear();

		convTest = new ConvergenceTest(width, height);
	}
	if (HasChannel(DEPTH)) {
		channel_DEPTH = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_DEPTH->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(POSITION)) {
		channel_POSITION = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_POSITION->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(GEOMETRY_NORMAL)) {
		channel_GEOMETRY_NORMAL = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_GEOMETRY_NORMAL->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(SHADING_NORMAL)) {
		channel_SHADING_NORMAL = new GenericFrameBuffer<3, 0, float>(width, height);
		channel_SHADING_NORMAL->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(MATERIAL_ID)) {
		channel_MATERIAL_ID = new GenericFrameBuffer<1, 0, u_int>(width, height);
		channel_MATERIAL_ID->Clear(numeric_limits<u_int>::max());
		hasDataChannel = true;
	}
	if (HasChannel(DIRECT_DIFFUSE)) {
		channel_DIRECT_DIFFUSE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_GLOSSY)) {
		channel_DIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_DIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(EMISSION)) {
		channel_EMISSION = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_EMISSION->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_DIFFUSE)) {
		channel_INDIRECT_DIFFUSE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_GLOSSY)) {
		channel_INDIRECT_GLOSSY = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR)) {
		channel_INDIRECT_SPECULAR = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_INDIRECT_SPECULAR->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
			GenericFrameBuffer<2, 1, float> *buf = new GenericFrameBuffer<2, 1, float>(width, height);
			buf->Clear();
			channel_MATERIAL_ID_MASKs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_SHADOW_MASK)) {
		channel_DIRECT_SHADOW_MASK = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_DIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SHADOW_MASK)) {
		channel_INDIRECT_SHADOW_MASK = new GenericFrameBuffer<2, 1, float>(width, height);
		channel_INDIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(UV)) {
		channel_UV = new GenericFrameBuffer<2, 0, float>(width, height);
		channel_UV->Clear(numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(RAYCOUNT)) {
		channel_RAYCOUNT = new GenericFrameBuffer<1, 0, float>(width, height);
		channel_RAYCOUNT->Clear();
		hasDataChannel = true;
	}
	if (HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < byMaterialIDs.size(); ++i) {
			GenericFrameBuffer<4, 1, float> *buf = new GenericFrameBuffer<4, 1, float>(width, height);
			buf->Clear();
			channel_BY_MATERIAL_IDs.push_back(buf);
		}
		hasComposingChannel = true;
	}
	if (HasChannel(IRRADIANCE)) {
		channel_IRRADIANCE = new GenericFrameBuffer<4, 1, float>(width, height);
		channel_IRRADIANCE->Clear();
		hasComposingChannel = true;
	}

	// Initialize the stats
	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::SetRadianceChannelScale(const u_int index, const RadianceChannelScale &scale) {
	radianceChannelScales.resize(Max<size_t>(radianceChannelScales.size(), index + 1));

	radianceChannelScales[index] = scale;
	radianceChannelScales[index].Init();
}

void Film::Reset() {
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i)
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i)
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->Clear();
	}
	if (HasChannel(ALPHA))
		channel_ALPHA->Clear();
	if (HasChannel(DEPTH))
		channel_DEPTH->Clear(numeric_limits<float>::infinity());
	if (HasChannel(POSITION))
		channel_POSITION->Clear(numeric_limits<float>::infinity());
	if (HasChannel(GEOMETRY_NORMAL))
		channel_GEOMETRY_NORMAL->Clear(numeric_limits<float>::infinity());
	if (HasChannel(SHADING_NORMAL))
		channel_SHADING_NORMAL->Clear(numeric_limits<float>::infinity());
	if (HasChannel(MATERIAL_ID))
		channel_MATERIAL_ID->Clear(numeric_limits<float>::max());
	if (HasChannel(DIRECT_DIFFUSE))
		channel_DIRECT_DIFFUSE->Clear();
	if (HasChannel(DIRECT_GLOSSY))
		channel_DIRECT_GLOSSY->Clear();
	if (HasChannel(EMISSION))
		channel_EMISSION->Clear();
	if (HasChannel(INDIRECT_DIFFUSE))
		channel_INDIRECT_DIFFUSE->Clear();
	if (HasChannel(INDIRECT_GLOSSY))
		channel_INDIRECT_GLOSSY->Clear();
	if (HasChannel(INDIRECT_SPECULAR))
		channel_INDIRECT_SPECULAR->Clear();
	if (HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i)
			channel_MATERIAL_ID_MASKs[i]->Clear();
	}
	if (HasChannel(DIRECT_SHADOW_MASK))
		channel_DIRECT_SHADOW_MASK->Clear();
	if (HasChannel(INDIRECT_SHADOW_MASK))
		channel_INDIRECT_SHADOW_MASK->Clear();
	if (HasChannel(UV))
		channel_UV->Clear();
	if (HasChannel(RAYCOUNT))
		channel_RAYCOUNT->Clear();
	if (HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i)
			channel_BY_MATERIAL_IDs[i]->Clear();
	}
	if (HasChannel(IRRADIANCE))
		channel_IRRADIANCE->Clear();

	// convTest has to be reset explicitly

	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::VarianceClampFilm(const VarianceClamping &varianceClamping,
		const Film &film, const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					const float *dstPixel = channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(dstOffsetX + x, dstOffsetY + y);

					varianceClamping.Clamp(dstPixel, srcPixel);
				}
			}
		}
	}
}

void Film::AddFilm(const Film &film,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	statsTotalSampleCount += film.statsTotalSampleCount;

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && film.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED) && film.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(radianceGroupCount, film.radianceGroupCount); ++i) {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(ALPHA) && film.HasChannel(ALPHA)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_ALPHA->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_ALPHA->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(POSITION) && film.HasChannel(POSITION)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_POSITION->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_POSITION->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(GEOMETRY_NORMAL) && film.HasChannel(GEOMETRY_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_GEOMETRY_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_GEOMETRY_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(SHADING_NORMAL) && film.HasChannel(SHADING_NORMAL)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_SHADING_NORMAL->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_SHADING_NORMAL->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(MATERIAL_ID) && film.HasChannel(MATERIAL_ID)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const u_int *srcPixel = film.channel_MATERIAL_ID->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_MATERIAL_ID->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(DIRECT_DIFFUSE) && film.HasChannel(DIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(DIRECT_GLOSSY) && film.HasChannel(DIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(EMISSION) && film.HasChannel(EMISSION)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_EMISSION->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_EMISSION->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_DIFFUSE) && film.HasChannel(INDIRECT_DIFFUSE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_DIFFUSE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_DIFFUSE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_GLOSSY) && film.HasChannel(INDIRECT_GLOSSY)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_GLOSSY->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_GLOSSY->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SPECULAR) && film.HasChannel(INDIRECT_SPECULAR)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SPECULAR->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_SPECULAR->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(MATERIAL_ID_MASK) && film.HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < channel_MATERIAL_ID_MASKs.size(); ++i) {
			for (u_int j = 0; j < film.maskMaterialIDs.size(); ++j) {
				if (maskMaterialIDs[i] == film.maskMaterialIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_MATERIAL_ID_MASKs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							channel_MATERIAL_ID_MASKs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(DIRECT_SHADOW_MASK) && film.HasChannel(DIRECT_SHADOW_MASK)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DIRECT_SHADOW_MASK->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DIRECT_SHADOW_MASK->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(INDIRECT_SHADOW_MASK) && film.HasChannel(INDIRECT_SHADOW_MASK)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_INDIRECT_SHADOW_MASK->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_INDIRECT_SHADOW_MASK->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(UV) && film.HasChannel(UV)) {
		if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
			// Used DEPTH information to merge Films
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					if (film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y)[0] < channel_DEPTH->GetPixel(dstOffsetX + x, dstOffsetY + y)[0]) {
						const float *srcPixel = film.channel_UV->GetPixel(srcOffsetX + x, srcOffsetY + y);
						channel_UV->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
					}
				}
			}
		} else {
			for (u_int y = 0; y < srcHeight; ++y) {
				for (u_int x = 0; x < srcWidth; ++x) {
					const float *srcPixel = film.channel_UV->GetPixel(srcOffsetX + x, srcOffsetY + y);
					channel_UV->SetPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
				}
			}
		}
	}

	if (HasChannel(RAYCOUNT) && film.HasChannel(RAYCOUNT)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_RAYCOUNT->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_RAYCOUNT->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	if (HasChannel(BY_MATERIAL_ID) && film.HasChannel(BY_MATERIAL_ID)) {
		for (u_int i = 0; i < channel_BY_MATERIAL_IDs.size(); ++i) {
			for (u_int j = 0; j < film.byMaterialIDs.size(); ++j) {
				if (byMaterialIDs[i] == film.byMaterialIDs[j]) {
					for (u_int y = 0; y < srcHeight; ++y) {
						for (u_int x = 0; x < srcWidth; ++x) {
							const float *srcPixel = film.channel_BY_MATERIAL_IDs[j]->GetPixel(srcOffsetX + x, srcOffsetY + y);
							channel_BY_MATERIAL_IDs[i]->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
						}
					}
				}
			}
		}
	}

	if (HasChannel(IRRADIANCE) && film.HasChannel(IRRADIANCE)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_IRRADIANCE->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_IRRADIANCE->AddPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}

	// NOTE: update DEPTH channel last because it is used to merge other channels
	if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DEPTH->MinPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}
}

u_int Film::GetChannelCount(const FilmChannelType type) const {
	switch (type) {
		case RADIANCE_PER_PIXEL_NORMALIZED:
			return channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size();
		case RADIANCE_PER_SCREEN_NORMALIZED:
			return channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size();
		case ALPHA:
			return channel_ALPHA ? 1 : 0;
		case RGB_TONEMAPPED:
			return channel_RGB_TONEMAPPED ? 1 : 0;
		case DEPTH:
			return channel_DEPTH ? 1 : 0;
		case POSITION:
			return channel_POSITION ? 1 : 0;
		case GEOMETRY_NORMAL:
			return channel_GEOMETRY_NORMAL ? 1 : 0;
		case SHADING_NORMAL:
			return channel_SHADING_NORMAL ? 1 : 0;
		case MATERIAL_ID:
			return channel_MATERIAL_ID ? 1 : 0;
		case DIRECT_DIFFUSE:
			return channel_DIRECT_DIFFUSE ? 1 : 0;
		case DIRECT_GLOSSY:
			return channel_DIRECT_GLOSSY ? 1 : 0;
		case EMISSION:
			return channel_EMISSION ? 1 : 0;
		case INDIRECT_DIFFUSE:
			return channel_INDIRECT_DIFFUSE ? 1 : 0;
		case INDIRECT_GLOSSY:
			return channel_INDIRECT_GLOSSY ? 1 : 0;
		case INDIRECT_SPECULAR:
			return channel_INDIRECT_SPECULAR ? 1 : 0;
		case MATERIAL_ID_MASK:
			return channel_MATERIAL_ID_MASKs.size();
		case DIRECT_SHADOW_MASK:
			return channel_DIRECT_SHADOW_MASK ? 1 : 0;
		case INDIRECT_SHADOW_MASK:
			return channel_INDIRECT_SHADOW_MASK ? 1 : 0;
		case UV:
			return channel_UV ? 1 : 0;
		case RAYCOUNT:
			return channel_RAYCOUNT ? 1 : 0;
		case BY_MATERIAL_ID:
			return channel_BY_MATERIAL_IDs.size();
		case IRRADIANCE:
			return channel_IRRADIANCE ? 1 : 0;
		default:
			throw runtime_error("Unknown FilmChannelType in Film::GetChannelCount>(): " + ToString(type));
	}
}

template<> const float *Film::GetChannel<float>(const FilmChannelType type, const u_int index) {
	switch (type) {
		case RADIANCE_PER_PIXEL_NORMALIZED:
			return channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->GetPixels();
		case RADIANCE_PER_SCREEN_NORMALIZED:
			return channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->GetPixels();
		case ALPHA:
			return channel_ALPHA->GetPixels();
		case RGB_TONEMAPPED: {
			ExecuteImagePipeline();
			return channel_RGB_TONEMAPPED->GetPixels();
		}
		case DEPTH:
			return channel_DEPTH->GetPixels();
		case POSITION:
			return channel_POSITION->GetPixels();
		case GEOMETRY_NORMAL:
			return channel_GEOMETRY_NORMAL->GetPixels();
		case SHADING_NORMAL:
			return channel_SHADING_NORMAL->GetPixels();
		case DIRECT_DIFFUSE:
			return channel_DIRECT_DIFFUSE->GetPixels();
		case DIRECT_GLOSSY:
			return channel_DIRECT_GLOSSY->GetPixels();
		case EMISSION:
			return channel_EMISSION->GetPixels();
		case INDIRECT_DIFFUSE:
			return channel_INDIRECT_DIFFUSE->GetPixels();
		case INDIRECT_GLOSSY:
			return channel_INDIRECT_GLOSSY->GetPixels();
		case INDIRECT_SPECULAR:
			return channel_INDIRECT_SPECULAR->GetPixels();
		case MATERIAL_ID_MASK:
			return channel_MATERIAL_ID_MASKs[index]->GetPixels();
		case DIRECT_SHADOW_MASK:
			return channel_DIRECT_SHADOW_MASK->GetPixels();
		case INDIRECT_SHADOW_MASK:
			return channel_INDIRECT_SHADOW_MASK->GetPixels();
		case UV:
			return channel_UV->GetPixels();
		case RAYCOUNT:
			return channel_RAYCOUNT->GetPixels();
		case BY_MATERIAL_ID:
			return channel_BY_MATERIAL_IDs[index]->GetPixels();
		case IRRADIANCE:
			return channel_IRRADIANCE->GetPixels();
		default:
			throw runtime_error("Unknown FilmChannelType in Film::GetChannel<float>(): " + ToString(type));
	}
}

template<> const u_int *Film::GetChannel<u_int>(const FilmChannelType type, const u_int index) {
	switch (type) {
		case MATERIAL_ID:
			return channel_MATERIAL_ID->GetPixels();
		default:
			throw runtime_error("Unknown FilmChannelType in Film::GetChannel<u_int>(): " + ToString(type));
	}
}

void Film::GetPixelFromMergedSampleBuffers(const u_int index, float *c) const {
	c[0] = 0.f;
	c[1] = 0.f;
	c[2] = 0.f;

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size(); ++i) {
		if (radianceChannelScales[i].enabled) {
			float v[3];
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetWeightedPixel(index, v);
			radianceChannelScales[i].Scale(v);

			c[0] += v[0];
			c[1] += v[1];
			c[2] += v[2];
		}
	}

	if (channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) {
		const float factor = statsTotalSampleCount / pixelCount;
		for (u_int i = 0; i < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i) {
			if (radianceChannelScales[i].enabled) {
				const float *src = channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(index);

				float v[3] = {
					factor * src[0],
					factor * src[1],
					factor * src[2]
				};
				radianceChannelScales[i].Scale(v);

				c[0] += v[0];
				c[1] += v[1];
				c[2] += v[2];
			}
		}
	}
}

void Film::ExecuteImagePipeline() {
	if (!rgbTonemapUpdate ||
			(!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) ||
			!HasChannel(RGB_TONEMAPPED)) {
		// Nothing to do
		return;
	}

	// Merge all buffers
	Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
	const u_int pixelCount = width * height;
	vector<bool> frameBufferMask(pixelCount, false);
	MergeSampleBuffers(p, frameBufferMask);

	// Apply the image pipeline if I have one
	if (imagePipeline)
		imagePipeline->Apply(*this, p, frameBufferMask);
}

void Film::MergeSampleBuffers(Spectrum *p, vector<bool> &frameBufferMask) const {
	const u_int pixelCount = width * height;

	// Merge RADIANCE_PER_PIXEL_NORMALIZED and RADIANCE_PER_SCREEN_NORMALIZED buffers

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (radianceChannelScales[i].enabled) {
				for (u_int j = 0; j < pixelCount; ++j) {
					const float *sp = channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(j);

					if (sp[3] > 0.f) {
						Spectrum s(sp);
						s /= sp[3];
						s = radianceChannelScales[i].Scale(s);

						if (frameBufferMask[j])
							p[j] += s;
						else
							p[j] = s;
						frameBufferMask[j] = true;
					}
				}
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		const float factor = pixelCount / statsTotalSampleCount;

		for (u_int i = 0; i < radianceGroupCount; ++i) {
			if (radianceChannelScales[i].enabled) {
				for (u_int j = 0; j < pixelCount; ++j) {
					Spectrum s(channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(j));

					if (!s.Black()) {
						s = factor * radianceChannelScales[i].Scale(s);

						if (frameBufferMask[j])
							p[j] += s;
						else
							p[j] = s;
						frameBufferMask[j] = true;
					}
				}
			}
		}
	}

	if (!enabledOverlappedScreenBufferUpdate) {
		for (u_int i = 0; i < pixelCount; ++i) {
			if (!frameBufferMask[i])
				p[i] = Spectrum();
		}
	}
}

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AddWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE))
			channel_DIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.directDiffuse.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY))
			channel_DIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.directGlossy.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AddWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE))
			channel_INDIRECT_DIFFUSE->AddWeightedPixel(x, y, sampleResult.indirectDiffuse.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY))
			channel_INDIRECT_GLOSSY->AddWeightedPixel(x, y, sampleResult.indirectGlossy.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR))
			channel_INDIRECT_SPECULAR->AddWeightedPixel(x, y, sampleResult.indirectSpecular.c, weight);

		// This is MATERIAL_ID_MASK and BY_MATERIAL_ID
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_MATERIAL_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min(sampleResult.radiance.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiance[i].IsNaN() || sampleResult.radiance[i].IsInf())
								continue;

							c += sampleResult.radiance[i];
						}
					}

					channel_BY_MATERIAL_IDs[index]->AddWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AddWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AddWeightedPixel(x, y, sampleResult.irradiance.c, weight);
	}
}

void Film::AddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH))
		depthWrite = channel_DEPTH->MinPixel(x, y, &sampleResult.depth);

	if (depthWrite) {
		// Faster than HasChannel(POSITION)
		if (channel_POSITION && sampleResult.HasChannel(POSITION))
			channel_POSITION->SetPixel(x, y, &sampleResult.position.x);

		// Faster than HasChannel(GEOMETRY_NORMAL)
		if (channel_GEOMETRY_NORMAL && sampleResult.HasChannel(GEOMETRY_NORMAL))
			channel_GEOMETRY_NORMAL->SetPixel(x, y, &sampleResult.geometryNormal.x);

		// Faster than HasChannel(SHADING_NORMAL)
		if (channel_SHADING_NORMAL && sampleResult.HasChannel(SHADING_NORMAL))
			channel_SHADING_NORMAL->SetPixel(x, y, &sampleResult.shadingNormal.x);

		// Faster than HasChannel(MATERIAL_ID)
		if (channel_MATERIAL_ID && sampleResult.HasChannel(MATERIAL_ID))
			channel_MATERIAL_ID->SetPixel(x, y, &sampleResult.materialID);

		// Faster than HasChannel(UV)
		if (channel_UV && sampleResult.HasChannel(UV))
			channel_UV->SetPixel(x, y, &sampleResult.uv.u);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AddPixel(x, y, &sampleResult.rayCount);
}

void Film::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AddSampleResultData(x, y, sampleResult);
}

void Film::ResetConvergenceTest() {
	if (convTest)
		convTest->Reset();
}

u_int Film::RunConvergenceTest() {
	// Required in order to have a valid convergence test
	ExecuteImagePipeline();

	return convTest->Test((const float *)channel_RGB_TONEMAPPED->GetPixels());
}

Film::FilmChannelType Film::String2FilmChannelType(const std::string &type) {
	if (type == "RADIANCE_PER_PIXEL_NORMALIZED")
		return RADIANCE_PER_PIXEL_NORMALIZED;
	else if (type == "RADIANCE_PER_SCREEN_NORMALIZED")
		return RADIANCE_PER_SCREEN_NORMALIZED;
	else if (type == "ALPHA")
		return ALPHA;
	else if (type == "DEPTH")
		return DEPTH;
	else if (type == "POSITION")
		return POSITION;
	else if (type == "GEOMETRY_NORMAL")
		return GEOMETRY_NORMAL;
	else if (type == "SHADING_NORMAL")
		return SHADING_NORMAL;
	else if (type == "MATERIAL_ID")
		return MATERIAL_ID;
	else if (type == "DIRECT_DIFFUSE")
		return DIRECT_DIFFUSE;
	else if (type == "DIRECT_GLOSSY")
		return DIRECT_GLOSSY;
	else if (type == "EMISSION")
		return EMISSION;
	else if (type == "INDIRECT_DIFFUSE")
		return INDIRECT_DIFFUSE;
	else if (type == "INDIRECT_GLOSSY")
		return INDIRECT_GLOSSY;
	else if (type == "INDIRECT_SPECULAR")
		return INDIRECT_SPECULAR;
	else if (type == "INDIRECT_SPECULAR")
		return INDIRECT_SPECULAR;
	else if (type == "MATERIAL_ID_MASK")
		return MATERIAL_ID_MASK;
	else if (type == "DIRECT_SHADOW_MASK")
		return DIRECT_SHADOW_MASK;
	else if (type == "INDIRECT_SHADOW_MASK")
		return INDIRECT_SHADOW_MASK;
	else if (type == "UV")
		return UV;
	else if (type == "RAYCOUNT")
		return RAYCOUNT;
	else if (type == "BY_MATERIAL_ID")
		return BY_MATERIAL_ID;
	else if (type == "IRRADIANCE")
		return IRRADIANCE;
	else
		throw runtime_error("Unknown film output type in Film::String2FilmChannelType(): " + type);
}

const std::string Film::FilmChannelType2String(const Film::FilmChannelType type) {
	switch (type) {
		case Film::RADIANCE_PER_PIXEL_NORMALIZED:
			return "RADIANCE_PER_PIXEL_NORMALIZED";
		case Film::RADIANCE_PER_SCREEN_NORMALIZED:
			return "RADIANCE_PER_SCREEN_NORMALIZED";
		case Film::ALPHA:
			return "ALPHA";
		case Film::DEPTH:
			return "DEPTH";
		case Film::POSITION:
			return "POSITION";
		case Film::GEOMETRY_NORMAL:
			return "GEOMETRY_NORMAL";
		case Film::SHADING_NORMAL:
			return "SHADING_NORMAL";
		case Film::MATERIAL_ID:
			return "MATERIAL_ID";
		case Film::DIRECT_DIFFUSE:
			return "DIRECT_DIFFUSE";
		case Film::DIRECT_GLOSSY:
			return "DIRECT_GLOSSY";
		case Film::EMISSION:
			return "EMISSION";
		case Film::INDIRECT_DIFFUSE:
			return "INDIRECT_DIFFUSE";
		case Film::INDIRECT_GLOSSY:
			return "INDIRECT_GLOSSY";
		case Film::INDIRECT_SPECULAR:
			return "INDIRECT_SPECULAR";
		case Film::MATERIAL_ID_MASK:
			return "MATERIAL_ID_MASK";
		case Film::DIRECT_SHADOW_MASK:
			return "DIRECT_SHADOW_MASK";
		case Film::INDIRECT_SHADOW_MASK:
			return "INDIRECT_SHADOW_MASK";
		case Film::UV:
			return "UV";
		case Film::RAYCOUNT:
			return "RAYCOUNT";
		case Film::BY_MATERIAL_ID:
			return "BY_MATERIAL_ID";
		case Film::IRRADIANCE:
			return "IRRADIANCE";
		default:
			throw runtime_error("Unknown film output type in Film::FilmChannelType2String(): " + ToString(type));
	}
}
