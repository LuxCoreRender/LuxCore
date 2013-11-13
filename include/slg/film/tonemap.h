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

#include <cmath>
#include <string>

#include "luxrays/luxrays.h"

namespace slg {

//------------------------------------------------------------------------------
// Tone mapping
//------------------------------------------------------------------------------

typedef enum {
	TONEMAP_LINEAR, TONEMAP_REINHARD02
} ToneMapType;

extern std::string ToneMapType2String(const ToneMapType type);
extern ToneMapType String2ToneMapType(const std::string &type);

class ToneMap {
public:
	ToneMap () {}
	virtual ~ToneMap () {}

	virtual ToneMapType GetType() const = 0;
	virtual ToneMap *Copy() const = 0;

	virtual void Apply(luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask,
		const u_int width, const u_int height) const = 0;
};

class LinearToneMap : public ToneMap {
public:
	LinearToneMap() {
		scale = 1.f;
	}

	LinearToneMap(const float s) {
		scale = s;
	}

	LinearToneMap(const float sensitivity, const float exposure,
		const float fstop, const float gamma) {
		scale = exposure / (fstop * fstop) * sensitivity * 0.65f / 10.f * powf(118.f / 255.f, gamma);
	}

	ToneMapType GetType() const { return TONEMAP_LINEAR; }

	ToneMap *Copy() const {
		return new LinearToneMap(scale);
	}

	void Apply(luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask,
		const u_int width, const u_int height) const;

	float scale;
};

class Reinhard02ToneMap : public ToneMap {
public:
	Reinhard02ToneMap() {
		preScale = 1.f;
		postScale = 1.2f;
		burn = 3.75f;
	}
	Reinhard02ToneMap(const float preS = 1.f, const float postS = 1.2f,
			const float b = 3.75f) {
		preScale = preS;
		postScale = postS;
		burn = b;
	}

	ToneMapType GetType() const { return TONEMAP_REINHARD02; }

	ToneMap *Copy() const {
		return new Reinhard02ToneMap(preScale, postScale, burn);
	}

	void Apply(luxrays::Spectrum *pixels, std::vector<bool> &pixelsMask,
		const u_int width, const u_int height) const;

	float preScale, postScale, burn;
};

}

#endif	/* _SLG_TONEMAPPING_H */
