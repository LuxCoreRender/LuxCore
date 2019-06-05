/***************************************************************************
￼  * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
￼  *                                                                         *
￼  *   This file is part of LuxCoreRender.                                   *
￼  *                                                                         *
￼  * Licensed under the Apache License, Version 2.0 (the "License");         *
￼  * you may not use this file except in compliance with the License.        *
￼  * You may obtain a copy of the License at                                 *
￼  *                                                                         *
￼  *     http://www.apache.org/licenses/LICENSE-2.0                          *
￼  *                                                                         *
￼  * Unless required by applicable law or agreed to in writing, software     *
￼  * distributed under the License is distributed on an "AS IS" BASIS,       *
￼  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
￼  * See the License for the specific language governing permissions and     *
￼  * limitations under the License.                                          *
￼  ***************************************************************************/

#ifndef _SLG_ROUNDTEX_H
#define _SLG_ROUNDTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Rounding texture
//------------------------------------------------------------------------------

class RoundingTexture : public Texture {
public:
    RoundingTexture(const Texture *t, const Texture *i) : texture(t), increment(i) { }
    virtual ~RoundingTexture() { }

    virtual TextureType GetType(void) const { return ROUNDING_TEX; }
    virtual float GetFloatValue(const HitPoint &hitPoint) const;
    virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
    virtual float Y() const;
    virtual float Filter() const;

    virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
        Texture::AddReferencedTextures(referencedTexs);

        texture->AddReferencedTextures(referencedTexs);
        increment->AddReferencedTextures(referencedTexs);
    }

    virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> referencedImgMaps) const {
        texture->AddReferencedImageMaps(referencedImgMaps);
        increment->AddReferencedImageMaps(referencedImgMaps);
    }

    virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
        if(texture == oldTex) {
            texture = newTex;
        }
        if(increment == oldTex) {
            increment = newTex;
        }
    }

    const Texture *GetTexture1() const { return texture; }
    const Texture *GetTexture2() const { return increment; }

    virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
    const Texture *texture;
    const Texture *increment;

    float round(float value, float increment) const;
};
}

#endif  /* _SLG_ROUNDTEX_H */
