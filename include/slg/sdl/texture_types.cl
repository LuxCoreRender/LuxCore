#line 2 "texture_types.cl"

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

#define DUDV_VALUE 0.001f

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY, NORMALMAP_TEX,
	// Procedural textures
	BLENDER_CLOUDS, BLENDER_WOOD, CHECKERBOARD2D, CHECKERBOARD3D, FBM_TEX,
	MARBLE, DOTS, BRICK, WINDY, WRINKLED, UV_TEX, BAND_TEX
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
	float gain;

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
	unsigned int amountTexIndex, tex1Index, tex2Index;
} MixTexParam;

typedef struct {
	TextureMapping2D mapping;
	unsigned int tex1Index, tex2Index;
} CheckerBoard2DTexParam;

typedef struct {
	TextureMapping3D mapping;
	unsigned int tex1Index, tex2Index;
} CheckerBoard3DTexParam;

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

typedef enum {
	BANDS, RINGS, BANDNOISE, RINGNOISE
} BlenderWoodType;

typedef enum {
	TEX_SIN, TEX_SAW, TEX_TRI
} BlenderWoodNoiseBase;

typedef struct {
	TextureMapping3D mapping;
	BlenderWoodType type;
	BlenderWoodNoiseBase noisebasis2;
	float noisesize, turbulence;
	float bright, contrast;
	int hard;
} BlenderWoodTexParam;

typedef struct {
	TextureMapping3D mapping;
	float noisesize;
	int noisedepth;
	float bright, contrast;
	int hard;
} BlenderCloudsTexParam;

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
	unsigned int texIndex;
} NormalMapTexParam;

typedef struct {
	TextureType type;
	union {
		BlenderCloudsTexParam blenderClouds;
		BlenderWoodTexParam blenderWood;
		ConstFloatParam constFloat;
		ConstFloat3Param constFloat3;
		ImageMapTexParam imageMapTex;
		ScaleTexParam scaleTex;
		FresnelApproxNTexParam fresnelApproxN;
		FresnelApproxKTexParam fresnelApproxK;
		MixTexParam mixTex;
		CheckerBoard2DTexParam checkerBoard2D;
		CheckerBoard3DTexParam checkerBoard3D;
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
        NormalMapTexParam normalMap;
	};
} Texture;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#if defined(PARAM_HAS_IMAGEMAPS)

#define IMAGEMAPS_PARAM_DECL , __global ImageMap *imageMapDescs, __global float **imageMapBuff
#define IMAGEMAPS_PARAM , imageMapDescs, imageMapBuff

#else

#define IMAGEMAPS_PARAM_DECL
#define IMAGEMAPS_PARAM

#endif

#define TEXTURES_PARAM_DECL , __global Texture *texs IMAGEMAPS_PARAM_DECL
#define TEXTURES_PARAM , texs IMAGEMAPS_PARAM

#endif
