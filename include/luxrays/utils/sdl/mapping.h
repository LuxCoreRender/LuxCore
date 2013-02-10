/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_SDL_MAPPING_H
#define	_LUXRAYS_SDL_MAPPING_H

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/uv.h"

namespace luxrays {

// OpenCL data types
//namespace ocl {
//#include "luxrays/utils/sdl/mapping_types.cl"
//}

namespace sdl {

//------------------------------------------------------------------------------
// UVMapping
//------------------------------------------------------------------------------

class UVMapping {
public:
	UVMapping(const float uscale, const float vscale,
			const float udelta, const float vdelta) : uScale(uscale),
			vScale(vscale), uDelta(udelta), vDelta(vdelta) { }
	virtual ~UVMapping() { }

	UV Map(const UV &uv) const {
		return UV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);
	}

	float GetUScale() const { return uScale; }
	float GetVScale() const { return vScale; }
	float GetUDelta() const { return uDelta; }
	float GetVDelta() const { return vDelta; }

	UV GetDuDv() const { return UV(1.f / uScale, 1.f / vScale); }

	float uScale, vScale, uDelta, vDelta;
};

} }

#endif	/* _LUXRAYS_SDL_MAPPING_H */
