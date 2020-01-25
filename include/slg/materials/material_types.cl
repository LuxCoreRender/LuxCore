#line 2 "material_types.cl"

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

typedef enum {
	MATTE, MIRROR, GLASS, ARCHGLASS, MIX, NULLMAT, MATTETRANSLUCENT,
	GLOSSY2, METAL2, ROUGHGLASS, VELVET, CLOTH, CARPAINT, ROUGHMATTE,
	ROUGHMATTETRANSLUCENT, GLOSSYTRANSLUCENT, GLOSSYCOATING, DISNEY,
			
	// Volumes
	HOMOGENEOUS_VOL, CLEAR_VOL, HETEROGENEOUS_VOL
} MaterialType;

typedef struct {
    unsigned int kdTexIndex;
} MatteParam;

typedef struct {
    unsigned int kdTexIndex;
    unsigned int sigmaTexIndex;
} RoughMatteParam;

typedef struct {
    unsigned int krTexIndex;
} MirrorParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
	unsigned int exteriorIorTexIndex, interiorIorTexIndex;
	unsigned int cauchyCTex;
} GlassParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int expTexIndex;
} MetalParam;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
	unsigned int exteriorIorTexIndex, interiorIorTexIndex;
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
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
	unsigned int sigmaTexIndex;
} RoughMatteTranslucentParam;

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
    unsigned int fresnelTexIndex;
    unsigned int nTexIndex;
	unsigned int kTexIndex;
	unsigned int nuTexIndex;
	unsigned int nvTexIndex;
} Metal2Param;

typedef struct {
    unsigned int krTexIndex;
	unsigned int ktTexIndex;
	unsigned int exteriorIorTexIndex, interiorIorTexIndex;
	unsigned int nuTexIndex;
	unsigned int nvTexIndex;
} RoughGlassParam;

typedef struct {
    unsigned int kdTexIndex;
	unsigned int p1TexIndex;
	unsigned int p2TexIndex;
	unsigned int p3TexIndex;
	unsigned int thicknessTexIndex;
} VelvetParam;

typedef enum {
	DENIM, SILKSHANTUNG, SILKCHARMEUSE, COTTONTWILL, WOOLGABARDINE, POLYESTER
} ClothPreset;

typedef enum {
	WARP, WEFT
} YarnType;

// Data structure describing the properties of a single yarn
typedef struct {
	// Fiber twist angle
	float psi;
	// Maximum inclination angle
	float umax;
	// Spine curvature
	float kappa;
	// Width of segment rectangle
	float width;
	// Length of segment rectangle
	float length;
	/*! u coordinate of the yarn segment center,
	 * assumes that the tile covers 0 <= u, v <= 1.
	 * (0, 0) is lower left corner of the weave pattern
	 */
	float centerU;
	// v coordinate of the yarn segment center
	float centerV;

	// Weft/Warp flag
	YarnType yarn_type;
} Yarn;

typedef struct  {
	// Size of the weave pattern
	unsigned int tileWidth, tileHeight;
	
	// Uniform scattering parameter
	float alpha;
	// Forward scattering parameter
	float beta;
	// Filament smoothing
	float ss;
	// Highlight width
	float hWidth;
	// Combined area taken up by the warp & weft
	float warpArea, weftArea;

	// Noise-related parameters
	float fineness;

	float dWarpUmaxOverDWarp;
	float dWarpUmaxOverDWeft;
	float dWeftUmaxOverDWarp;
	float dWeftUmaxOverDWeft;
	float period;
} WeaveConfig;

typedef struct {
    ClothPreset Preset;
	unsigned int Weft_KdIndex;
	unsigned int Weft_KsIndex;
	unsigned int Warp_KdIndex;
	unsigned int Warp_KsIndex;
	float Repeat_U;
	float Repeat_V;
	float specularNormalization;
} ClothParam;

typedef struct {
	unsigned int KdTexIndex;
	unsigned int Ks1TexIndex;
	unsigned int Ks2TexIndex;
	unsigned int Ks3TexIndex;
	unsigned int M1TexIndex;
	unsigned int M2TexIndex;
	unsigned int M3TexIndex;
	unsigned int R1TexIndex;
	unsigned int R2TexIndex;
	unsigned int R3TexIndex;
	unsigned int KaTexIndex;
	unsigned int depthTexIndex;
} CarPaintParam;

