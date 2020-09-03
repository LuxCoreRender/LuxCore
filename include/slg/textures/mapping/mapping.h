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

#ifndef _SLG_MAPPING_H
#define	_SLG_MAPPING_H

#include <string>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/geometry/matrix3x3.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"
#include "slg/slg.h"
#include "slg/bsdf/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
using namespace luxrays::ocl;
#include "slg/textures/mapping/mapping_types.cl"
}

typedef enum {
	OBJECT_ID, TRIANGLE_AOV, OBJECT_ID_OFFSET
} RandomMappingSeedType;

extern RandomMappingSeedType String2RandomMappingSeedType(const std::string &type);
extern std::string RandomMappingSeedType2String(const RandomMappingSeedType type);

//------------------------------------------------------------------------------
// TextureMapping2D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING2D, UVRANDOMMAPPING2D
} TextureMapping2DType;

class TextureMapping2D {
public:
	TextureMapping2D(const u_int index) : dataIndex(index) { }
	virtual ~TextureMapping2D() { }

	u_int GetDataIndex() const { return dataIndex; }

	virtual TextureMapping2DType GetType() const = 0;

	virtual luxrays::UV Map(const HitPoint &hitPoint) const = 0;
	virtual luxrays::UV MapDuv(const HitPoint &hitPoint,
		luxrays::UV *ds, luxrays::UV *dt) const = 0;

	virtual luxrays::Properties ToProperties(const std::string &name) const;

protected:
	const u_int dataIndex;
};

//------------------------------------------------------------------------------
// UVMapping2D
//------------------------------------------------------------------------------

class UVMapping2D : public TextureMapping2D {
public:
	UVMapping2D(const u_int dataIndex, const float rot, const float uScale, const float vScale,
			const float uDelta, const float vDelta);
	virtual ~UVMapping2D() { }

	virtual TextureMapping2DType GetType() const { return UVMAPPING2D; }

	virtual luxrays::UV Map(const HitPoint &hitPoint) const;
	virtual luxrays::UV MapDuv(const HitPoint &hitPoint, luxrays::UV *ds, luxrays::UV *dt) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;

	const float uvRotation, uScale, vScale, uDelta, vDelta;
	const float sinTheta, cosTheta;
};

//------------------------------------------------------------------------------
// UVRandomMapping2D
//------------------------------------------------------------------------------

class UVRandomMapping2D : public TextureMapping2D {
public:
	UVRandomMapping2D(const u_int dataIndex, const RandomMappingSeedType seedType,
			const u_int triAOVIndex, const u_int objectIDOffset,
			const float uvRotationMin, const float uvRotationMax,
			const float uScaleMin, const float uScaleMax,
			const float vScaleMin, const float vScaleMax,
			const float uDeltaMin, const float uDeltaMax,
			const float vDeltaMin, const float vDeltaMax,
			const bool uniformScale);
	virtual ~UVRandomMapping2D() { }

	virtual TextureMapping2DType GetType() const { return UVRANDOMMAPPING2D; }

	virtual luxrays::UV Map(const HitPoint &hitPoint) const;
	virtual luxrays::UV MapDuv(const HitPoint &hitPoint, luxrays::UV *ds, luxrays::UV *dt) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;

	const RandomMappingSeedType seedType;
	const u_int triAOVIndex;
	const u_int objectIDOffset;
	const float uvRotationMin, uvRotationMax;
	const float uScaleMin, uScaleMax;
	const float vScaleMin, vScaleMax;
	const float uDeltaMin, uDeltaMax;
	const float vDeltaMin, vDeltaMax;
	
	const bool uniformScale;

private:
	luxrays::UV Map(const HitPoint &hitPoint, luxrays::UV *ds, luxrays::UV *dt) const;
};

//------------------------------------------------------------------------------
// TextureMapping3D
//------------------------------------------------------------------------------

typedef enum {
	UVMAPPING3D, GLOBALMAPPING3D, LOCALMAPPING3D, LOCALRANDOMMAPPING3D
} TextureMapping3DType;

class TextureMapping3D {
public:
	TextureMapping3D(const luxrays::Transform &w2l) : worldToLocal(w2l) { }
	virtual ~TextureMapping3D() { }

	virtual TextureMapping3DType GetType() const = 0;

	virtual luxrays::Point Map(const HitPoint &hitPoint, luxrays::Normal *shadeN = nullptr) const = 0;

	virtual luxrays::Properties ToProperties(const std::string &name) const = 0;

	const luxrays::Transform worldToLocal;
};

//------------------------------------------------------------------------------
// UVMapping3D
//------------------------------------------------------------------------------

class UVMapping3D : public TextureMapping3D {
public:
	UVMapping3D(const u_int index, const luxrays::Transform &w2l) :
		TextureMapping3D(w2l), dataIndex(index) { }
	virtual ~UVMapping3D() { }

	u_int GetDataIndex() const { return dataIndex; }

	virtual TextureMapping3DType GetType() const { return UVMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint, luxrays::Normal *shadeN = nullptr) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;

private:
	const u_int dataIndex;
};

//------------------------------------------------------------------------------
// GlobalMapping3D
//------------------------------------------------------------------------------

class GlobalMapping3D : public TextureMapping3D {
public:
	GlobalMapping3D(const luxrays::Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~GlobalMapping3D() { }

	virtual TextureMapping3DType GetType() const { return GLOBALMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint, luxrays::Normal *shadeN = nullptr) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;
};

//------------------------------------------------------------------------------
// LocalMapping3D
//------------------------------------------------------------------------------

class LocalMapping3D : public TextureMapping3D {
public:
	LocalMapping3D(const luxrays::Transform &w2l) : TextureMapping3D(w2l) { }
	virtual ~LocalMapping3D() { }

	virtual TextureMapping3DType GetType() const { return LOCALMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint, luxrays::Normal *shadeN = nullptr) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;
};

//------------------------------------------------------------------------------
// LocalRandomMapping3D
//------------------------------------------------------------------------------

class LocalRandomMapping3D : public TextureMapping3D {
public:
	LocalRandomMapping3D(const luxrays::Transform &w2l, const RandomMappingSeedType seedType,
			const u_int triAOVIndex, const u_int objectIDOffset,
			const float xRotationMin, const float xRotationMax,
			const float yRotationMin, const float yRotationMax,
			const float zRotationMin, const float zRotationMax,
			const float xScaleMin, const float xScaleMax,
			const float yScaleMin, const float yScaleMax,
			const float zScaleMin, const float zScaleMax,
			const float xTranslateMin, const float xTranslateMax,
			const float yTranslateMin, const float yTranslateMax,
			const float zTranslateMin, const float zTranslateMax,
			const bool uniformScale);
	virtual ~LocalRandomMapping3D() { }

	virtual TextureMapping3DType GetType() const { return LOCALRANDOMMAPPING3D; }

	virtual luxrays::Point Map(const HitPoint &hitPoint, luxrays::Normal *shadeN = nullptr) const;

	virtual luxrays::Properties ToProperties(const std::string &name) const;
	
	const RandomMappingSeedType seedType;
	const u_int triAOVIndex;
	const u_int objectIDOffset;
	const float xRotationMin, xRotationMax;
	const float yRotationMin, yRotationMax;
	const float zRotationMin, zRotationMax;
	const float xScaleMin, xScaleMax;
	const float yScaleMin, yScaleMax;
	const float zScaleMin, zScaleMax;
	const float xTranslateMin, xTranslateMax;
	const float yTranslateMin, yTranslateMax;
	const float zTranslateMin, zTranslateMax;

	const bool uniformScale;
};

}

#endif	/* _SLG_MAPPING_H */
