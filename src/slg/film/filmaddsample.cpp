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

#include "slg/film/film.h"
#include "slg/film/sampleresult.h"
#include "slg/utils/varianceclamping.h"
#include "slg/film/denoiser/filmdenoiser.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Film add sample related methods
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Normal method versions
//------------------------------------------------------------------------------

void Film::AddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	filmDenoiser.AddSample(x, y, sampleResult, weight);

	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AddIfValidWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i) {
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AddIfValidWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
		}
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AddIfValidWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE)) {
			const Spectrum c = sampleResult.directDiffuseReflect + sampleResult.directDiffuseTransmit;
			channel_DIRECT_DIFFUSE->AddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_DIRECT_DIFFUSE_REFLECT && sampleResult.HasChannel(DIRECT_DIFFUSE_REFLECT))
			channel_DIRECT_DIFFUSE_REFLECT->AddIfValidWeightedPixel(x, y, sampleResult.directDiffuseReflect.c, weight);
		if (channel_DIRECT_DIFFUSE_TRANSMIT && sampleResult.HasChannel(DIRECT_DIFFUSE_TRANSMIT))
			channel_DIRECT_DIFFUSE_TRANSMIT->AddIfValidWeightedPixel(x, y, sampleResult.directDiffuseTransmit.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY)) {
			const Spectrum c = sampleResult.directGlossyReflect + sampleResult.directGlossyTransmit;
			channel_DIRECT_GLOSSY->AddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_DIRECT_GLOSSY_REFLECT && sampleResult.HasChannel(DIRECT_GLOSSY_REFLECT))
			channel_DIRECT_GLOSSY_REFLECT->AddIfValidWeightedPixel(x, y, sampleResult.directGlossyReflect.c, weight);
		if (channel_DIRECT_GLOSSY_TRANSMIT && sampleResult.HasChannel(DIRECT_GLOSSY_TRANSMIT))
			channel_DIRECT_GLOSSY_TRANSMIT->AddIfValidWeightedPixel(x, y, sampleResult.directGlossyTransmit.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AddIfValidWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE)) {
			const Spectrum c = sampleResult.indirectDiffuseReflect + sampleResult.indirectDiffuseTransmit;
			channel_INDIRECT_DIFFUSE->AddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_DIFFUSE_REFLECT && sampleResult.HasChannel(INDIRECT_DIFFUSE_REFLECT))
			channel_INDIRECT_DIFFUSE_REFLECT->AddIfValidWeightedPixel(x, y, sampleResult.indirectDiffuseReflect.c, weight);
		if (channel_INDIRECT_DIFFUSE_TRANSMIT && sampleResult.HasChannel(INDIRECT_DIFFUSE_TRANSMIT))
			channel_INDIRECT_DIFFUSE_TRANSMIT->AddIfValidWeightedPixel(x, y, sampleResult.indirectDiffuseTransmit.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY)) {
			const Spectrum c = sampleResult.indirectGlossyReflect + sampleResult.indirectGlossyTransmit;
			channel_INDIRECT_GLOSSY->AddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_GLOSSY_REFLECT && sampleResult.HasChannel(INDIRECT_GLOSSY_REFLECT))
			channel_INDIRECT_GLOSSY_REFLECT->AddIfValidWeightedPixel(x, y, sampleResult.indirectGlossyReflect.c, weight);
		if (channel_INDIRECT_GLOSSY_TRANSMIT && sampleResult.HasChannel(INDIRECT_GLOSSY_TRANSMIT))
			channel_INDIRECT_GLOSSY_TRANSMIT->AddIfValidWeightedPixel(x, y, sampleResult.indirectGlossyTransmit.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR)) {
			const Spectrum c = sampleResult.indirectSpecularReflect + sampleResult.indirectSpecularTransmit;
			channel_INDIRECT_SPECULAR->AddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_SPECULAR_REFLECT && sampleResult.HasChannel(INDIRECT_SPECULAR_REFLECT))
			channel_INDIRECT_SPECULAR_REFLECT->AddIfValidWeightedPixel(x, y, sampleResult.indirectSpecularReflect.c, weight);
		if (channel_INDIRECT_SPECULAR_TRANSMIT && sampleResult.HasChannel(INDIRECT_SPECULAR_TRANSMIT))
			channel_INDIRECT_SPECULAR_TRANSMIT->AddIfValidWeightedPixel(x, y, sampleResult.indirectSpecularTransmit.c, weight);

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
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
							c += sampleResult.radiance[i];
					}

					channel_BY_MATERIAL_IDs[index]->AddIfValidWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AddIfValidWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AddIfValidWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AddIfValidWeightedPixel(x, y, sampleResult.irradiance.c, weight);

		// This is OBJECT_ID_MASK and BY_OBJECT_ID
		if (sampleResult.HasChannel(OBJECT_ID)) {
			// OBJECT_ID_MASK
			for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.objectID == maskObjectIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_OBJECT_ID_MASKs[i]->AddPixel(x, y, pixel);
			}

			// BY_OBJECT_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byObjectIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.objectID == byObjectIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
							c += sampleResult.radiance[i];
					}

					channel_BY_OBJECT_IDs[index]->AddIfValidWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(MATERIAL_ID_COLOR)
		if (channel_MATERIAL_ID_COLOR && sampleResult.HasChannel(MATERIAL_ID_COLOR)) {
			const u_int matID = sampleResult.materialID;
			const Spectrum matColID(
					(matID & 0x0000ffu) * (1.f / 255.f),
					((matID & 0x00ff00u) >> 8) * (1.f / 255.f),
					((matID & 0xff0000u) >> 16) * (1.f / 255.f));

			channel_MATERIAL_ID_COLOR->AddIfValidWeightedPixel(x, y, matColID.c, weight);
		}

		// Faster than HasChannel(ALBEDO)
		if (channel_ALBEDO && sampleResult.HasChannel(ALBEDO))
			channel_ALBEDO->AddIfValidWeightedPixel(x, y, sampleResult.albedo.c, weight);
		
		// Faster than HasChannel(AVG_SHADING_NORMAL)
		if (channel_AVG_SHADING_NORMAL && sampleResult.HasChannel(AVG_SHADING_NORMAL))
			channel_AVG_SHADING_NORMAL->AddIfValidWeightedPixel(x, y, &sampleResult.shadingNormal.x, weight);
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

		// Faster than HasChannel(OBJECT_ID)
		if (channel_OBJECT_ID && sampleResult.HasChannel(OBJECT_ID) &&
				(sampleResult.objectID != std::numeric_limits<u_int>::max()))
			channel_OBJECT_ID->SetPixel(x, y, &sampleResult.objectID);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AddPixel(x, y, &sampleResult.rayCount);

	if (channel_SAMPLECOUNT && sampleResult.HasChannel(SAMPLECOUNT)) {
		static u_int one = 1;
		channel_SAMPLECOUNT->AddPixel(x, y, &one);
	}
	
	// Nothing to do for CONVERGENCE channel because it is updated
	// by the convergence test
}

