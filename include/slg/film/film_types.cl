#line 2 "film_types.cl"

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
// Film
//------------------------------------------------------------------------------

typedef struct {
	unsigned int radianceGroupCount;
	int bcdDenoiserEnable, usePixelAtomics;
	
	// Film channels (AOVs)
	int hasChannelAlpha;
	int hasChannelDepth;
	int hasChannelPosition;
	int hasChannelGeometryNormal;
	int hasChannelShadingNormal;
	int hasChannelMaterialID;
	int hasChannelDirectDiffuse;
	int hasChannelDirectDiffuseReflect;
	int hasChannelDirectDiffuseTransmit;
	int hasChannelDirectGlossy;
	int hasChannelDirectGlossyReflect;
	int hasChannelDirectGlossyTransmit;
	int hasChannelEmission;
	int hasChannelIndirectDiffuse;
	int hasChannelIndirectDiffuseReflect;
	int hasChannelIndirectDiffuseTransmit;
	int hasChannelIndirectGlossy;
	int hasChannelIndirectGlossyReflect;
	int hasChannelIndirectGlossyTransmit;
	int hasChannelIndirectSpecular;
	int hasChannelIndirectSpecularReflect;
	int hasChannelIndirectSpecularTransmit;
	int hasChannelMaterialIDMask;
	unsigned int channelMaterialIDMask;
	int hasChannelDirectShadowMask;
	int hasChannelIndirectShadowMask;
	int hasChannelUV;
	int hasChannelRayCount;
	int hasChannelByMaterialID;
	unsigned int channelByMaterialID;
	int hasChannelIrradiance;
	int hasChannelObjectID;
	int hasChannelObjectIDMask;
	int channelObjectIDMask;
	int hasChannelByObjectID;
	unsigned int channelByObjectID;
	int hasChannelSampleCount;
	int hasChannelConvergence;
	int hasChannelMaterialIDColor;
	int hasChannelAlbedo;
	int hasChannelAvgShadingNormal;
	int hasChannelNoise;
	int hasChannelUserImportance;
} Film;

//------------------------------------------------------------------------------
// Some macro trick in order to have more readable code
//------------------------------------------------------------------------------

#if defined(SLG_OPENCL_KERNEL)

#define FILM_DENOISER_PARAM_DECL \
		, const int filmDenoiserWarmUpDone \
		, const float filmDenoiserGamma \
		, const float filmDenoiserMaxValue \
		, const float filmDenoiserSampleScale \
		, const uint filmDenoiserNbOfBins \
		, __global float *filmDenoiserNbOfSamplesImage \
		, __global float *filmDenoiserSquaredWeightSumsImage \
		, __global float *filmDenoiserMeanImage \
		, __global float *filmDenoiserCovarImage \
		, __global float *filmDenoiserHistoImage \
		, float3 filmRadianceGroupScale[FILM_MAX_RADIANCE_GROUP_COUNT]
#define FILM_DENOISER_PARAM \
		, filmDenoiserWarmUpDone \
		, filmDenoiserGamma \
		, filmDenoiserMaxValue \
		, filmDenoiserSampleScale \
		, filmDenoiserNbOfBins \
		, filmDenoiserNbOfSamplesImage \
		, filmDenoiserSquaredWeightSumsImage \
		, filmDenoiserMeanImage \
		, filmDenoiserCovarImage \
		, filmDenoiserHistoImage \
		, filmRadianceGroupScale

#define FILM_PARAM_DECL \
	, __constant const Film* restrict film \
	, const uint filmWidth, const uint filmHeight \
	, const uint filmSubRegion0, const uint filmSubRegion1, const uint filmSubRegion2, const uint filmSubRegion3 \
	, __global float **filmRadianceGroup \
	, __global float *filmAlpha \
	, __global float *filmDepth \
	, __global float *filmPosition \
	, __global float *filmGeometryNormal \
	, __global float *filmShadingNormal \
	, __global uint *filmMaterialID \
	, __global float *filmDirectDiffuse \
	, __global float *filmDirectDiffuseReflect \
	, __global float *filmDirectDiffuseTransmit \
	, __global float *filmDirectGlossy \
	, __global float *filmDirectGlossyReflect \
	, __global float *filmDirectGlossyTransmit \
	, __global float *filmEmission \
	, __global float *filmIndirectDiffuse \
	, __global float *filmIndirectDiffuseReflect \
	, __global float *filmIndirectDiffuseTransmit \
	, __global float *filmIndirectGlossy \
	, __global float *filmIndirectGlossyReflect \
	, __global float *filmIndirectGlossyTransmit \
	, __global float *filmIndirectSpecular \
	, __global float *filmIndirectSpecularReflect \
	, __global float *filmIndirectSpecularTransmit \
	, __global float *filmMaterialIDMask \
	, __global float *filmDirectShadowMask \
	, __global float *filmIndirectShadowMask \
	, __global float *filmUV \
	, __global float *filmRayCount \
	, __global float *filmByMaterialID \
	, __global float *filmIrradiance \
	, __global uint *filmObjectID \
	, __global float *filmObjectIDMask \
	, __global float *filmByObjectID \
	, __global uint *filmSampleCount \
	, __global float *filmConvergence \
	, __global float *filmMaterialIDColor \
	, __global float *filmAlbedo \
	, __global float *filmAvgShadingNormal \
	, __global float *filmNoise \
	, __global float *filmUserImportance \
	FILM_DENOISER_PARAM_DECL

#define FILM_PARAM \
	, film \
	, filmWidth, filmHeight \
	, filmSubRegion0, filmSubRegion1, filmSubRegion2, filmSubRegion3 \
	, filmRadianceGroup \
	, filmAlpha \
	, filmDepth \
	, filmPosition \
	, filmGeometryNormal \
	, filmShadingNormal \
	, filmMaterialID \
	, filmDirectDiffuse \
	, filmDirectDiffuseReflect \
	, filmDirectDiffuseTransmit \
	, filmDirectGlossy \
	, filmDirectGlossyReflect \
	, filmDirectGlossyTransmit \
	, filmEmission \
	, filmIndirectDiffuse \
	, filmIndirectDiffuseReflect \
	, filmIndirectDiffuseTransmit \
	, filmIndirectGlossy \
	, filmIndirectGlossyReflect \
	, filmIndirectGlossyTransmit \
	, filmIndirectSpecular \
	, filmIndirectSpecularReflect \
	, filmIndirectSpecularTransmit \
	, filmMaterialIDMask \
	, filmDirectShadowMask \
	, filmIndirectShadowMask \
	, filmUV \
	, filmRayCount \
	, filmByMaterialID \
	, filmIrradiance \
	, filmObjectID \
	, filmObjectIDMask \
	, filmByObjectID \
	, filmSampleCount \
	, filmConvergence \
	, filmMaterialIDColor \
	, filmAlbedo \
	, filmAvgShadingNormal \
	, filmNoise \
	, filmUserImportance \
	FILM_DENOISER_PARAM

#endif
