/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/film/film.h"
#include "luxrays/core/geometry/point.h"
#include "slg/editaction.h"
#include "luxrays/utils/properties.h"

using namespace std;
using namespace luxrays;
using namespace slg;

typedef unsigned char BYTE;

//------------------------------------------------------------------------------
// FilmOutput
//------------------------------------------------------------------------------

void FilmOutputs::Add(const FilmOutputType type, const string &fileName,
		const luxrays::Properties *p) {
	types.push_back(type);
	fileNames.push_back(fileName);
	if (p)
		props.push_back(*p);
	else
		props.push_back(Properties());
}

//------------------------------------------------------------------------------
// Film
//------------------------------------------------------------------------------

Film::Film(const u_int w, const u_int h) {
	initialized = false;

	width = w;
	height = h;
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

	convTest = NULL;

	enabledOverlappedScreenBufferUpdate = true;
	rgbTonemapUpdate = true;

	imagePipeline = NULL;
	filter = NULL;
	filterLUTs = NULL;
	SetFilter(new GaussianFilter(1.5f, 1.5f, 2.f));
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

	delete filterLUTs;
	delete filter;
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

	// Initialize the stats
	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::SetFilter(Filter *flt) {
	delete filterLUTs;
	filterLUTs = NULL;
	delete filter;
	filter = flt;

	if (filter) {
		const u_int size = Max<u_int>(4, Max(filter->xWidth, filter->yWidth) + 1);
		filterLUTs = new FilterLUTs(*filter, size);
	}
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

	// convTest has to be reset explicitly

	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
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

bool Film::HasOutput(const FilmOutputs::FilmOutputType type) const {
	switch (type) {
		case FilmOutputs::RGB:
			return HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) || HasChannel(RADIANCE_PER_SCREEN_NORMALIZED);
		case FilmOutputs::RGB_TONEMAPPED:
			return HasChannel(RGB_TONEMAPPED);
		case FilmOutputs::RGBA:
			return (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) || HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) && HasChannel(ALPHA);
		case FilmOutputs::RGBA_TONEMAPPED:
			return HasChannel(RGB_TONEMAPPED) && HasChannel(ALPHA);
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
		case FilmOutputs::DIRECT_GLOSSY:
			return HasChannel(DIRECT_GLOSSY);
		case FilmOutputs::EMISSION:
			return HasChannel(EMISSION);
		case FilmOutputs::INDIRECT_DIFFUSE:
			return HasChannel(INDIRECT_DIFFUSE);
		case FilmOutputs::INDIRECT_GLOSSY:
			return HasChannel(INDIRECT_GLOSSY);
		case FilmOutputs::INDIRECT_SPECULAR:
			return HasChannel(INDIRECT_SPECULAR);
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
		default:
			throw runtime_error("Unknown film output type in Film::HasOutput(): " + ToString(type));
	}
}

void Film::Output(const FilmOutputs &filmOutputs) {
	for (u_int i = 0; i < filmOutputs.GetCount(); ++i)
		Output(filmOutputs.GetType(i), filmOutputs.GetFileName(i), &filmOutputs.GetProperties(i));
}

