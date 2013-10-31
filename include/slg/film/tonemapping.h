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

#ifndef _SLG_TONEMAPPING_H
#define	_SLG_TONEMAPPING_H

namespace slg {

//------------------------------------------------------------------------------
// Tonemapping
//------------------------------------------------------------------------------

typedef enum {
	TONEMAP_NONE, TONEMAP_LINEAR, TONEMAP_REINHARD02
} ToneMapType;

class ToneMapParams {
public:
	virtual ToneMapType GetType() const = 0;
	virtual ToneMapParams *Copy() const = 0;
	virtual ~ToneMapParams (){}; 
};

class LinearToneMapParams : public ToneMapParams {
public:
	LinearToneMapParams(const float s = 1.f) {
		scale = s;
	}

	ToneMapType GetType() const { return TONEMAP_LINEAR; }

	ToneMapParams *Copy() const {
		return new LinearToneMapParams(scale);
	}

	float scale;
};

class Reinhard02ToneMapParams : public ToneMapParams {
public:
	Reinhard02ToneMapParams(const float preS = 1.f, const float postS = 1.2f,
			const float b = 3.75f) {
		preScale = preS;
		postScale = postS;
		burn = b;
	}

	ToneMapType GetType() const { return TONEMAP_REINHARD02; }

	ToneMapParams *Copy() const {
		return new Reinhard02ToneMapParams(preScale, postScale, burn);
	}

	float preScale, postScale, burn;
};

}

#endif	/* _SLG_TONEMAPPING_H */
