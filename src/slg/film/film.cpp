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

#include <boost/lexical_cast.hpp>
#include <boost/foreach.hpp>

#include "slg/film/film.h"
#include "luxrays/core/geometry/point.h"
#include "slg/editaction.h"
#include "luxrays/utils/properties.h"

using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// FilmOutput
//------------------------------------------------------------------------------

void FilmOutputs::Add(const FilmOutputType type, const std::string &fileName,
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

	filter = NULL;
	filterLUTs = NULL;
	SetFilter(new GaussianFilter(1.5f, 1.5f, 2.f));

	toneMapParams = new LinearToneMapParams();

	SetGamma();
}

Film::~Film() {
	delete toneMapParams;

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

	delete filterLUTs;
	delete filter;
}

void Film::AddChannel(const FilmChannelType type, const Properties *prop) {
	if (initialized)
		throw std::runtime_error("it is possible to add a channel to a Film only before the initialization");

	channels.insert(type);
	switch (type) {
		case MATERIAL_ID_MASK: {
			const u_int id = prop->GetInt("id", 255);
			if (std::count(maskMaterialIDs.begin(), maskMaterialIDs.end(), id) == 0)
				maskMaterialIDs.push_back(id);
			break;
		}
		default:
			break;
	}
}

void Film::RemoveChannel(const FilmChannelType type) {
	if (initialized)
		throw std::runtime_error("it is possible to remove a channel of a Film only before the initialization");

	channels.erase(type);
}

void Film::Init() {
	if (initialized)
		throw std::runtime_error("A Film can not be initialized multiple times");

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

	// Allocate all required channels
	hasDataChannel = false;
	hasComposingChannel = false;
	if (HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		channel_RADIANCE_PER_PIXEL_NORMALIZEDs.resize(radianceGroupCount, NULL);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i] = new GenericFrameBuffer<4, float>(width, height);
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->Clear();
		}
	}
	if (HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		channel_RADIANCE_PER_SCREEN_NORMALIZEDs.resize(radianceGroupCount, NULL);
		for (u_int i = 0; i < radianceGroupCount; ++i) {
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i] = new GenericFrameBuffer<4, float>(width, height);
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->Clear();
		}
	}
	if (HasChannel(ALPHA)) {
		channel_ALPHA = new GenericFrameBuffer<2, float>(width, height);
		channel_ALPHA->Clear();
	}
	if (HasChannel(TONEMAPPED_FRAMEBUFFER)) {
		channel_RGB_TONEMAPPED = new GenericFrameBuffer<3, float>(width, height);
		channel_RGB_TONEMAPPED->Clear();

		convTest = new ConvergenceTest(width, height);
	}
	if (HasChannel(DEPTH)) {
		channel_DEPTH = new GenericFrameBuffer<1, float>(width, height);
		channel_DEPTH->Clear(std::numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(POSITION)) {
		channel_POSITION = new GenericFrameBuffer<3, float>(width, height);
		channel_POSITION->Clear(std::numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(GEOMETRY_NORMAL)) {
		channel_GEOMETRY_NORMAL = new GenericFrameBuffer<3, float>(width, height);
		channel_GEOMETRY_NORMAL->Clear(std::numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(SHADING_NORMAL)) {
		channel_SHADING_NORMAL = new GenericFrameBuffer<3, float>(width, height);
		channel_SHADING_NORMAL->Clear(std::numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(MATERIAL_ID)) {
		channel_MATERIAL_ID = new GenericFrameBuffer<1, u_int>(width, height);
		channel_MATERIAL_ID->Clear(std::numeric_limits<u_int>::max());
		hasDataChannel = true;
	}
	if (HasChannel(DIRECT_DIFFUSE)) {
		channel_DIRECT_DIFFUSE = new GenericFrameBuffer<4, float>(width, height);
		channel_DIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(DIRECT_GLOSSY)) {
		channel_DIRECT_GLOSSY = new GenericFrameBuffer<4, float>(width, height);
		channel_DIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(EMISSION)) {
		channel_EMISSION = new GenericFrameBuffer<4, float>(width, height);
		channel_EMISSION->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_DIFFUSE)) {
		channel_INDIRECT_DIFFUSE = new GenericFrameBuffer<4, float>(width, height);
		channel_INDIRECT_DIFFUSE->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_GLOSSY)) {
		channel_INDIRECT_GLOSSY = new GenericFrameBuffer<4, float>(width, height);
		channel_INDIRECT_GLOSSY->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SPECULAR)) {
		channel_INDIRECT_SPECULAR = new GenericFrameBuffer<4, float>(width, height);
		channel_INDIRECT_SPECULAR->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(MATERIAL_ID_MASK)) {
		for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
			GenericFrameBuffer<2, float> *buf = new GenericFrameBuffer<2, float>(width, height);
			buf->Clear();
			channel_MATERIAL_ID_MASKs.push_back(buf);
		}
		hasDataChannel = true;
	}
	if (HasChannel(DIRECT_SHADOW_MASK)) {
		channel_DIRECT_SHADOW_MASK = new GenericFrameBuffer<2, float>(width, height);
		channel_DIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(INDIRECT_SHADOW_MASK)) {
		channel_INDIRECT_SHADOW_MASK = new GenericFrameBuffer<2, float>(width, height);
		channel_INDIRECT_SHADOW_MASK->Clear();
		hasComposingChannel = true;
	}
	if (HasChannel(UV)) {
		channel_UV = new GenericFrameBuffer<2, float>(width, height);
		channel_UV->Clear(std::numeric_limits<float>::infinity());
		hasDataChannel = true;
	}
	if (HasChannel(RAYCOUNT)) {
		channel_RAYCOUNT = new GenericFrameBuffer<1, float>(width, height);
		channel_RAYCOUNT->Clear();
		hasDataChannel = true;
	}

	// Initialize the stats
	statsTotalSampleCount = 0.0;
	statsAvgSampleSec = 0.0;
	statsStartSampleTime = WallClockTime();
}

