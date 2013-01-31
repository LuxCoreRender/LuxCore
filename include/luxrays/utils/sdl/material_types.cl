#line 2 "material_types.cl"

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

typedef enum {
	MATTE, MIRROR, GLASS, METAL, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2
} MaterialType;

typedef struct {
    unsigned int kdTexIndex;
} MatteParam;

typedef struct {
    unsigned int krTexIndex;
} MirrorParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
	unsigned int ousideIorTexIndex, iorTexIndex;
} GlassParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int expTexIndex;
} MetalParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
} ArchGlassParam;

typedef struct {
	unsigned int matAIndex, matBIndex;
	unsigned int mixFactorTexIndex;
} MixParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
} MatteTranslucentParam;

typedef struct {
    unsigned int kdTexIndex;
	unsigned int ksTexIndex;
	unsigned int nuTexIndex;
	unsigned int nvTexIndex;
	unsigned int kaTexIndex;
	unsigned int depthTexIndex;
	unsigned int indexTexIndex;
	int multibounce;
} Glossy2Param;

typedef struct {
    unsigned int etaTexIndex;
	unsigned int kTexIndex;
	unsigned int nuTexIndex;
	unsigned int nvTexIndex;
} Metal2Param;

typedef struct {
	MaterialType type;
	unsigned int emitTexIndex, bumpTexIndex, normalTexIndex;
	union {
		MatteParam matte;
		MirrorParam mirror;
		GlassParam glass;
		MetalParam metal;
		ArchGlassParam archglass;
		MixParam mix;
		// NULLMAT has no parameters
		MatteTranslucentParam matteTranslucent;
		Glossy2Param glossy2;
		Metal2Param metal2;
	};
} Material;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#define MATERIALS_PARAM_DECL , __global Material *mats TEXTURES_PARAM_DECL
#define MATERIALS_PARAM , mats TEXTURES_PARAM
