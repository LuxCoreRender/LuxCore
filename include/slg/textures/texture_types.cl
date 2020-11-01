#line 2 "texture_types.cl"

/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)                        *
 *                                                                         *
 *   This file is part of LuxCoreRender.                                   *
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

//------------------------------------------------------------------------------
// Texture evaluation op
//------------------------------------------------------------------------------

typedef enum {
	EVAL_FLOAT,
	EVAL_SPECTRUM,
	EVAL_BUMP,
	// For the very special case of Triplanar texture
	EVAL_TRIPLANAR_STEP_1,
	EVAL_TRIPLANAR_STEP_2,
	EVAL_TRIPLANAR_STEP_3,
	// For the very special case of Distort texture
	EVAL_DISTORT_SETUP,
	// For the very special case of Bombing texture
	EVAL_BOMBING_SETUP_11,
	EVAL_BOMBING_SETUP_10,
	EVAL_BOMBING_SETUP_01,
	EVAL_BOMBING_SETUP_00,
	// For evaluting generic bump mapping
	EVAL_BUMP_GENERIC_OFFSET_U,
	EVAL_BUMP_GENERIC_OFFSET_V,
	// For the very special case of Triplanar texture
	EVAL_BUMP_TRIPLANAR_STEP_1,
	EVAL_BUMP_TRIPLANAR_STEP_2,
	EVAL_BUMP_TRIPLANAR_STEP_3
} TextureEvalOpType;

typedef struct {
	unsigned int texIndex;
	TextureEvalOpType evalType;
} TextureEvalOp;

//------------------------------------------------------------------------------
// Textures
//------------------------------------------------------------------------------

#define DUDV_VALUE 0.001f

typedef enum {
	CONST_FLOAT, CONST_FLOAT3, IMAGEMAP, SCALE_TEX, FRESNEL_APPROX_N,
	FRESNEL_APPROX_K, MIX_TEX, ADD_TEX, SUBTRACT_TEX, HITPOINTCOLOR, HITPOINTALPHA,
	HITPOINTGREY, NORMALMAP_TEX, BLACKBODY_TEX, IRREGULARDATA_TEX, DENSITYGRID_TEX,
	ABS_TEX, CLAMP_TEX, BILERP_TEX, COLORDEPTH_TEX, HSV_TEX, DIVIDE_TEX, REMAP_TEX,
	OBJECTID_TEX, OBJECTID_COLOR_TEX, OBJECTID_NORMALIZED_TEX, DOT_PRODUCT_TEX,
	POWER_TEX, LESS_THAN_TEX, GREATER_THAN_TEX, ROUNDING_TEX, MODULO_TEX, SHADING_NORMAL_TEX,
    POSITION_TEX, SPLIT_FLOAT3, MAKE_FLOAT3, BRIGHT_CONTRAST_TEX, HITPOINTVERTEXAOV,
	HITPOINTTRIANGLEAOV, TRIPLANAR_TEX, RANDOM_TEX, DISTORT_TEX,
	BOMBING_TEX, // 44 textures
	// Procedural textures
	BLENDER_BLEND, BLENDER_CLOUDS, BLENDER_DISTORTED_NOISE, BLENDER_MAGIC, BLENDER_MARBLE,
	BLENDER_MUSGRAVE, BLENDER_NOISE, BLENDER_STUCCI, BLENDER_WOOD,  BLENDER_VORONOI,
	CHECKERBOARD2D, CHECKERBOARD3D, CLOUD_TEX, FBM_TEX,
	MARBLE, DOTS, BRICK, WINDY, WRINKLED, UV_TEX, BAND_TEX,
	WIREFRAME_TEX, // 65 textures
	// Fresnel textures
	FRESNELCOLOR_TEX, FRESNELCONST_TEX
} TextureType;

typedef struct {
	float value;
} ConstFloatParam;

typedef struct {
	Spectrum color;
} ConstFloat3Param;

