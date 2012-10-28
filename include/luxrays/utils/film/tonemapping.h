/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#ifndef _LUXRAYS_UTILS_TONEMAPPING_H
#define	_LUXRAYS_UTILS_TONEMAPPING_H

namespace luxrays {

//------------------------------------------------------------------------------
// Tonemapping
//------------------------------------------------------------------------------

typedef enum {
	NONE, TONEMAP_LINEAR, TONEMAP_REINHARD02
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

//------------------------------------------------------------------------------
// Filtering
//------------------------------------------------------------------------------

typedef enum {
	FILTER_NONE, FILTER_PREVIEW, FILTER_GAUSSIAN
} FilterType;

}

#endif	/* _LUXRAYS_UTILS_TONEMAPPING_H */
