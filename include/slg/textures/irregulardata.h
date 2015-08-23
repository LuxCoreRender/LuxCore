/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_IRREGULARDATATEX_H
#define	_SLG_IRREGULARDATATEX_H

#include "slg/textures/texture.h"

namespace slg {

//------------------------------------------------------------------------------
// Irregular data texture
//------------------------------------------------------------------------------

class IrregularDataTexture : public Texture {
public:
	IrregularDataTexture(const u_int n, const float *waveLengths, const float *data,
			const float resolution);
	virtual ~IrregularDataTexture() { }

	virtual TextureType GetType() const { return IRREGULARDATA_TEX; }
	virtual float GetFloatValue(const HitPoint &hitPoint) const { return rgb.Y(); }
	virtual luxrays::Spectrum GetSpectrumValue(const HitPoint &hitPoint) const { return rgb; }
	virtual float Y() const { return rgb.Y(); }
	virtual float Filter() const { return rgb.Filter(); }

	const vector<float> &GetWaveLengths() const { return waveLengths; }
	const vector<float> &GetData() const { return data; }
	const luxrays::Spectrum &GetRGB() const { return rgb; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	vector<float> waveLengths;
	vector<float> data;

	luxrays::Spectrum rgb;
};

}

#endif	/* _SLG_BLACKBODYTEX_H */
