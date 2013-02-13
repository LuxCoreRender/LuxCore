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

#include <string>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/transform.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/utils/sdl/mapping_types.cl"
}

namespace sdl {

class TextureMapping {
public:
	TextureMapping() { }
	virtual ~TextureMapping() { }

	virtual UV Map(const UV &uv) const = 0;
	virtual Point Map(const Point &p) const = 0;

	virtual Properties ToProperties(const std::string &name) const = 0;
};

//------------------------------------------------------------------------------
// UVMapping
//------------------------------------------------------------------------------

class UVMapping : public TextureMapping {
public:
	UVMapping(const float uscale, const float vscale,
			const float udelta, const float vdelta) : uScale(uscale),
			vScale(vscale), uDelta(udelta), vDelta(vdelta) { }
	virtual ~UVMapping() { }

	virtual UV Map(const UV &uv) const {
		return UV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);
	}

	virtual Point Map(const Point &p) const {
		return Point(p.x * uScale + uDelta, p.x * vScale + vDelta, 0.f);
	}

	virtual Properties ToProperties(const std::string &name) const {
		Properties props;
		props.SetString(name + ".uvscale", ToString(uScale) + " " + ToString(vScale));
		props.SetString(name + ".uvdelta", ToString(uDelta) + " " + ToString(vDelta));

		return props;
	}

	float uScale, vScale, uDelta, vDelta;
};

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

class GlobalMapping3D : public TextureMapping {
public:
	GlobalMapping3D(const Transform &w2l) : worldToLocal(w2l) { }
	virtual ~GlobalMapping3D() { }

	virtual UV Map(const UV &uv) const {
		const Point p = worldToLocal * Point(uv.u, uv.v, 0.f);
		return UV(p.x, p.y);
	}

	virtual Point Map(const Point &p) const {
		return worldToLocal * p;
	}

	virtual Properties ToProperties(const std::string &name) const {
		Properties props;
		props.SetString(name + ".transformation", ToString(worldToLocal.mInv));

		return props;
	}

	Transform worldToLocal;
};

} }

#endif	/* _LUXRAYS_SDL_MAPPING_H */
