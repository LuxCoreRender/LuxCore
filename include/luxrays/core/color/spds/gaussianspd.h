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

#ifndef _LUXRAYS_GAUSSIANSPD_H
#define _LUXRAYS_GAUSSIANSPD_H

#include "luxrays/core/color/spd.h"

// Peak + fallof SPD using gaussian distribution

#define GAUSS_CACHE_START   380.f // precomputed cache starts at wavelength,
#define GAUSS_CACHE_END     720.f // and ends at wavelength
#define GAUSS_CACHE_SAMPLES 512  // total number of cache samples 

namespace luxrays {

class GaussianSPD : public SPD {
public:
  GaussianSPD() : SPD() {
	  init(550.0f, 50.0f, 1.f);
  }

  GaussianSPD(float mean, float width, float refl) : SPD() {
	  init(mean, width, refl);
  }

  virtual ~GaussianSPD() {
  }

  void init(float mean, float width, float refl);

protected:
  float mu, wd, r0;
};

}

#endif // _LUXRAYS_GAUSSIANSPD_H
