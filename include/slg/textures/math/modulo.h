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

#ifndef _SLG_MODULOTEX_H
#define _SLG_MODULOTEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Modulo texture
//------------------------------------------------------------------------------

class ModuloTexture : public Texture {
public:
    ModuloTexture(const Texture *t, const Texture *m) : texture(t), modulo(m) {}
    virtual ~ModuloTexture() {}

    virtual TextureType GetType() const {return MODULO_TEX;}
    virtual float GetFloatValue(const HitPoint &hitPoint) const;
    virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const;
    virtual float Y() const {return 1.f;}
    virtual float Filter() const {return 1.f;}

    virtual void AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
        Texture::AddReferencedTextures(referencedTexs);

        texture->AddReferencedTextures(referencedTexs);
        modulo->AddReferencedTextures(referencedTexs);
    }

    virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
        texture->AddReferencedImageMaps(referencedImgMaps);
        modulo->AddReferencedImageMaps(referencedImgMaps);
    }

    virtual void UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
        if(texture == oldTex) {
            texture = newTex;
        }
        if(modulo == oldTex) {
            modulo = newTex;
        }
    }

    const Texture *GetTexture() const {return texture;}
    const Texture *GetModulo() const {return modulo;}

    virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

private:
    const Texture *texture;
    const Texture *modulo;
};
}
#endif  /* _SLG_MODULOTEX_H */
