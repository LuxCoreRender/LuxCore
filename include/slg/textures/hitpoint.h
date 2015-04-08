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

#ifndef _SLG_HITPOINTTEX_H
#define	_SLG_HITPOINTTEX_H

#include "slg/textures/texture.h"

namespace slg {

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

}

#endif	/* _SLG_HITPOINTTEX_H */