void Film::SetGamma(const float g) {
	gamma = g;

	float x = 0.f;
	const float dx = 1.f / GAMMA_TABLE_SIZE;
	for (u_int i = 0; i < GAMMA_TABLE_SIZE; ++i, x += dx)
		gammaTable[i] = powf(Clamp(x, 0.f, 1.f), 1.f / g);
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
		channel_DEPTH->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(POSITION))
		channel_POSITION->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(GEOMETRY_NORMAL))
		channel_GEOMETRY_NORMAL->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(SHADING_NORMAL))
		channel_SHADING_NORMAL->Clear(std::numeric_limits<float>::infinity());
	if (HasChannel(MATERIAL_ID))
		channel_MATERIAL_ID->Clear(std::numeric_limits<float>::max());
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

	// convTest has to be reseted explicitely

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

	// NOTE: update DEPTH channel as last because it is used to merge other channels
	if (HasChannel(DEPTH) && film.HasChannel(DEPTH)) {
		for (u_int y = 0; y < srcHeight; ++y) {
			for (u_int x = 0; x < srcWidth; ++x) {
				const float *srcPixel = film.channel_DEPTH->GetPixel(srcOffsetX + x, srcOffsetY + y);
				channel_DEPTH->MinPixel(dstOffsetX + x, dstOffsetY + y, srcPixel);
			}
		}
	}
}

void Film::Output(const FilmOutputs &filmOutputs) {
	for (u_int i = 0; i < filmOutputs.GetCount(); ++i)
		Output(filmOutputs.GetType(i), filmOutputs.GetFileName(i), &filmOutputs.GetProperties(i));
}

