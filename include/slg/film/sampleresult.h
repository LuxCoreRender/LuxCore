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

#include "slg/film/film.h"

#ifndef _SLG_SAMPLERESULT_H
#define	_SLG_SAMPLERESULT_H

namespace slg {

//------------------------------------------------------------------------------
// SampleResult
//------------------------------------------------------------------------------

class SampleResult {
public:
	SampleResult() { }
	SampleResult(const u_int channelTypes, const u_int radianceGroupCount) {
		Init(channelTypes, radianceGroupCount);
	}
	~SampleResult() { }

	void Init(const u_int channelTypes, const u_int radianceGroupCount);

	bool HasChannel(const Film::FilmChannelType type) const { return (channels & type) != 0; }
	float Y() const;

	void AddEmission(const u_int lightID, const luxrays::Spectrum &pathThroughput,
		const luxrays::Spectrum &incomingRadiance);
	void AddDirectLight(const u_int lightID, const BSDFEvent bsdfEvent,
		const luxrays::Spectrum &pathThroughput, const luxrays::Spectrum &incomingRadiance,
		const float lightScale);

	void ClampRadiance(const float radianceCap);

	//--------------------------------------------------------------------------
	// Used by render engines not supporting AOVs (note: DEPRECATED)
	static void AddSampleResult(std::vector<SampleResult> &sampleResults,
		const float filmX, const float filmY,
		const luxrays::Spectrum &radiancePPN,
		const float alpha);

	static void AddSampleResult(std::vector<SampleResult> &sampleResults,
		const float filmX, const float filmY,
		const luxrays::Spectrum &radiancePSN);
	//--------------------------------------------------------------------------

	float filmX, filmY;
	std::vector<luxrays::Spectrum> radiance;

	float alpha, depth;
	luxrays::Point position;
	luxrays::Normal geometryNormal, shadingNormal;
	// Note: MATERIAL_ID_MASK is calculated starting from materialID field
	u_int materialID;
	luxrays::Spectrum directDiffuse, directGlossy;
	luxrays::Spectrum emission;
	luxrays::Spectrum indirectDiffuse, indirectGlossy, indirectSpecular;
	float directShadowMask, indirectShadowMask;
	luxrays::UV uv;
	float rayCount;
	luxrays::Spectrum irradiance;
	// Irradiance requires to store some additional information to be computed
	luxrays::Spectrum irradiancePathThroughput;

	BSDFEvent firstPathVertexEvent;
	bool firstPathVertex, lastPathVertex;

private:
	u_int channels;
};

}

#endif	/* _SLG_SAMPLERESULT_H */
