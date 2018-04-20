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

#ifndef _SLG_BCD_DENOISER_PLUGIN_H
#define	_SLG_BCD_DENOISER_PLUGIN_H

#include "luxrays/utils/serializationutils.h"
#include "slg/film/imagepipeline/imagepipeline.h"
#include "bcd/core/DeepImage.h"

namespace slg {

//------------------------------------------------------------------------------
// BCD denoiser image plugin
//------------------------------------------------------------------------------

class BCDDenoiserPlugin : public ImagePipelinePlugin {
public:
	BCDDenoiserPlugin(float histogramDistanceThreshold,
					  int patchRadius,
				  	  int searchWindowRadius,
				  	  float minEigenValue,
				  	  bool useRandomPixelOrder,
				  	  float markedPixelsSkippingProbability,
				  	  int threadCount,
				  	  int scales);
	virtual ~BCDDenoiserPlugin();

	virtual ImagePipelinePlugin *Copy() const;

	virtual void Apply(Film &film, const u_int index);

	friend class boost::serialization::access;

private:
	// Used by serialization
	BCDDenoiserPlugin();

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		// TODO
	}
	
	void Sanitize(bcd::DeepImage<float> &image) const;
	
	float histogramDistanceThreshold;
  	int patchRadius;
	int searchWindowRadius;
	float minEigenValue;
	bool useRandomPixelOrder;
	float markedPixelsSkippingProbability;
	int threadCount;
	int scales;
};

}

BOOST_CLASS_VERSION(slg::BCDDenoiserPlugin, 1)

BOOST_CLASS_EXPORT_KEY(slg::BCDDenoiserPlugin)

#endif	/*  _SLG_BCD_DENOISER_PLUGIN_H */
