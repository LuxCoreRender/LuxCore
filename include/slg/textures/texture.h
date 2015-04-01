/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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
#include "luxrays/core/color/color.h"
#include "slg/imagemap/imagemap.h"
#include "slg/imagemap/imagemapcache.h"
#include "slg/sdl/mapping.h"
#include "slg/sdl/hitpoint.h"

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
	HITPOINTGREY, NORMALMAP_TEX, BLACKBODY_TEX, IRREGULARDATA_TEX,
	ABS_TEX, CLAMP_TEX,
	// Procedural textures
	BLENDER_BLEND, BLENDER_CLOUDS, BLENDER_DISTORTED_NOISE, BLENDER_MAGIC, BLENDER_MARBLE,
	BLENDER_MUSGRAVE, BLENDER_NOISE, BLENDER_STUCCI, BLENDER_WOOD,  BLENDER_VORONOI,
	CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX,
	MARBLE, DOTS, BRICK, WINDY, WRINKLED, UV_TEX, BAND_TEX,
	// Fresnel textures
	FRESNELCOLOR_TEX, FRESNELCONST_TEX
} TextureType;

class Texture {
public:
	Texture() { }
	virtual ~Texture() { }

	std::string GetName() const { return "texture-" + boost::lexical_cast<std::string>(this); }
	virtual TextureType GetType() const = 0;

	virtual float GetFloatValue(const HitPoint &hitPoint) const = 0;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const = 0;
	virtual float Y() const = 0;
	virtual float Filter() const = 0;

	// Used for bump/normal mapping support
    virtual luxrays::UV GetDuv(const HitPoint &hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		referencedTexs.insert(this);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;
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

//------------------------------------------------------------------------------
// HitPointColor texture
//------------------------------------------------------------------------------

class HitPointColorTexture : public Texture {
public:
	HitPointColorTexture() { }
	virtual ~HitPointColorTexture() { }

	virtual TextureType GetType() const { return HITPOINTCOLOR; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
};

//------------------------------------------------------------------------------
// HitPointAlpha texture
//------------------------------------------------------------------------------

class HitPointAlphaTexture : public Texture {
public:
	HitPointAlphaTexture() { }
	virtual ~HitPointAlphaTexture() { }

	virtual TextureType GetType() const { return HITPOINTALPHA; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
};

//------------------------------------------------------------------------------
// HitPointGrey texture
//------------------------------------------------------------------------------

class HitPointGreyTexture : public Texture {
public:
	HitPointGreyTexture(const u_int ch) : channel(ch) { }
	virtual ~HitPointGreyTexture() { }

	virtual TextureType GetType() const { return HITPOINTGREY; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	u_int GetChannel() const { return channel; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	u_int channel;
};

//------------------------------------------------------------------------------
// NormalMap textures
//------------------------------------------------------------------------------

class NormalMapTexture : public Texture {
public:
	NormalMapTexture(const Texture *t) : tex(t) { }
	virtual ~NormalMapTexture() { }

	virtual TextureType GetType() const { return NORMALMAP_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return 0.f; }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return luxrays::Spectrum(); }
	virtual float Y() const { return 0.f; }
	virtual float Filter() const { return 0.f; }

    virtual luxrays::UV GetDuv(const HitPoint &hitPoint,
        const luxrays::Vector &dpdu, const luxrays::Vector &dpdv,
        const luxrays::Normal &dndu, const luxrays::Normal &dndv,
        const float sampleDistance) const;

	virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
		Texture::AddReferencedTextures(referencedTexs);

		tex->AddReferencedTextures(referencedTexs);
	}
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		tex->AddReferencedImageMaps(referencedImgMaps);
	}

	virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
		if (tex == oldTex)
			tex = newTex;
	}

	const Texture *GetTexture() const { return tex; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const Texture *tex;
};

}

#endif	/* _SLG_TEXTURE_H */
