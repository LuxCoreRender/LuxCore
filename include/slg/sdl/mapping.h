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

#ifndef _SLG_MAPPING_H
#define	_SLG_MAPPING_H

#include <string>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/transform.h"
#include "slg/slg.h"
#include "slg/sdl/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
using namespace luxrays::ocl;
#include "slg/sdl/mapping_types.cl"
}

typedef enum {
	UVMAPPING2D
} TextureMapping2DType;

class TextureMapping2D {
public:
	TextureMapping2D() { }
	virtual ~TextureMapping2D() { }

	virtual TextureMapping2DType GetType() const = 0;

	virtual luxrays::UV Map(const HitPoint &hitPoint) const {
		return Map(hitPoint.uv);
	}
	// Directly used only in InfiniteLight and ImageMapTexture
	virtual luxrays::UV Map(const luxrays::UV &uv) const = 0;

	virtual luxrays::Properties ToProperties(const std::string &name) const = 0;
};

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D
} TextureMapping3DType;

class TextureMapping3D {
public:
	TextureMapping3D(const luxrays::Transform &w2l) : worldToLocal(w2l) { }
	virtual ~TextureMapping3D() { }

	virtual TextureMapping3DType GetType() const = 0;

	virtual luxrays::Point Map(const HitPoint &hitPoint) const = 0;

	virtual luxrays::Properties ToProperties(const std::string &name) const = 0;

	luxrays::Transform worldToLocal;
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

	virtual luxrays::UV Map(const luxrays::UV &uv) const {
		return luxrays::UV(uv.u * uScale + uDelta, uv.v * vScale + vDelta);
	}

	virtual luxrays::Properties ToProperties(const std::string &name) const {
		luxrays::Properties props;
		props.SetString(name + ".type", "uvmapping2d");
		props.SetString(name + ".uvscale", luxrays::ToString(uScale) + " " + luxrays::ToString(vScale));
		props.SetString(name + ".uvdelta", luxrays::ToString(uDelta) + " " + luxrays::ToString(vDelta));

		return props;
	}

	float uScale, vScale, uDelta, vDelta;
};

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

class UVMapping3D : public TextureMapping3D {
public:
	UVMapping3D(const luxrays::Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~UVMapping3D() { }

	virtual TextureMapping3DType GetType() const { return UVMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint) const {
		return worldToLocal * luxrays::Point(hitPoint.uv.u, hitPoint.uv.v, 0.f);
	}

	virtual luxrays::Properties ToProperties(const std::string &name) const {
		luxrays::Properties props;
		props.SetString(name + ".type", "uvmapping3d");
		props.SetString(name + ".transformation", luxrays::ToString(worldToLocal.mInv));

		return props;
	}
};

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

class GlobalMapping3D : public TextureMapping3D {
public:
	GlobalMapping3D(const luxrays::Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~GlobalMapping3D() { }

	virtual TextureMapping3DType GetType() const { return GLOBALMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint) const {
		return worldToLocal * hitPoint.p;
	}

	virtual luxrays::Properties ToProperties(const std::string &name) const {
		luxrays::Properties props;
		props.SetString(name + ".type", "globalmapping3d");
		props.SetString(name + ".transformation", luxrays::ToString(worldToLocal.mInv));

		return props;
	}
};

}

#endif	/* _SLG_MAPPING_H */
