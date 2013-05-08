#line 2 "texture_types.cl"

/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#define DUDV_VALUE 0.001f

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY,
	// Procedural textures
	CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX, MARBLE, DOTS, BRICK, WINDY,
	WRINKLED, UV_TEX, BAND_TEX
} TextureType;

typedef struct {
	float value;
} ConstFloatParam;

typedef struct {
	Spectrum color;
} ConstFloat3Param;

typedef struct {
	unsigned int channelCount, width, height;
	unsigned int pageIndex, pixelsIndex;
} ImageMap;

typedef struct {
	TextureMapping2D mapping;
	float gain, Du, Dv;

	unsigned int imageMapIndex;
} ImageMapTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} ScaleTexParam;

typedef struct {
	unsigned int texIndex;
} FresnelApproxNTexParam;

typedef struct {
	unsigned int texIndex;
} FresnelApproxKTexParam;

typedef struct {
	TextureMapping2D mapping;
	unsigned int tex1Index, tex2Index;
} CheckerBoard2DTexParam;

typedef struct {
	TextureMapping3D mapping;
	unsigned int tex1Index, tex2Index;
} CheckerBoard3DTexParam;

typedef struct {
	unsigned int amountTexIndex, tex1Index, tex2Index;
} MixTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega;
} FBMTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega, scale, variation;
} MarbleTexParam;

typedef struct {
	TextureMapping2D mapping;
	unsigned int insideIndex, outsideIndex;
} DotsTexParam;

typedef enum {
	FLEMISH, RUNNING, ENGLISH, HERRINGBONE, BASKET, KETTING
} MasonryBond;

typedef struct {
	TextureMapping3D mapping;
	unsigned int tex1Index, tex2Index, tex3Index;
	MasonryBond bond;
	float offsetx, offsety, offsetz;
	float brickwidth, brickheight, brickdepth, mortarsize;
	float proportion, invproportion, run;
	float mortarwidth, mortarheight, mortardepth;
	float bevelwidth, bevelheight, beveldepth;
	int usebevel;
} BrickTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} AddTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega;
} WindyTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega;
} WrinkledTexParam;

typedef struct {
	TextureMapping2D mapping;
} UVTexParam;

#define BAND_TEX_MAX_SIZE 16

typedef struct {
	unsigned int amountTexIndex;
	unsigned int size;
	float offsets[BAND_TEX_MAX_SIZE];
	Spectrum values[BAND_TEX_MAX_SIZE];
} BandTexParam;

typedef struct {
	unsigned int channel;
} HitPointGreyTexParam;

typedef struct {
	TextureType type;
	union {
		ConstFloatParam constFloat;
		ConstFloat3Param constFloat3;
		ImageMapTexParam imageMapTex;
		ScaleTexParam scaleTex;
		FresnelApproxNTexParam fresnelApproxN;
		FresnelApproxKTexParam fresnelApproxK;
		CheckerBoard2DTexParam checkerBoard2D;
		CheckerBoard3DTexParam checkerBoard3D;
		MixTexParam mixTex;
		FBMTexParam fbm;
		MarbleTexParam marble;
		DotsTexParam dots;
		BrickTexParam brick;
		AddTexParam addTex;
		WindyTexParam windy;
		WrinkledTexParam wrinkled;
		UVTexParam uvTex;
		BandTexParam band;
		HitPointGreyTexParam hitPointGrey;
	};
} Texture;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(PARAM_HAS_IMAGEMAPS)

#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float **imageMapBuff
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff

#else

#define IMAGEMAPS_PARAM_DECL
#define IMAGEMAPS_PARAM

#endif

#define TEXTURES_PARAM_DECL , __global Texture *texs IMAGEMAPS_PARAM_DECL
#define TEXTURES_PARAM , texs IMAGEMAPS_PARAM
