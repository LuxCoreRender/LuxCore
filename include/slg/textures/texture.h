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

#ifndef _SLG_TEXTURE_H
#define	_SLG_TEXTURE_H

#include <string>
#include <vector>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/geometry/uv.h"
#include "luxrays/core/geometry/point.h"
#include "luxrays/core/geometry/normal.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/namedobject.h"
#include "slg/imagemap/imagemap.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/textures/mapping/mapping.h"
#include "slg/bsdf/hitpoint.h"

namespace slg {

// OpenCL data types
namespace ocl {
using luxrays::ocl::Spectrum;
#include "slg/textures/texture_types.cl"
}

//------------------------------------------------------------------------------
// Texture
//------------------------------------------------------------------------------

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, SUBTRACT_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY, NORMALMAP_TEX, BLACKBODY_TEX, IRREGULARDATA_TEX, DENSITYGRID_TEX,
	ABS_TEX, CLAMP_TEX, BILERP_TEX, COLORDEPTH_TEX, HSV_TEX, DIVIDE_TEX, REMAP_TEX,
	OBJECTID_TEX, OBJECTID_COLOR_TEX, OBJECTID_NORMALIZED_TEX, DOT_PRODUCT_TEX,
	POWER_TEX, LESS_THAN_TEX, GREATER_THAN_TEX, ROUNDING_TEX, MODULO_TEX, SHADING_NORMAL_TEX,
    POSITION_TEX, SPLIT_FLOAT3, MAKE_FLOAT3, BRIGHT_CONTRAST_TEX, HITPOINTVERTEXAOV,
	HITPOINTTRIANGLEAOV, TRIPLANAR_TEX, RANDOM_TEX, BEVEL_TEX, DISTORT_TEX,
	BOMBING_TEX, // 44 textures
	// Procedural textures
	BLENDER_BLEND, BLENDER_CLOUDS, BLENDER_DISTORTED_NOISE, BLENDER_MAGIC, BLENDER_MARBLE,
	BLENDER_MUSGRAVE, BLENDER_NOISE, BLENDER_STUCCI, BLENDER_WOOD,  BLENDER_VORONOI,
	CHECKERBOARD2D, CHECKERBOARD3D, CLOUD_TEX, FBM_TEX,
	MARBLE, DOTS, BRICK, WINDY, WRINKLED, UV_TEX, BAND_TEX,
	WIREFRAME_TEX, // 65 textures
	// Fresnel textures
	FRESNELCOLOR_TEX, FRESNELCONST_TEX
} TextureType;

class Texture : public luxrays::NamedObject {
public:
	Texture() : NamedObject("texture") { }
	virtual ~Texture() { }

	virtual TextureType GetType() const = 0;
	// Return the texture name or the values for constant textures, it is
	// used when exporting the scene in text format
	virtual std::string GetSDLValue() const;

	virtual float GetFloatValue(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const = 0;
	virtual float Y() const = 0;
	virtual float Filter() const = 0;

	// Used for bump/normal mapping support
	virtual luxrays::Normal Bump(const HitPoint &hitPoint, const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		referencedTexs.insert(this);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const = 0;
};

//------------------------------------------------------------------------------
// Texture utility functions
//------------------------------------------------------------------------------

extern float tex_sin(float a);
extern float tex_saw(float a);
extern float tex_tri(float a);

extern float Turbulence(const luxrays::Point &P, const float omega, const int maxOctaves);
extern float FBm(const luxrays::Point &P, const float omega, const int maxOctaves);
extern float Noise(float x, float y = .5f, float z = .5f);
inline float Noise(const luxrays::Point &P) {
	return Noise(P.x, P.y, P.z);
}

}

#endif	/* _SLG_TEXTURE_H */
