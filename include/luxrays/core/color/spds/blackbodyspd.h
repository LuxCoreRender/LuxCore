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

#ifndef _LUXRAYS_BLACKBODYSPD_H
#define _LUXRAYS_BLACKBODYSPD_H

#include "luxrays/core/color/spd.h"

#define BB_CACHE_START   380.f // precomputed cache starts at wavelength,
#define BB_CACHE_END     720.f // and ends at wavelength
#define BB_CACHE_SAMPLES 256  // total number of cache samples 

namespace luxrays {

class BlackbodySPD : public SPD {
public:
  BlackbodySPD() : SPD() {
	  init(6500.f); // default to D65 temperature
  }

  BlackbodySPD(float t) : SPD() {
	  init(t);
  }

  virtual ~BlackbodySPD() {
  }

  void init(float t);

protected:
  float temp;
};

}

#endif // _LUXRAYS_BLACKBODYSPD_H