typedef struct {
	TextureMapping2D mapping;
	float gain;

	unsigned int imageMapIndex;

	int randomizedTiling;
	unsigned int randomizedTilingLUTIndex;
	unsigned int randomizedTilingInvLUTIndex;
	unsigned int randomImageMapIndex;
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
	float radius;
	unsigned int numspheres;
	float spheresize;
	float sharpness;
	float basefadedistance;
	float baseflatness;
	float variability;
	float omega;
	float noisescale;
	float noiseoffset;
	float turbulence;
	int octaves;
//	CumulusSphere *spheres;
} CloudTexParam;

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
	float modulationBias;
} BrickTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} AddTexParam;

typedef struct {
    unsigned int textureIndex;
    unsigned int incrementIndex;
} RoundingTexParam;

typedef struct {
    unsigned int textureIndex;
    unsigned int moduloIndex;
} ModuloTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega;
} WindyTexParam;

typedef enum {
	ACTUAL_DISTANCE, DISTANCE_SQUARED, MANHATTAN, CHEBYCHEV, MINKOWSKI_HALF,
	MINKOWSKI_FOUR, MINKOWSKI
} DistanceMetric;

typedef enum {
	TEX_LIN, TEX_QUAD, TEX_EASE, TEX_DIAG, TEX_SPHERE, TEX_HALO, TEX_RAD
} ProgressionType;

typedef enum {
    BLENDER_ORIGINAL, ORIGINAL_PERLIN, IMPROVED_PERLIN,
    VORONOI_F1, VORONOI_F2, VORONOI_F3, VORONOI_F4, VORONOI_F2_F1,
    VORONOI_CRACKLE, CELL_NOISE
} BlenderNoiseBasis;

typedef enum {
	TEX_SIN, TEX_SAW, TEX_TRI
} BlenderNoiseBase;

typedef struct {
	TextureMapping3D mapping;
	ProgressionType type;
	int direction;
	float contrast, bright;
} BlenderBlendTexParam;

typedef struct {
	TextureMapping3D mapping;
	BlenderNoiseBasis noisebasis;
	float noisesize;
	int noisedepth;
	float bright, contrast;
	int hard;
} BlenderCloudsTexParam;

typedef struct {
	TextureMapping3D mapping;
	BlenderNoiseBasis noisedistortion;
	BlenderNoiseBasis noisebasis;
	float distortion;
	float noisesize;
	float bright, contrast;
} BlenderDistortedNoiseTexParam;

typedef struct {
	TextureMapping3D mapping;
	int noisedepth;
	float turbulence;
	float bright, contrast;
} BlenderMagicTexParam;

typedef enum {
	TEX_SOFT, TEX_SHARP, TEX_SHARPER
} BlenderMarbleType;

typedef struct {
	TextureMapping3D mapping;
	BlenderMarbleType type;
	BlenderNoiseBasis noisebasis;
	BlenderNoiseBase noisebasis2;
	float noisesize, turbulence;
	int noisedepth;
	float bright, contrast;
	int hard;
} BlenderMarbleTexParam;

typedef enum {
	TEX_MULTIFRACTAL, TEX_RIDGED_MULTIFRACTAL, TEX_HYBRID_MULTIFRACTAL, TEX_FBM, TEX_HETERO_TERRAIN
} BlenderMusgraveType;

typedef struct {
	TextureMapping3D mapping;
	BlenderMusgraveType type;
	BlenderNoiseBasis noisebasis;
	float dimension;
	float intensity;
	float lacunarity;
	float offset;
	float gain;
	float octaves;
	float noisesize;
	float bright, contrast;
} BlenderMusgraveTexParam;

typedef struct {
	int noisedepth;
	float bright, contrast;
} BlenderNoiseTexParam;

typedef enum {
	TEX_PLASTIC, TEX_WALL_IN, TEX_WALL_OUT
} BlenderStucciType;

typedef struct {
	TextureMapping3D mapping;
	BlenderStucciType type;
	BlenderNoiseBasis noisebasis;
	float noisesize;
	float turbulence;
	float bright, contrast;
	int hard;
} BlenderStucciTexParam;

