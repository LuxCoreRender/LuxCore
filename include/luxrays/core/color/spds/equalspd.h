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

#ifndef _LUXRAYS_EQUALSPD_H
#define _LUXRAYS_EQUALSPD_H

#include "luxrays/core/color/spd.h"

// Equal energy SPD

#define EQ_CACHE_START   380.f // precomputed cache starts at wavelength,
#define EQ_CACHE_END     720.f // and ends at wavelength
#define EQ_CACHE_SAMPLES 2  // total number of cache samples 

namespace luxrays {

class EqualSPD : public SPD {
public:
  EqualSPD() : SPD() {
	  init(1.f);
  }

  EqualSPD(float p) : SPD() {
	  init(p);
  }

  virtual ~EqualSPD() {
  }

  void init(float p);

protected:
  float power;
};

}

#endif // _LUXRAYS_EQUALSPD_H