void Film::Output(const FilmOutputs::FilmOutputType type, const std::string &fileName,
		const Properties *props) {
	// Image format
	FREE_IMAGE_FORMAT fif = FREEIMAGE_GETFIFFROMFILENAME(FREEIMAGE_CONVFILENAME(fileName).c_str());
	if (fif == FIF_UNKNOWN)
		throw std::runtime_error("Image type unknown");

	// HDR image or not
	const bool hdrImage = ((fif == FIF_HDR) || (fif == FIF_EXR));

	// Image type and bit count
	FREE_IMAGE_TYPE imageType;
	u_int bitCount;
	u_int maskMaterialIDsIndex = 0;
	u_int radianceGroupIndex = 0;
	switch (type) {
		case FilmOutputs::RGB:
			if (!hdrImage)
				return;
			imageType = FIT_RGBF;
			bitCount = 96;
			break;
		case FilmOutputs::RGB_TONEMAPPED:
			imageType = hdrImage ? FIT_RGBF : FIT_BITMAP;
			bitCount = hdrImage ? 96 : 24;
			break;
		case FilmOutputs::RGBA:
			if (!hdrImage)
				return;
			imageType = FIT_RGBAF;
			bitCount = 128;
			break;
		case FilmOutputs::RGBA_TONEMAPPED:
			imageType = hdrImage ? FIT_RGBAF : FIT_BITMAP;
			bitCount = hdrImage ? 128 : 32;
			break;
		case FilmOutputs::ALPHA:
			if (HasChannel(ALPHA)) {
				imageType = hdrImage ? FIT_FLOAT : FIT_BITMAP;
				bitCount = hdrImage ? 32 : 8;
			} else
				return;
			break;
		case FilmOutputs::DEPTH:
			if (HasChannel(DEPTH) && hdrImage) {
				imageType = FIT_FLOAT;
				bitCount = 32;
			} else
				return;
			break;
		case FilmOutputs::POSITION:
			if (HasChannel(POSITION) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::GEOMETRY_NORMAL:
			if (HasChannel(GEOMETRY_NORMAL) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::SHADING_NORMAL:
			if (HasChannel(SHADING_NORMAL) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::MATERIAL_ID:
			if (HasChannel(MATERIAL_ID) && !hdrImage) {
				imageType = FIT_BITMAP;
				bitCount = 24;
			} else
				return;
			break;
		case FilmOutputs::DIRECT_DIFFUSE:
			if (HasChannel(DIRECT_DIFFUSE) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::DIRECT_GLOSSY:
			if (HasChannel(DIRECT_GLOSSY) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::EMISSION:
			if (HasChannel(EMISSION) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::INDIRECT_DIFFUSE:
			if (HasChannel(INDIRECT_DIFFUSE) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::INDIRECT_GLOSSY:
			if (HasChannel(INDIRECT_GLOSSY) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::INDIRECT_SPECULAR:
			if (HasChannel(INDIRECT_SPECULAR) && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::MATERIAL_ID_MASK:
			if (HasChannel(MATERIAL_ID_MASK) && props) {
				imageType = hdrImage ? FIT_FLOAT : FIT_BITMAP;
				bitCount = hdrImage ? 32 : 8;

				// Look for the material mask ID index
				const u_int id = props->GetInt("id", 255);
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
			if (HasChannel(DIRECT_SHADOW_MASK)) {
				imageType = hdrImage ? FIT_FLOAT : FIT_BITMAP;
				bitCount = hdrImage ? 32 : 8;
			} else
				return;
			break;
		case FilmOutputs::INDIRECT_SHADOW_MASK:
			if (HasChannel(INDIRECT_SHADOW_MASK)) {
				imageType = hdrImage ? FIT_FLOAT : FIT_BITMAP;
				bitCount = hdrImage ? 32 : 8;
			} else
				return;
			break;
		case FilmOutputs::RADIANCE_GROUP:
			if (props && hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;

				radianceGroupIndex = props->GetInt("id", 0);
				if (radianceGroupIndex >= radianceGroupCount)
					return;
			} else
				return;
			break;
		case FilmOutputs::UV:
			if (hdrImage) {
				imageType = FIT_RGBF;
				bitCount = 96;
			} else
				return;
			break;
		case FilmOutputs::RAYCOUNT:
			if (hdrImage) {
				imageType = FIT_FLOAT;
				bitCount = 32;
			} else
				return;
			break;
		default:
			throw std::runtime_error("Unknown film output type");
	}

	if ((type == FilmOutputs::RGB) || (type == FilmOutputs::RGBA)) {
		// In order to merge the all radiance buffers
		UpdateScreenBufferImpl(TONEMAP_NONE);
	}

	// Allocate the image
	FIBITMAP *dib = FreeImage_AllocateT(imageType, width, height, bitCount);
	if (!dib)
		throw std::runtime_error("Unable to allocate FreeImage image");

	// Build the image
	u_int pitch = FreeImage_GetPitch(dib);
	BYTE *bits = (BYTE *)FreeImage_GetBits(dib);
	for (u_int y = 0; y < height; ++y) {
		for (u_int x = 0; x < width; ++x) {
			switch (type) {
				case FilmOutputs::RGB: {
					FIRGBF *dst = (FIRGBF *)bits;
					// channel_RGB_TONEMAPPED has the _untonemapped_ version of
					// the image in the case of an HDR because of the call
					// to UpdateScreenBufferImpl(TONEMAP_NONE)
					const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::RGB_TONEMAPPED: {
					if (hdrImage) {
						FIRGBF *dst = (FIRGBF *)bits;
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[x].red = src[0];
						dst[x].green = src[1];
						dst[x].blue = src[2];
					} else {
						BYTE *dst = &bits[x * 3];
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[FI_RGBA_RED] = (BYTE)(src[0] * 255.f + .5f);
						dst[FI_RGBA_GREEN] = (BYTE)(src[1] * 255.f + .5f);
						dst[FI_RGBA_BLUE] = (BYTE)(src[2] * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::RGBA: {
					FIRGBAF *dst = (FIRGBAF *)bits;
					// channel_RGB_TONEMAPPED has the _untonemapped_ version of
					// the image in the case of an HDR because of the call
					// to UpdateScreenBufferImpl(TONEMAP_NONE)
					const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];

					const float *alphaData = channel_ALPHA->GetPixel(x, y);
					if (alphaData[1] == 0.f)
						dst[x].alpha = 0.f;
					else
						dst[x].alpha = alphaData[0] / alphaData[1];
					break;
				}
				case FilmOutputs::RGBA_TONEMAPPED: {
					if (hdrImage) {
						FIRGBAF *dst = (FIRGBAF *)bits;
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[x].red = src[0];
						dst[x].green = src[1];
						dst[x].blue = src[2];

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[x].alpha = 0.f;
						else
							dst[x].alpha = alphaData[0] / alphaData[1];
					} else {
						BYTE *dst = &bits[x * 4];
						const float *src = channel_RGB_TONEMAPPED->GetPixel(x, y);
						dst[FI_RGBA_RED] = (BYTE)(src[0] * 255.f + .5f);
						dst[FI_RGBA_GREEN] = (BYTE)(src[1] * 255.f + .5f);
						dst[FI_RGBA_BLUE] = (BYTE)(src[2] * 255.f + .5f);

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[FI_RGBA_ALPHA] = 0;
						else
							dst[FI_RGBA_ALPHA] = (BYTE)((alphaData[0] / alphaData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::ALPHA: {
					if (hdrImage) {
						float *dst = (float *)bits;

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							dst[x] = 0.f;
						else
							dst[x] = alphaData[0] / alphaData[1];
					} else {
						BYTE *dst = &bits[x];

						const float *alphaData = channel_ALPHA->GetPixel(x, y);
						if (alphaData[1] == 0.f)
							*dst = 0;
						else
							*dst = (BYTE)((alphaData[0] / alphaData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::DEPTH: {
					float *dst = (float *)bits;

					dst[x] = channel_DEPTH->GetPixel(x, y)[0];
					break;
				}
				case FilmOutputs::POSITION: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_POSITION->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::GEOMETRY_NORMAL: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_GEOMETRY_NORMAL->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::SHADING_NORMAL: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_SHADING_NORMAL->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = src[2];
					break;
				}
				case FilmOutputs::MATERIAL_ID: {
					BYTE *dst = &bits[x * 3];
					const u_int *src = channel_MATERIAL_ID->GetPixel(x, y);
					dst[FI_RGBA_RED] = (BYTE)(src[0] & 0xff);
					dst[FI_RGBA_GREEN] = (BYTE)((src[0] & 0xff00) >> 8);
					dst[FI_RGBA_BLUE] = (BYTE)((src[0] & 0xff0000) >> 16);
					break;
				}
				case FilmOutputs::DIRECT_DIFFUSE: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_DIRECT_DIFFUSE->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::DIRECT_GLOSSY: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_DIRECT_GLOSSY->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::EMISSION: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_EMISSION->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::INDIRECT_DIFFUSE: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_INDIRECT_DIFFUSE->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::INDIRECT_GLOSSY: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_INDIRECT_GLOSSY->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::INDIRECT_SPECULAR: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_INDIRECT_SPECULAR->GetPixel(x, y);
					if (src[3] == 0.f) {
						dst[x].red = 0.f;
						dst[x].green = 0.f;
						dst[x].blue = 0.f;
					} else {
						const float iweight = 1.f / src[3];
						dst[x].red = src[0] * iweight;
						dst[x].green = src[1] * iweight;
						dst[x].blue = src[2] * iweight;
					}
					break;
				}
				case FilmOutputs::MATERIAL_ID_MASK: {
					if (hdrImage) {
						float *dst = (float *)bits;

						const float *maskData = channel_MATERIAL_ID_MASKs[maskMaterialIDsIndex]->GetPixel(x, y);
						if (maskData[1] == 0.f)
							dst[x] = 0.f;
						else
							dst[x] = maskData[0] / maskData[1];
					} else {
						BYTE *dst = &bits[x];

						const float *maskData = channel_MATERIAL_ID_MASKs[maskMaterialIDsIndex]->GetPixel(x, y);
						if (maskData[1] == 0.f)
							*dst = 0;
						else
							*dst = (BYTE)((maskData[0] / maskData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::DIRECT_SHADOW_MASK: {
					if (hdrImage) {
						float *dst = (float *)bits;

						const float *shadowData = channel_DIRECT_SHADOW_MASK->GetPixel(x, y);
						if (shadowData[1] == 0.f)
							dst[x] = 0.f;
						else
							dst[x] = shadowData[0] / shadowData[1];
					} else {
						BYTE *dst = &bits[x];

						const float *shadowData = channel_DIRECT_SHADOW_MASK->GetPixel(x, y);
						if (shadowData[1] == 0.f)
							*dst = 0;
						else
							*dst = (BYTE)((shadowData[0] / shadowData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::INDIRECT_SHADOW_MASK: {
					if (hdrImage) {
						float *dst = (float *)bits;

						const float *shadowData = channel_INDIRECT_SHADOW_MASK->GetPixel(x, y);
						if (shadowData[1] == 0.f)
							dst[x] = 0.f;
						else
							dst[x] = shadowData[0] / shadowData[1];
					} else {
						BYTE *dst = &bits[x];

						const float *shadowData = channel_INDIRECT_SHADOW_MASK->GetPixel(x, y);
						if (shadowData[1] == 0.f)
							*dst = 0;
						else
							*dst = (BYTE)((shadowData[0] / shadowData[1]) * 255.f + .5f);
					}
					break;
				}
				case FilmOutputs::RADIANCE_GROUP: {
					FIRGBF *dst = (FIRGBF *)bits;
					Spectrum c;
					if (radianceGroupIndex < channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()) {
						const float *src = channel_RADIANCE_PER_PIXEL_NORMALIZEDs[radianceGroupIndex]->GetPixel(x, y);

						const float weight = src[3];
						if (weight > 0.f)
							c += Spectrum(src) / weight;
					}
					if (radianceGroupIndex < channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()) {
						const float *src = channel_RADIANCE_PER_SCREEN_NORMALIZEDs[radianceGroupIndex]->GetPixel(x, y);
						
						const float weight = src[3];
						if (weight > 0.f)
							c += Spectrum(src) / (pixelCount / statsTotalSampleCount);
					}

					dst[x].red = c.r;
					dst[x].green = c.g;
					dst[x].blue = c.b;
					break;
				}
				case FilmOutputs::UV: {
					FIRGBF *dst = (FIRGBF *)bits;
					const float *src = channel_UV->GetPixel(x, y);
					dst[x].red = src[0];
					dst[x].green = src[1];
					dst[x].blue = 0.f;
					break;
				}
				case FilmOutputs::RAYCOUNT: {
					float *dst = (float *)bits;

					const float *rayCountData = channel_RAYCOUNT->GetPixel(x, y);
					dst[x] = rayCountData[0];
					break;
				}
				default:
					throw std::runtime_error("Unknown film output type: " + ToString(type));
			}
		}

		// Next line
		bits += pitch;
	}

	if (!FREEIMAGE_SAVE(fif, dib, FREEIMAGE_CONVFILENAME(fileName).c_str(), 0))
		throw std::runtime_error("Failed image save");

	FreeImage_Unload(dib);

	if ((type == FilmOutputs::RGB) || (type == FilmOutputs::RGBA)) {
		// To restore the used tonemapping instead of NONE
		UpdateScreenBuffer();
	}
}

void Film::UpdateScreenBuffer() {
	UpdateScreenBufferImpl(toneMapParams->GetType());
}

void Film::MergeSampleBuffers(Spectrum *p, std::vector<bool> &frameBufferMask) const {
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
				const float *sp = channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->GetPixel(j);

				if (sp[3] > 0.f) {
					if (frameBufferMask[j])
						p[j] += Spectrum(sp) * factor;
					else
						p[j] = Spectrum(sp) * factor;
					frameBufferMask[j] = true;
				}
			}
		}
	}

	if (!enabledOverlappedScreenBufferUpdate) {
		for (u_int i = 0; i < pixelCount; ++i) {
			if (!frameBufferMask[i]) {
				p[i].r = 0.f;
				p[i].g = 0.f;
				p[i].b = 0.f;
			}
		}
	}
}

void Film::UpdateScreenBufferImpl(const ToneMapType type) {
	if ((!HasChannel(RADIANCE_PER_PIXEL_NORMALIZED) && !HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) ||
			!HasChannel(TONEMAPPED_FRAMEBUFFER)) {
		// Nothing to do
		return;
	}

	switch (type) {
		case TONEMAP_NONE: {
			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);
			break;
		}
		case TONEMAP_LINEAR: {
			const LinearToneMapParams &tm = (LinearToneMapParams &)(*toneMapParams);
			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			const u_int pixelCount = width * height;
			std::vector<bool> frameBufferMask(pixelCount, false);

			MergeSampleBuffers(p, frameBufferMask);

			// Gamma correction
			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i])
					p[i] = Radiance2Pixel(tm.scale * p[i]);
			}
			break;
		}
		case TONEMAP_REINHARD02: {
			const Reinhard02ToneMapParams &tm = (Reinhard02ToneMapParams &)(*toneMapParams);

			const float alpha = .1f;
			const float preScale = tm.preScale;
			const float postScale = tm.postScale;
			const float burn = tm.burn;

			Spectrum *p = (Spectrum *)channel_RGB_TONEMAPPED->GetPixels();
			const u_int pixelCount = width * height;

			std::vector<bool> frameBufferMask(pixelCount, false);
			MergeSampleBuffers(p, frameBufferMask);

			// Use the frame buffer as temporary storage and calculate the average luminance
			float Ywa = 0.f;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i]) {
					// Convert to XYZ color space
					Spectrum xyz;
					xyz.r = 0.412453f * p[i].r + 0.357580f * p[i].g + 0.180423f * p[i].b;
					xyz.g = 0.212671f * p[i].r + 0.715160f * p[i].g + 0.072169f * p[i].b;
					xyz.b = 0.019334f * p[i].r + 0.119193f * p[i].g + 0.950227f * p[i].b;
					p[i] = xyz;

					Ywa += p[i].g;
				}
			}
			Ywa /= pixelCount;

			// Avoid division by zero
			if (Ywa == 0.f)
				Ywa = 1.f;

			const float Yw = preScale * alpha * burn;
			const float invY2 = 1.f / (Yw * Yw);
			const float pScale = postScale * preScale * alpha / Ywa;

			for (u_int i = 0; i < pixelCount; ++i) {
				if (frameBufferMask[i]) {
					Spectrum xyz = p[i];

					const float ys = xyz.g;
					xyz *= pScale * (1.f + ys * invY2) / (1.f + ys);

					// Convert back to RGB color space
					p[i].r =  3.240479f * xyz.r - 1.537150f * xyz.g - 0.498535f * xyz.b;
					p[i].g = -0.969256f * xyz.r + 1.875991f * xyz.g + 0.041556f * xyz.b;
					p[i].b =  0.055648f * xyz.r - 0.204043f * xyz.g + 1.057311f * xyz.b;

					// Gamma correction
					p[i].r = Radiance2PixelFloat(p[i].r);
					p[i].g = Radiance2PixelFloat(p[i].g);
					p[i].b = Radiance2PixelFloat(p[i].b);
				}
			}
			break;
		}
		default:
			assert (false);
			break;
	}
}

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiancePerPixelNormalized.size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiancePerPixelNormalized[i].IsNaN() || sampleResult.radiancePerPixelNormalized[i].IsInf())
				continue;

			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddWeightedPixel(x, y, &sampleResult.radiancePerPixelNormalized[i].r, weight);
		}
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min(sampleResult.radiancePerScreenNormalized.size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			if (sampleResult.radiancePerScreenNormalized[i].IsNaN() || sampleResult.radiancePerScreenNormalized[i].IsInf())
				continue;

			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddWeightedPixel(x, y, &sampleResult.radiancePerScreenNormalized[i].r, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AddWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE))
			channel_DIRECT_DIFFUSE->AddWeightedPixel(x, y, &sampleResult.directDiffuse.r, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY))
			channel_DIRECT_GLOSSY->AddWeightedPixel(x, y, &sampleResult.directGlossy.r, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AddWeightedPixel(x, y, &sampleResult.emission.r, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE))
			channel_INDIRECT_DIFFUSE->AddWeightedPixel(x, y, &sampleResult.indirectDiffuse.r, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY))
			channel_INDIRECT_GLOSSY->AddWeightedPixel(x, y, &sampleResult.indirectGlossy.r, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR))
			channel_INDIRECT_SPECULAR->AddWeightedPixel(x, y, &sampleResult.indirectSpecular.r, weight);

		// This is MATERIAL_ID_MASK
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AddPixel(x, y, pixel);
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
	return convTest->Test((const float *)channel_RGB_TONEMAPPED->GetPixels());
}
