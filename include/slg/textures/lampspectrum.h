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

#ifndef _SLG_LAMPSPECTRUMTEX_H
#define	_SLG_LAMPSPECTRUMTEX_H

#include <string>

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Lamp spectrum texture
//------------------------------------------------------------------------------

extern Texture *AllocLampSpectrumTex(const luxrays::Properties &props, const std::string &propName);

}

#endif	/* _SLG_LAMPSPECTRUMTEX_H */
