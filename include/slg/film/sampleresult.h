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

#include "luxrays/core/color/spectrumgroup.h"

#include "slg/bsdf/bsdf.h"
#include "slg/film/film.h"

#ifndef _SLG_SAMPLERESULT_H
#define	_SLG_SAMPLERESULT_H

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/film/sampleresult_types.cl"
}

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

class SampleResult {
public:
	SampleResult() : useFilmSplat(true) { }
	SampleResult(const u_int channelTypes, const u_int radianceGroupCount) {
		Init(channelTypes, radianceGroupCount);
	}
	~SampleResult() { }

	void Init(const u_int channelTypes, const u_int radianceGroupCount);

	u_int GetChannels() const { return channels; }
	bool HasChannel(const Film::FilmChannelType type) const { return (channels & type) != 0; }

	luxrays::Spectrum GetSpectrum(const std::vector<RadianceChannelScale> &radianceChannelScales) const;
	float GetY(const std::vector<RadianceChannelScale> &radianceChannelScales) const;

	void AddEmission(const u_int lightID, const luxrays::Spectrum &pathThroughput,
		const luxrays::Spectrum &incomingRadiance);
	void AddDirectLight(const u_int lightID, const BSDFEvent bsdfEvent,
		const luxrays::Spectrum &pathThroughput, const luxrays::Spectrum &incomingRadiance,
		const float lightScale);

	void ClampRadiance(const u_int index, const float minRadiance, const float maxRadiance) {
		radiance[index] = radiance[index].ScaledClamp(minRadiance, maxRadiance);
	}

	void ClampRadiance(const float minRadiance, const float maxRadiance) {
		for (u_int i = 0; i < radiance.Size(); ++i)
			ClampRadiance(i, minRadiance, maxRadiance);
	}

	bool IsValid() const;

	static bool IsAllValid(const std::vector<SampleResult> &sampleResults);
	
	//--------------------------------------------------------------------------

	// pixelX and pixelY have to be initialized only if !useFilmSplat
	u_int pixelX, pixelY;
	float filmX, filmY;
	luxrays::SpectrumGroup radiance;

	float alpha, depth;
	luxrays::Point position;
	luxrays::Normal geometryNormal, shadingNormal;
	// Note: MATERIAL_ID_MASK is calculated starting from materialID field
	u_int materialID;
	// Note: OBJECT_ID_MASK is calculated starting from objectID field
	u_int objectID;
	luxrays::Spectrum directDiffuse, directGlossy;
	luxrays::Spectrum emission;
	luxrays::Spectrum indirectDiffuse, indirectGlossy, indirectSpecular;
	float directShadowMask, indirectShadowMask;
	luxrays::UV uv;
	float rayCount;
	luxrays::Spectrum irradiance;
	// Irradiance requires to store some additional information to be computed
	luxrays::Spectrum irradiancePathThroughput;
	luxrays::Spectrum albedo;

	BSDFEvent firstPathVertexEvent;
	bool isHoldout;

	// Used to keep some state of the current sample
	bool firstPathVertex, lastPathVertex;

	bool useFilmSplat;

private:
	u_int channels;
};

}

#endif	/* _SLG_SAMPLERESULT_H */