typedef struct {
	unsigned int kdTexIndex;
	unsigned int ktTexIndex;
	unsigned int ksTexIndex;
	unsigned int ksbfTexIndex;
	unsigned int nuTexIndex;
	unsigned int nubfTexIndex;
	unsigned int nvTexIndex;
	unsigned int nvbfTexIndex;
	unsigned int kaTexIndex;
	unsigned int kabfTexIndex;
	unsigned int depthTexIndex;
	unsigned int depthbfTexIndex;
	unsigned int indexTexIndex;
	unsigned int indexbfTexIndex;
	int multibounce;
	int multibouncebf;
} GlossyTranslucentParam;

typedef struct {
	unsigned int matBaseIndex;
	unsigned int ksTexIndex;
	unsigned int nuTexIndex;
	unsigned int nvTexIndex;
	unsigned int kaTexIndex;
	unsigned int depthTexIndex;
	unsigned int indexTexIndex;
	int multibounce;
} GlossyCoatingParam;

typedef struct {
	unsigned int baseColorTexIndex;
	unsigned int subsurfaceTexIndex;
	unsigned int roughnessTexIndex;
	unsigned int metallicTexIndex;
	unsigned int specularTexIndex;
	unsigned int specularTintTexIndex;
	unsigned int clearcoatTexIndex;
	unsigned int clearcoatGlossTexIndex;
	unsigned int anisotropicTexIndex;
	unsigned int sheenTexIndex;
	unsigned int sheenTintTexIndex;
} DisneyParam;

typedef struct {
	unsigned int sigmaATexIndex;
} ClearVolumeParam;

typedef struct {
	unsigned int sigmaATexIndex;
	unsigned int sigmaSTexIndex;
	unsigned int gTexIndex;
	int multiScattering;
} HomogenousVolumeParam;

typedef struct {
	unsigned int sigmaATexIndex;
	unsigned int sigmaSTexIndex;
	unsigned int gTexIndex;
	float stepSize;
	unsigned int maxStepsCount;
	int multiScattering;
} HeterogenousVolumeParam;

typedef struct {
	unsigned int iorTexIndex;
	// This is a different kind of emission texture from the one in
	// Material class (i.e. is not sampled by direct light).
	unsigned int volumeEmissionTexIndex;
	unsigned int volumeLightID;
	int priority;

	union {
		ClearVolumeParam clear;
		HomogenousVolumeParam homogenous;
		HeterogenousVolumeParam heterogenous;
	};
} VolumeParam;

typedef struct {
	MaterialType type;
	unsigned int matID, lightID;
	float bumpSampleDistance;
	Spectrum emittedFactor;
	float emittedCosThetaMax;
	int usePrimitiveArea;
	unsigned int frontTranspTexIndex, backTranspTexIndex;
	Spectrum passThroughShadowTransparency;
	unsigned int emitTexIndex, bumpTexIndex;
	// Type of indirect paths where a light source is visible with a direct hit. It is
	// an OR of DIFFUSE, GLOSSY and SPECULAR.
	BSDFEvent visibility;
	unsigned int interiorVolumeIndex, exteriorVolumeIndex;
	float glossiness, avgPassThroughTransparency;
	int isShadowCatcher, isShadowCatcherOnlyInfiniteLights, isPhotonGIEnabled;

	union {
		MatteParam matte;
		RoughMatteParam roughmatte;
		MirrorParam mirror;
		GlassParam glass;
		MetalParam metal;
		ArchGlassParam archglass;
		MixParam mix;
		// NULLMAT has no parameters
		MatteTranslucentParam matteTranslucent;
		RoughMatteTranslucentParam roughmatteTranslucent;
		Glossy2Param glossy2;
		Metal2Param metal2;
		RoughGlassParam roughglass;
		VelvetParam velvet;
        ClothParam cloth;
		CarPaintParam carpaint;
		GlossyTranslucentParam glossytranslucent;
		GlossyCoatingParam glossycoating;
		DisneyParam disney;
		VolumeParam volume;
	};
} Material;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define MATERIALS_PARAM_DECL , __global const Material* restrict mats TEXTURES_PARAM_DECL SCENE_PARAM_DECL
#define MATERIALS_PARAM , mats TEXTURES_PARAM SCENE_PARAM

#endif
