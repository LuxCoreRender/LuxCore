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

#ifndef _SLG_OBJECT_ID_H
#define	_SLG_OBJECT_ID_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// ObjectID texture
//------------------------------------------------------------------------------

class ObjectIDTexture : public Texture {
public:
	ObjectIDTexture() { }
	virtual ~ObjectIDTexture() { }

	virtual TextureType GetType() const { return OBJECTID_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;
};

//------------------------------------------------------------------------------
// ObjectIDColor texture
//------------------------------------------------------------------------------

class ObjectIDColorTexture : public Texture {
public:
	ObjectIDColorTexture() { }
	virtual ~ObjectIDColorTexture() { }

	virtual TextureType GetType() const { return OBJECTID_COLOR_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;
};

//------------------------------------------------------------------------------
// ObjectIDNormalized texture
//------------------------------------------------------------------------------

class ObjectIDNormalizedTexture : public Texture {
public:
	ObjectIDNormalizedTexture() { }
	virtual ~ObjectIDNormalizedTexture() { }

	virtual TextureType GetType() const { return OBJECTID_NORMALIZED_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const;
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
	// The following methods don't make very much sense in this case. I have no
	// information about the color.
	virtual float Y() const { return 1.f; }
	virtual float Filter() const { return 1.f; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;
};

}

#endif	/* _SLG_OBJECT_ID_H */