void Film::Output(const FilmOutputs::FilmOutputType type, const string &fileName,
		const Properties *props) { 
	u_int maskMaterialIDsIndex = 0;
	u_int byMaterialIDsIndex = 0;
	u_int radianceGroupIndex = 0;
	u_int channelCount = 3;

	switch (type) {
		case FilmOutputs::RGB:
			if (!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED))
				return;
			break;
		case FilmOutputs::RGB_TONEMAPPED:
			if (!HasChannel(RGB_TONEMAPPED))
				return;
			ExecuteImagePipeline();
			break;
		case FilmOutputs::RGBA:
			if ((!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) || !HasChannel(ALPHA))
				return;
			channelCount = 4;
			break;
		case FilmOutputs::RGBA_TONEMAPPED:
			if (!HasChannel(RGB_TONEMAPPED) || !HasChannel(ALPHA))
				return;
			ExecuteImagePipeline();
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
		case FilmOutputs::DIRECT_GLOSSY:
			if (!HasChannel(DIRECT_GLOSSY))
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
		case FilmOutputs::INDIRECT_GLOSSY:
			if (!HasChannel(INDIRECT_GLOSSY))
				return;
			break;
		case FilmOutputs::INDIRECT_SPECULAR:
			if (!HasChannel(INDIRECT_SPECULAR))
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
		default:
			throw runtime_error("Unknown film output type in Film::Output(): " + ToString(type));
	}

	ImageBuf buffer;
	
	SLG_LOG("Outputting film: " << fileName << " type: " << ToString(type));

	if (type == FilmOutputs::MATERIAL_ID) {
		// For material IDs we must copy into int buffer first or risk screwing up the ID
		ImageSpec spec(width, height, channelCount, TypeDesc::UINT8);
		buffer.reset(spec);
		for (ImageBuf::ConstIterator<BYTE> it(buffer); !it.done(); ++it) {
			u_int x = it.x();
			u_int y = it.y();
			BYTE *pixel = (BYTE *)buffer.pixeladdr(x, y, 0);
			y = height - y - 1;
			
			if (pixel == NULL)
				throw runtime_error("Error while unpacking film data, could not address buffer!");
			
			const u_int *src = channel_MATERIAL_ID->GetPixel(x, y);
			pixel[0] = (BYTE)src[0];
			pixel[1] = (BYTE)src[1];
			pixel[2] = (BYTE)src[2];
		}
	} else {
		// OIIO 1 channel EXR output is apparently not working, I write 3 channels as
		// temporary workaround

		// For all others copy into float buffer first and let OIIO figure out the conversion on write
		ImageSpec spec(width, height, (channelCount == 1) ? 3 : channelCount, TypeDesc::FLOAT);
		buffer.reset(spec);
	
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
					GetPixelFromMergedSampleBuffers(x, y, pixel);
					break;
				}
				case FilmOutputs::RGB_TONEMAPPED: {
					channel_RGB_TONEMAPPED->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::RGBA: {
					// Accumulate all light groups
					GetPixelFromMergedSampleBuffers(x, y, pixel);
					channel_ALPHA->GetWeightedPixel(x, y, &pixel[3]);
					break;
				}
				case FilmOutputs::RGBA_TONEMAPPED: {
					channel_RGB_TONEMAPPED->GetWeightedPixel(x, y, pixel);
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
				case FilmOutputs::DIRECT_GLOSSY: {
					channel_DIRECT_GLOSSY->GetWeightedPixel(x, y, pixel);
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
				case FilmOutputs::INDIRECT_GLOSSY: {
					channel_INDIRECT_GLOSSY->GetWeightedPixel(x, y, pixel);
					break;
				}
				case FilmOutputs::INDIRECT_SPECULAR: {
					channel_INDIRECT_SPECULAR->GetWeightedPixel(x, y, pixel);
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
					// Accumulate all light groups
					if (radianceGroupIndex < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size())
						channel_RADIANCE_PER_PIXEL_NORMALIZEDs[radianceGroupIndex]->AccumulateWeightedPixel(x, y, pixel);
					if (radianceGroupIndex < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size())
						channel_RADIANCE_PER_SCREEN_NORMALIZEDs[radianceGroupIndex]->AccumulateWeightedPixel(x, y, pixel);
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
	
	buffer.write(fileName);
}

template<> void Film::GetOutput<float>(const FilmOutputs::FilmOutputType type, float *buffer, const u_int index) {
	switch (type) {
		case FilmOutputs::RGB: {
			for (u_int i = 0; i < pixelCount; ++i)
				GetPixelFromMergedSampleBuffers(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::RGB_TONEMAPPED:
			ExecuteImagePipeline();

			copy(channel_RGB_TONEMAPPED->GetPixels(), channel_RGB_TONEMAPPED->GetPixels() + pixelCount * 3, buffer);
			break;
		case FilmOutputs::RGBA: {
			for (u_int i = 0; i < pixelCount; ++i) {
				const u_int offset = i * 4;
				GetPixelFromMergedSampleBuffers(i, &buffer[offset]);
				channel_ALPHA->GetWeightedPixel(i, &buffer[offset + 3]);
			}
			break;
		}
		case FilmOutputs::RGBA_TONEMAPPED: {
			ExecuteImagePipeline();

			float *srcRGB = channel_RGB_TONEMAPPED->GetPixels();
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
		case FilmOutputs::DIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_DIRECT_GLOSSY->GetWeightedPixel(i, &buffer[i * 3]);
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
		case FilmOutputs::INDIRECT_GLOSSY: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_GLOSSY->GetWeightedPixel(i, &buffer[i * 3]);
			break;
		}
		case FilmOutputs::INDIRECT_SPECULAR: {
			for (u_int i = 0; i < pixelCount; ++i)
				channel_INDIRECT_SPECULAR->GetWeightedPixel(i, &buffer[i * 3]);
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
			float *dst = buffer;
			for (u_int i = 0; i < pixelCount; ++i) {
				Spectrum c;
				channel_RADIANCE_PER_PIXEL_NORMALIZEDs[index]->AccumulateWeightedPixel(i, c.c);
				channel_RADIANCE_PER_SCREEN_NORMALIZEDs[index]->AccumulateWeightedPixel(i, c.c);

				*dst++ = c.c[0];
				*dst++ = c.c[1];
				*dst++ = c.c[2];
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
		default:
			throw runtime_error("Unknown film output type in Film::GetOutput<float>(): " + ToString(type));
	}
}

template<> void Film::GetOutput<u_int>(const FilmOutputs::FilmOutputType type, u_int *buffer, const u_int index) {
	switch (type) {
		case FilmOutputs::MATERIAL_ID:
			copy(channel_MATERIAL_ID->GetPixels(), channel_MATERIAL_ID->GetPixels() + pixelCount, buffer);
			break;
		default:
			throw runtime_error("Unknown film output type in Film::GetOutput<u_int>(): " + ToString(type));
	}
}

void Film::GetPixelFromMergedSampleBuffers(const u_int index, float *c) const {
	c[0] = 0.f;
	c[1] = 0.f;
	c[2] = 0.f;

	for (u_int i = 0; i < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size(); ++i)
		channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AccumulateWeightedPixel(index, c);

	if (channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) {
		const float factor = statsTotalSampleCount / pixelCount;
		for (u_int i = 0; i < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size(); ++i) {
			const float *src = channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(index);

			c[0] += src[0] * factor;
			c[1] += src[1] * factor;
			c[2] += src[2] * factor;
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

	// apply the image pipeline if I have one
	if (imagePipeline)
		imagePipeline->Apply(*this, p, frameBufferMask);
}

void Film::MergeSampleBuffers(Spectrum *p, vector<bool> &frameBufferMask) const {
	const u_int pixelCount = width * height;

	// Merge RADIANCE_PER_PIXEL_NORMALIZED and RADIANCE_PER_SCREEN_NORMALIZED buffers

	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			for (u_int j = 0; j < pixelCount; ++j) {
				const float *sp = channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->GetPixel(j);

				if (sp[3] > 0.f) {
					if (frameBufferMask[j])
						p[j] += Spectrum(sp) / sp[3];
					else
						p[j] = Spectrum(sp) / sp[3];
					frameBufferMask[j] = true;
				}
			}
		}
	}

	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		const float factor = pixelCount / statsTotalSampleCount;

		for (u_int i = 0; i < radianceGroupCount; ++i) {
			for (u_int j = 0; j < pixelCount; ++j) {
				const Spectrum s(channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(j));

				if (!s.Black()) {
					if (frameBufferMask[j])
						p[j] += s * factor;
					else
						p[j] = s * factor;
					frameBufferMask[j] = true;
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
		for (u_int i = 0; i < Min(sampleResult.radiancePerPixelNormalized.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiancePerPixelNormalized[i].IsNaN() || sampleResult.radiancePerPixelNormalized[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiancePerPixelNormalized[i].c, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiancePerScreenNormalized.size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiancePerScreenNormalized[i].IsNaN() || sampleResult.radiancePerScreenNormalized[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddWeightedPixel(x, y, sampleResult.radiancePerScreenNormalized[i].c, weight);
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

		// This is MATERIAL_ID_MASK and BY_MATERIAL_MASK
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_MATERIAL_MASK
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min(sampleResult.radiancePerPixelNormalized.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
							if (sampleResult.radiancePerPixelNormalized[i].IsNaN() || sampleResult.radiancePerPixelNormalized[i].IsInf())
								continue;

							c += sampleResult.radiancePerPixelNormalized[i];
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

void Film::SplatSample(const SampleResult &sampleResult, const float weight) {
	if (!filter) {
		const int x = Ceil2Int(sampleResult.filmX - .5f);
		const int y = Ceil2Int(sampleResult.filmY - .5f);

		if ((x >= 0) && (x < (int)width) && (y >= 0) && (y < (int)height)) {
			AddSampleResultColor(x, y, sampleResult, weight);
			if (hasDataChannel)
				AddSampleResultData(x, y, sampleResult);
		}
	} else {
		//----------------------------------------------------------------------
		// Add all data related information (not filtered)
		//----------------------------------------------------------------------

		if (hasDataChannel) {
			const int x = Ceil2Int(sampleResult.filmX - .5f);
			const int y = Ceil2Int(sampleResult.filmY - .5f);

			if ((x >= 0.f) && (x < (int)width) && (y >= 0.f) && (y < (int)height))
				AddSampleResultData(x, y, sampleResult);
		}

		//----------------------------------------------------------------------
		// Add all color related information (filtered)
		//----------------------------------------------------------------------

		// Compute sample's raster extent
		const float dImageX = sampleResult.filmX - .5f;
		const float dImageY = sampleResult.filmY - .5f;
		const FilterLUT *filterLUT = filterLUTs->GetLUT(dImageX - floorf(sampleResult.filmX), dImageY - floorf(sampleResult.filmY));
		const float *lut = filterLUT->GetLUT();

		const int x0 = Ceil2Int(dImageX - filter->xWidth);
		const int x1 = x0 + filterLUT->GetWidth();
		const int y0 = Ceil2Int(dImageY - filter->yWidth);
		const int y1 = y0 + filterLUT->GetHeight();

		for (int iy = y0; iy < y1; ++iy) {
			if (iy < 0) {
				lut += filterLUT->GetWidth();
				continue;
			} else if(iy >= (int)height)
				break;

			for (int ix = x0; ix < x1; ++ix) {
				const float filterWeight = *lut++;

				if ((ix < 0) || (ix >= (int)width))
					continue;

				const float filteredWeight = weight * filterWeight;
				AddSampleResultColor(ix, iy, sampleResult, filteredWeight);
			}
		}
	}
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
		default:
			throw runtime_error("Unknown film output type in Film::FilmChannelType2String(): " + ToString(type));
	}
}

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

void SampleResult::AddEmission(const u_int lightID, const Spectrum &radiance) {
	radiancePerPixelNormalized[lightID] += radiance;

	if (firstPathVertex)
		emission += radiance;
	else {
		indirectShadowMask = 0.f;

		if (firstPathVertexEvent & DIFFUSE)
			indirectDiffuse += radiance;
		else if (firstPathVertexEvent & GLOSSY)
			indirectGlossy += radiance;
		else if (firstPathVertexEvent & SPECULAR)
			indirectSpecular += radiance;
	}
}

void SampleResult::AddDirectLight(const u_int lightID, const BSDFEvent bsdfEvent,
		const luxrays::Spectrum &radiance, const float lightScale) {
	radiancePerPixelNormalized[lightID] += radiance;

	if (firstPathVertex) {
		// directShadowMask is supposed to be initialized to 1.0
		directShadowMask = Max(0.f, directShadowMask - lightScale);

		if (bsdfEvent & DIFFUSE)
			directDiffuse += radiance;
		else
			directGlossy += radiance;
	} else {
		// indirectShadowMask is supposed to be initialized to 1.0
		indirectShadowMask = Max(0.f, indirectShadowMask - lightScale);

		if (firstPathVertexEvent & DIFFUSE)
			indirectDiffuse += radiance;
		else if (firstPathVertexEvent & GLOSSY)
			indirectGlossy += radiance;
		else if (firstPathVertexEvent & SPECULAR)
			indirectSpecular += radiance;
	}
}

void SampleResult::AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const luxrays::Spectrum &radiancePPN,
	const float alpha) {
	assert(!radiancePPN.IsInf() || !radiancePPN.IsNaN());
	assert(!isinf(alpha) || !isnan(alpha));

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_PIXEL_NORMALIZED | Film::ALPHA, 1);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiancePerPixelNormalized[0] = radiancePPN;
	sampleResults[size].alpha = alpha;
}

void SampleResult::AddSampleResult(std::vector<SampleResult> &sampleResults,
	const float filmX, const float filmY,
	const luxrays::Spectrum &radiancePSN) {
	assert(!radiancePSN.IsInf() || !radiancePSN.IsNaN());

	const u_int size = sampleResults.size();
	sampleResults.resize(size + 1);

	sampleResults[size].Init(Film::RADIANCE_PER_SCREEN_NORMALIZED, 1);
	sampleResults[size].filmX = filmX;
	sampleResults[size].filmY = filmY;
	sampleResults[size].radiancePerScreenNormalized[0] = radiancePSN;
}
