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

#ifndef _SLG_HARLEQUINCOLORS_H
#define	_SLG_HARLEQUINCOLORS_H

#include "luxrays/luxrays.h"
#include "luxrays/core/color/color.h"

namespace slg {

luxrays::Spectrum GetHarlequinColorImpl(const u_longlong v);

template <class T>
luxrays::Spectrum GetHarlequinColor(const T v) {
	return GetHarlequinColorImpl((const u_longlong)v);
}

}

#endif	/* _SLG_HARLEQUINCOLORS_H */