typedef enum {
	BANDS, RINGS, BANDNOISE, RINGNOISE
} BlenderWoodType;

typedef struct {
	TextureMapping3D mapping;
	BlenderWoodType type;
	BlenderNoiseBase noisebasis2;
	BlenderNoiseBasis noisebasis;
	float noisesize, turbulence;
	float bright, contrast;
	int hard;
} BlenderWoodTexParam;

typedef struct {
 	TextureMapping3D mapping;
	DistanceMetric distancemetric;
	float feature_weight1;
	float feature_weight2;
	float feature_weight3;
	float feature_weight4;
	float noisesize;
	float intensity;
	float exponent;
 	float bright, contrast;
} BlenderVoronoiTexParam;

typedef struct {
	TextureMapping3D mapping;
	int octaves;
	float omega;
} WrinkledTexParam;

typedef struct {
	TextureMapping2D mapping;
} UVTexParam;

#define BAND_TEX_MAX_SIZE 16

typedef enum {
	INTERP_NONE,
	INTERP_LINEAR,
	INTERP_CUBIC
} InterpolationType;

typedef struct {
	InterpolationType interpType;
	unsigned int amountTexIndex;
	unsigned int size;
	float offsets[BAND_TEX_MAX_SIZE];
	Spectrum values[BAND_TEX_MAX_SIZE];
} BandTexParam;


typedef struct {
	unsigned int dataIndex;
} HitPointColorTexParam;

typedef struct {
	unsigned int dataIndex;
} HitPointAlphaTexParam;

typedef struct {
	unsigned int dataIndex;
	unsigned int channelIndex;
} HitPointGreyTexParam;

typedef struct {
	unsigned int dataIndex;
} HitPointVertexAOVTexParam;

typedef struct {
	unsigned int dataIndex;
} HitPointTriangleAOVTexParam;

typedef struct {
	unsigned int texIndex;
	float scale;
} NormalMapTexParam;

typedef struct {
	Spectrum rgb;
} BlackBodyParam;

typedef struct {
	Spectrum rgb;
} IrregularDataParam;

typedef struct {
	TextureMapping3D mapping;

	unsigned int nx, ny, nz;
	unsigned int imageMapIndex;
} DensityGridParam;

typedef struct {
	unsigned int krIndex;
} FresnelColorParam;

typedef struct {
	Spectrum n, k;
} FresnelConstParam;

typedef struct {
	unsigned int texIndex;
} AbsTexParam;

typedef struct {
	unsigned int texIndex;
	float minVal, maxVal;
} ClampTexParam;

typedef struct {
	unsigned int t00Index, t01Index, t10Index, t11Index;
} BilerpTexParam;

typedef struct {
	float dVal;
	unsigned int ktIndex;
} ColorDepthTexParam;

typedef struct {
	unsigned int texIndex;
	unsigned int hueTexIndex, satTexIndex, valTexIndex;
} HsvTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} DivideTexParam;

typedef struct {
	unsigned int valueTexIndex,
	             sourceMinTexIndex, sourceMaxTexIndex,
	             targetMinTexIndex, targetMaxTexIndex;
} RemapTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} DotProductTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} GreaterThanTexParam;

typedef struct {
	unsigned int tex1Index, tex2Index;
} LessThanTexParam;

typedef struct {
	unsigned int baseTexIndex, exponentTexIndex;
} PowerTexParam;

typedef struct {
	unsigned int texIndex;
	unsigned int channelIndex;
} SplitFloat3TexParam;

typedef struct {
	unsigned int tex1Index, tex2Index, tex3Index;
} MakeFloat3TexParam;

typedef struct {
	unsigned int texIndex, brightnessTexIndex, contrastTexIndex;
} BrightContrastTexParam;

typedef struct {
	TextureMapping3D mapping;
	unsigned int tex1Index, tex2Index, tex3Index;
	int enableUVlessBumpMap;
} TriplanarTexParam;

typedef struct {
	unsigned int texIndex;
} RandomTexParam;

typedef struct {
	float width;
	unsigned int borderTexIndex, insideTexIndex;
} WireFrameTexParam;

