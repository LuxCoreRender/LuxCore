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

#ifndef _LUXRAYS_FREQUENCYSPD_H
#define _LUXRAYS_FREQUENCYSPD_H

#include "luxrays/core/color/spd.h"

// Sin frequency/phase distribution

#define FREQ_CACHE_START   380.f // precomputed cache starts at wavelength,
#define FREQ_CACHE_END     720.f // and ends at wavelength
#define FREQ_CACHE_SAMPLES 2048  // total number of cache samples 

namespace luxrays {

class FrequencySPD : public SPD {
public:
  FrequencySPD() : SPD() {
	  init(0.03f, 0.5f, 1.f);
  }

  FrequencySPD(float freq, float phase, float refl) : SPD() {
	  init(freq, phase, refl);
  }

  virtual ~FrequencySPD() {
  }

  void init(float freq, float phase, float refl);

protected:
  float fq, ph, r0;
};

}

#endif // _LUXRAYS_FREQUENCYSPD_H