void Film::AddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AddSampleResultData(x, y, sampleResult);
}

//------------------------------------------------------------------------------
// Atomic method versions
//------------------------------------------------------------------------------

void Film::AtomicAddSampleResultColor(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight)  {
	filmDenoiser.AddSample(x, y, sampleResult, weight);

	if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
			channel_RADIANCE_PER_PIXEL_NORMALIZEDs[i]->AtomicAddIfValidWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
	}

	// Faster than HasChannel(channel_RADIANCE_PER_SCREEN_NORMALIZED)
	if ((channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_SCREEN_NORMALIZED)) {
		for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_SCREEN_NORMALIZEDs.size()); ++i)
			channel_RADIANCE_PER_SCREEN_NORMALIZEDs[i]->AtomicAddIfValidWeightedPixel(x, y, sampleResult.radiance[i].c, weight);
	}

	// Faster than HasChannel(ALPHA)
	if (channel_ALPHA && sampleResult.HasChannel(ALPHA))
		channel_ALPHA->AtomicAddIfValidWeightedPixel(x, y, &sampleResult.alpha, weight);

	if (hasComposingChannel) {
		// Faster than HasChannel(DIRECT_DIFFUSE)
		if (channel_DIRECT_DIFFUSE && sampleResult.HasChannel(DIRECT_DIFFUSE)) {
			const Spectrum c = sampleResult.directDiffuseReflect + sampleResult.directDiffuseTransmit;
			channel_DIRECT_DIFFUSE->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_DIRECT_DIFFUSE_REFLECT && sampleResult.HasChannel(DIRECT_DIFFUSE_REFLECT))
			channel_DIRECT_DIFFUSE_REFLECT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.directDiffuseReflect.c, weight);
		if (channel_DIRECT_DIFFUSE_TRANSMIT && sampleResult.HasChannel(DIRECT_DIFFUSE_TRANSMIT))
			channel_DIRECT_DIFFUSE_TRANSMIT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.directDiffuseTransmit.c, weight);

		// Faster than HasChannel(DIRECT_GLOSSY)
		if (channel_DIRECT_GLOSSY && sampleResult.HasChannel(DIRECT_GLOSSY)) {
			const Spectrum c = sampleResult.directGlossyReflect + sampleResult.directGlossyTransmit;
			channel_DIRECT_GLOSSY->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_DIRECT_GLOSSY_REFLECT && sampleResult.HasChannel(DIRECT_GLOSSY_REFLECT))
			channel_DIRECT_GLOSSY_REFLECT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.directGlossyReflect.c, weight);
		if (channel_DIRECT_GLOSSY_TRANSMIT && sampleResult.HasChannel(DIRECT_GLOSSY_TRANSMIT))
			channel_DIRECT_GLOSSY_TRANSMIT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.directGlossyTransmit.c, weight);

		// Faster than HasChannel(EMISSION)
		if (channel_EMISSION && sampleResult.HasChannel(EMISSION))
			channel_EMISSION->AtomicAddIfValidWeightedPixel(x, y, sampleResult.emission.c, weight);

		// Faster than HasChannel(INDIRECT_DIFFUSE)
		if (channel_INDIRECT_DIFFUSE && sampleResult.HasChannel(INDIRECT_DIFFUSE)) {
			const Spectrum c = sampleResult.indirectDiffuseReflect + sampleResult.indirectDiffuseTransmit;
			channel_INDIRECT_DIFFUSE->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_DIFFUSE_REFLECT && sampleResult.HasChannel(INDIRECT_DIFFUSE_REFLECT))
			channel_INDIRECT_DIFFUSE_REFLECT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectDiffuseReflect.c, weight);
		if (channel_INDIRECT_DIFFUSE_TRANSMIT && sampleResult.HasChannel(INDIRECT_DIFFUSE_TRANSMIT))
			channel_INDIRECT_DIFFUSE_TRANSMIT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectDiffuseTransmit.c, weight);

		// Faster than HasChannel(INDIRECT_GLOSSY)
		if (channel_INDIRECT_GLOSSY && sampleResult.HasChannel(INDIRECT_GLOSSY)) {
			const Spectrum c = sampleResult.indirectGlossyReflect + sampleResult.indirectGlossyTransmit;
			channel_INDIRECT_GLOSSY->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_GLOSSY_REFLECT && sampleResult.HasChannel(INDIRECT_GLOSSY_REFLECT))
			channel_INDIRECT_GLOSSY_REFLECT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectGlossyReflect.c, weight);
		if (channel_INDIRECT_GLOSSY_TRANSMIT && sampleResult.HasChannel(INDIRECT_GLOSSY_TRANSMIT))
			channel_INDIRECT_GLOSSY_TRANSMIT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectGlossyTransmit.c, weight);

		// Faster than HasChannel(INDIRECT_SPECULAR)
		if (channel_INDIRECT_SPECULAR && sampleResult.HasChannel(INDIRECT_SPECULAR)) {
			const Spectrum c = sampleResult.indirectSpecularReflect + sampleResult.indirectSpecularTransmit;
			channel_INDIRECT_SPECULAR->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
		}
		if (channel_INDIRECT_SPECULAR_REFLECT && sampleResult.HasChannel(INDIRECT_SPECULAR_REFLECT))
			channel_INDIRECT_SPECULAR_REFLECT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectSpecularReflect.c, weight);
		if (channel_INDIRECT_SPECULAR_TRANSMIT && sampleResult.HasChannel(INDIRECT_SPECULAR_TRANSMIT))
			channel_INDIRECT_SPECULAR_TRANSMIT->AtomicAddIfValidWeightedPixel(x, y, sampleResult.indirectSpecularTransmit.c, weight);

		// This is MATERIAL_ID_MASK and BY_MATERIAL_ID
		if (sampleResult.HasChannel(MATERIAL_ID)) {
			// MATERIAL_ID_MASK
			for (u_int i = 0; i < maskMaterialIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.materialID == maskMaterialIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_MATERIAL_ID_MASKs[i]->AtomicAddPixel(x, y, pixel);
			}

			// BY_MATERIAL_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byMaterialIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.materialID == byMaterialIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
							c += sampleResult.radiance[i];
					}

					channel_BY_MATERIAL_IDs[index]->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(DIRECT_SHADOW)
		if (channel_DIRECT_SHADOW_MASK && sampleResult.HasChannel(DIRECT_SHADOW_MASK))
			channel_DIRECT_SHADOW_MASK->AtomicAddIfValidWeightedPixel(x, y, &sampleResult.directShadowMask, weight);

		// Faster than HasChannel(INDIRECT_SHADOW_MASK)
		if (channel_INDIRECT_SHADOW_MASK && sampleResult.HasChannel(INDIRECT_SHADOW_MASK))
			channel_INDIRECT_SHADOW_MASK->AtomicAddIfValidWeightedPixel(x, y, &sampleResult.indirectShadowMask, weight);

		// Faster than HasChannel(IRRADIANCE)
		if (channel_IRRADIANCE && sampleResult.HasChannel(IRRADIANCE))
			channel_IRRADIANCE->AtomicAddIfValidWeightedPixel(x, y, sampleResult.irradiance.c, weight);

		// This is OBJECT_ID_MASK and BY_OBJECT_ID
		if (sampleResult.HasChannel(OBJECT_ID)) {
			// OBJECT_ID_MASK
			for (u_int i = 0; i < maskObjectIDs.size(); ++i) {
				float pixel[2];
				pixel[0] = (sampleResult.objectID == maskObjectIDs[i]) ? weight : 0.f;
				pixel[1] = weight;
				channel_OBJECT_ID_MASKs[i]->AtomicAddPixel(x, y, pixel);
			}

			// BY_OBJECT_ID
			if ((channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size() > 0) && sampleResult.HasChannel(RADIANCE_PER_PIXEL_NORMALIZED)) {
				for (u_int index = 0; index < byObjectIDs.size(); ++index) {
					Spectrum c;

					if (sampleResult.objectID == byObjectIDs[index]) {
						// Merge all radiance groups
						for (u_int i = 0; i < Min<u_int>(sampleResult.radiance.Size(), channel_RADIANCE_PER_PIXEL_NORMALIZEDs.size()); ++i)
							c += sampleResult.radiance[i];
					}

					channel_BY_OBJECT_IDs[index]->AtomicAddIfValidWeightedPixel(x, y, c.c, weight);
				}
			}
		}

		// Faster than HasChannel(MATERIAL_ID_COLOR)
		if (channel_MATERIAL_ID_COLOR && sampleResult.HasChannel(MATERIAL_ID_COLOR)) {
			const u_int matID = sampleResult.materialID;
			const Spectrum matColID(
					(matID & 0x0000ffu) * ( 1.f / 255.f),
					((matID & 0x00ff00u) >> 8) * ( 1.f / 255.f),
					((matID & 0xff0000u) >> 16) * ( 1.f / 255.f));

			channel_MATERIAL_ID_COLOR->AtomicAddIfValidWeightedPixel(x, y, matColID.c, weight);
		}
		
		// Faster than HasChannel(ALBEDO)
		if (channel_ALBEDO && sampleResult.HasChannel(ALBEDO))
			channel_ALBEDO->AtomicAddIfValidWeightedPixel(x, y, sampleResult.albedo.c, weight);

		// Faster than HasChannel(AVG_SHADING_NORMAL)
		if (channel_AVG_SHADING_NORMAL && sampleResult.HasChannel(AVG_SHADING_NORMAL))
			channel_AVG_SHADING_NORMAL->AtomicAddIfValidWeightedPixel(x, y, &sampleResult.shadingNormal.x, weight);
	}
}

