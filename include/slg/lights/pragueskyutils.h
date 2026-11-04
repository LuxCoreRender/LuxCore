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

#ifndef _SLG_PRAGUESKYUTILS_H
#define	 _SLG_PRAGUESKYUTILS_H

#include <memory>
#include <string>

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"
#include "slg/lights/data/pragueskymodel/PragueSkyModel.h"

namespace slg {

//------------------------------------------------------------------------------
// Shared helpers for the Prague Sky Model based light sources (sky3, sun3)
//------------------------------------------------------------------------------

// Returns a shared instance of the Prague Sky Model loaded from the given
// dataset file (resolved with SLG_FileNameResolver). The instances are cached
// (thread-safely), so multiple light sources (e.g. a sky3 + sun3 pair) share
// the same dataset in memory and the (multi GB) file is read only once.
// The cache holds only weak references: the dataset is released when the last
// light source using it is destroyed. Note that new light sources are always
// created before the old ones are deleted (e.g. during scene edits), so an
// edit of a light property does not re-read the dataset.
std::shared_ptr<PragueSkyModel> GetPragueSkyModel(const std::string &datasetFileName);

// Converts spectral radiance samples (one per dataset channel, in
// W / (m^2 * sr * nm)) to a linear RGB luxrays::Spectrum in
// lm / (m^2 * sr), using the CIE 1931 2 degree standard observer as in the
// reference implementation of the model. Out of gamut (negative) components
// are clamped to 0.
luxrays::Spectrum PragueSkyModelToSpectrum(const double *channelRadiance,
		const u_int channels, const double channelStart, const double channelWidth);

// Evaluates the given PragueSkyModel and returns the wavelength of a dataset
// channel
inline double PragueSkyModelChannelWavelength(const u_int index,
		const double channelStart, const double channelWidth) {
	return channelStart + index * channelWidth;
}

}

#endif	/* _SLG_PRAGUESKYUTILS_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