typedef struct {
	float strength;
	unsigned int texIndex, offsetTexIndex;
} DistortTexParam;

typedef struct {
	TextureMapping2D mapping;
	float randomScaleFactor;
	int useRandomRotation;
	unsigned int multiBulletCount;
	unsigned int backgroundTex, bulletTexIndex, bulletMaskTexIndex;
	unsigned int randomImageMapIndex;
} BombingTexParam;

typedef struct {
	TextureType type;

	unsigned int evalFloatOpStartIndex, evalFloatOpLength;
	unsigned int evalSpectrumOpStartIndex, evalSpectrumOpLength;
	unsigned int evalBumpOpStartIndex, evalBumpOpLength;

	union {
		BlenderBlendTexParam blenderBlend;
 		BlenderCloudsTexParam blenderClouds;
		BlenderDistortedNoiseTexParam blenderDistortedNoise;
		BlenderMagicTexParam blenderMagic;
		BlenderMarbleTexParam blenderMarble;
		BlenderMusgraveTexParam blenderMusgrave;
		BlenderNoiseTexParam blenderNoise;
		BlenderStucciTexParam blenderStucci;
 		BlenderWoodTexParam blenderWood;
		BlenderVoronoiTexParam blenderVoronoi;
		ConstFloatParam constFloat;
		ConstFloat3Param constFloat3;
		ImageMapTexParam imageMapTex;
		ScaleTexParam scaleTex;
		FresnelApproxNTexParam fresnelApproxN;
		FresnelApproxKTexParam fresnelApproxK;
		MixTexParam mixTex;
		CheckerBoard2DTexParam checkerBoard2D;
		CheckerBoard3DTexParam checkerBoard3D;
		CloudTexParam cloud;
		FBMTexParam fbm;
		MarbleTexParam marble;
		DotsTexParam dots;
		BrickTexParam brick;
		AddTexParam addTex;
		AddTexParam subtractTex;
		WindyTexParam windy;
		WrinkledTexParam wrinkled;
		UVTexParam uvTex;
		BandTexParam band;
		HitPointColorTexParam hitPointColor;
		HitPointColorTexParam hitPointAlpha;
		HitPointGreyTexParam hitPointGrey;
		HitPointVertexAOVTexParam hitPointVertexAOV;
		HitPointTriangleAOVTexParam hitPointTriangleAOV;
		NormalMapTexParam normalMap;
		BlackBodyParam blackBody;
		IrregularDataParam irregularData;
		DensityGridParam densityGrid;
		FresnelColorParam fresnelColor;
		FresnelConstParam fresnelConst;
		AbsTexParam absTex;
		ClampTexParam clampTex;
		BilerpTexParam bilerpTex;
		ColorDepthTexParam colorDepthTex;
		HsvTexParam hsvTex;
		DivideTexParam divideTex;
		RemapTexParam remapTex;
		DotProductTexParam dotProductTex;
		GreaterThanTexParam greaterThanTex;
		LessThanTexParam lessThanTex;
		PowerTexParam powerTex;
		SplitFloat3TexParam splitFloat3Tex;
		MakeFloat3TexParam makeFloat3Tex;
		RoundingTexParam roundingTex;
		ModuloTexParam moduloTex;
		BrightContrastTexParam brightContrastTex;
		TriplanarTexParam triplanarTex;
		RandomTexParam randomTex;
		WireFrameTexParam wireFrameTex;
		DistortTexParam distortTex;
		BombingTexParam bombingTex;
	};
} Texture;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define TEXTURES_PARAM_DECL \
	, __global const Texture* restrict texs \
	, __global const TextureEvalOp* restrict texEvalOps \
	, __global float *texEvalStacks \
	, const uint maxTextureEvalStackSize \
	IMAGEMAPS_PARAM_DECL SCENE_PARAM_DECL
#define TEXTURES_PARAM \
	, texs \
	, texEvalOps \
	, texEvalStacks \
	, maxTextureEvalStackSize \
	IMAGEMAPS_PARAM SCENE_PARAM

#endif