void Film::AtomicAddSampleResultData(const u_int x, const u_int y,
		const SampleResult &sampleResult)  {
	bool depthWrite = true;

	// Faster than HasChannel(DEPTH)
	if (channel_DEPTH && sampleResult.HasChannel(DEPTH))
		depthWrite = channel_DEPTH->AtomicMinPixel(x, y, &sampleResult.depth);

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

		// Faster than HasChannel(OBJECT_ID)
		if (channel_OBJECT_ID && sampleResult.HasChannel(OBJECT_ID) &&
				(sampleResult.objectID != std::numeric_limits<u_int>::max()))
			channel_OBJECT_ID->SetPixel(x, y, &sampleResult.objectID);
	}

	if (channel_RAYCOUNT && sampleResult.HasChannel(RAYCOUNT))
		channel_RAYCOUNT->AtomicAddPixel(x, y, &sampleResult.rayCount);

	if (channel_SAMPLECOUNT && sampleResult.HasChannel(SAMPLECOUNT)) {
		static u_int one = 1;
		channel_SAMPLECOUNT->AtomicAddPixel(x, y, &one);
	}
	
	// Nothing to do for CONVERGENCE channel because it is updated
	// by the convergence test
}

void Film::AtomicAddSample(const u_int x, const u_int y,
		const SampleResult &sampleResult, const float weight) {
	AtomicAddSampleResultColor(x, y, sampleResult, weight);
	if (hasDataChannel)
		AtomicAddSampleResultData(x, y, sampleResult);
}
