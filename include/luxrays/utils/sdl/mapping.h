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
#include "luxrays/utils/sdl/hitpoint.h"

namespace luxrays {

// OpenCL data types
namespace ocl {
#include "luxrays/utils/sdl/mapping_types.cl"
}

namespace sdl {

typedef enum {
	UVMAPPING2D
} TextureMapping2DType;

class TextureMapping2D {
public:
	TextureMapping2D() { }
	virtual ~TextureMapping2D() { }

	virtual TextureMapping2DType GetType() const = 0;

	virtual UV Map(const HitPoint &hitPoint) const {
		return Map(hitPoint.uv);
	}
	// Directly used only in InfiniteLight and ImageMapTexture
	virtual UV Map(const UV &uv) const = 0;

	virtual Properties ToProperties(const std::string &name) const = 0;
};

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D
} TextureMapping3DType;

class TextureMapping3D {
public:
	TextureMapping3D(const Transform &w2l) : worldToLocal(w2l) { }
	virtual ~TextureMapping3D() { }

	virtual TextureMapping3DType GetType() const = 0;

	virtual Point Map(const HitPoint &hitPoint) const = 0;

	virtual Properties ToProperties(const std::string &name) const = 0;

	Transform worldToLocal;
};

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

class UVMapping2D : public TextureMapping2D {
public:
	UVMapping2D(const float uscale, const float vscale,
			const float udelta, const float vdelta) : uScale(uscale),
			vScale(vscale), uDelta(udelta), vDelta(vdelta) { }
	virtual ~UVMapping2D() { }

	virtual TextureMapping2DType GetType() const { return UVMAPPING2D; }

	virtual UV Map(const UV &uv) const {
		return UV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);
	}

	virtual Properties ToProperties(const std::string &name) const {
		Properties props;
		props.SetString(name + ".type", "uvmapping2d");
		props.SetString(name + ".uvscale", ToString(uScale) + " " + ToString(vScale));
		props.SetString(name + ".uvdelta", ToString(uDelta) + " " + ToString(vDelta));

		return props;
	}

	float uScale, vScale, uDelta, vDelta;
};

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

class UVMapping3D : public TextureMapping3D {
public:
	UVMapping3D(const Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~UVMapping3D() { }

	virtual TextureMapping3DType GetType() const { return UVMAPPING3D; }

	virtual Point Map(const HitPoint &hitPoint) const {
		return worldToLocal * Point(hitPoint.uv.u, hitPoint.uv.v, 0.f);
	}

	virtual Properties ToProperties(const std::string &name) const {
		Properties props;
		props.SetString(name + ".type", "uvmapping3d");
		props.SetString(name + ".transformation", ToString(worldToLocal.mInv));

		return props;
	}
};

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

class GlobalMapping3D : public TextureMapping3D {
public:
	GlobalMapping3D(const Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~GlobalMapping3D() { }

	virtual TextureMapping3DType GetType() const { return GLOBALMAPPING3D; }

	virtual Point Map(const HitPoint &hitPoint) const {
		return worldToLocal * hitPoint.p;
	}

	virtual Properties ToProperties(const std::string &name) const {
		Properties props;
		props.SetString(name + ".type", "globalmapping3d");
		props.SetString(name + ".transformation", ToString(worldToLocal.mInv));

		return props;
	}
};

} }

#endif	/* _LUXRAYS_SDL_MAPPING_H */
