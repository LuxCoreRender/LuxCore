#line 2 "texture_types.cl"

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
	CONST_FLOAT, CONST_FLOAT3, CONST_FLOAT4, IMAGEMAP
} TextureType;

typedef struct {
	float value;
} ConstFloatParam;

typedef struct {
	Spectrum color;
} ConstFloat3Param;

typedef struct {
	Spectrum color;
	float alpha;
} ConstFloat4Param;

typedef struct {
	unsigned int channelCount, width, height;
	unsigned int pageIndex, pixelsIndex;
} ImageMap;

typedef struct {
	float gain, uScale, vScale, uDelta, vDelta;
	float Du, Dv;

	unsigned int imageMapIndex;
} ImageMapInstanceParam;

typedef struct {
	TextureType type;
	union {
		ConstFloatParam constFloat;
		ConstFloat3Param constFloat3;
		ConstFloat4Param constFloat4;
		ImageMapInstanceParam imageMapInstance;
	};
} Texture;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)

#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float *imageMapBuff0, __global float *imageMapBuff1, __global float *imageMapBuff2, __global float *imageMapBuff3, __global float *imageMapBuff4
#elif  defined(PARAM_IMAGEMAPS_PAGE_3)
#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float *imageMapBuff0, __global float *imageMapBuff1, __global float *imageMapBuff2, __global float *imageMapBuff3
#elif  defined(PARAM_IMAGEMAPS_PAGE_2)
#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float *imageMapBuff0, __global float *imageMapBuff1, __global float *imageMapBuff2
#elif  defined(PARAM_IMAGEMAPS_PAGE_1)
#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float *imageMapBuff0, __global float *imageMapBuff1
#elif  defined(PARAM_IMAGEMAPS_PAGE_0)
#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float *imageMapBuff0
#endif

#if defined(PARAM_IMAGEMAPS_PAGE_4)
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff0, imageMapBuff1, imageMapBuff2, imageMapBuff3, imageMapBuff4
#elif  defined(PARAM_IMAGEMAPS_PAGE_3)
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff0, imageMapBuff1, imageMapBuff2, imageMapBuff3
#elif  defined(PARAM_IMAGEMAPS_PAGE_2)
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff0, imageMapBuff1, imageMapBuff2
#elif  defined(PARAM_IMAGEMAPS_PAGE_1)
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff0, imageMapBuff1
#elif  defined(PARAM_IMAGEMAPS_PAGE_0)
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff0
#endif

#else

#define IMAGEMAPS_PARAM_DECL

#endif
