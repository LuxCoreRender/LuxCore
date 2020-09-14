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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include "slg/engines/pathoclbase/compiledscene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void CompiledScene::CompileFilm(const Film &film, slg::ocl::Film &oclFilm) {
	oclFilm.radianceGroupCount = film.GetRadianceGroupCount();
	oclFilm.bcdDenoiserEnable = film.GetDenoiser().IsEnabled();
	oclFilm.usePixelAtomics = false;

	// Film channels (AOVs)

	oclFilm.hasChannelAlpha = film.HasChannel(Film::ALPHA);
	oclFilm.hasChannelDepth = film.HasChannel(Film::DEPTH);
	oclFilm.hasChannelPosition = film.HasChannel(Film::POSITION);
	oclFilm.hasChannelGeometryNormal = film.HasChannel(Film::GEOMETRY_NORMAL);
	oclFilm.hasChannelShadingNormal = film.HasChannel(Film::SHADING_NORMAL);
	oclFilm.hasChannelMaterialID = film.HasChannel(Film::MATERIAL_ID);
	oclFilm.hasChannelDirectDiffuse = film.HasChannel(Film::DIRECT_DIFFUSE);
	oclFilm.hasChannelDirectDiffuseReflect = film.HasChannel(Film::DIRECT_DIFFUSE_REFLECT);
	oclFilm.hasChannelDirectDiffuseTransmit = film.HasChannel(Film::DIRECT_DIFFUSE_TRANSMIT);
	oclFilm.hasChannelDirectGlossy = film.HasChannel(Film::DIRECT_GLOSSY);
	oclFilm.hasChannelDirectGlossyReflect = film.HasChannel(Film::DIRECT_GLOSSY_REFLECT);
	oclFilm.hasChannelDirectGlossyTransmit = film.HasChannel(Film::DIRECT_GLOSSY_TRANSMIT);
	oclFilm.hasChannelEmission = film.HasChannel(Film::EMISSION);
	oclFilm.hasChannelIndirectDiffuse = film.HasChannel(Film::INDIRECT_DIFFUSE);
	oclFilm.hasChannelIndirectDiffuseReflect = film.HasChannel(Film::INDIRECT_DIFFUSE_REFLECT);
	oclFilm.hasChannelIndirectDiffuseTransmit = film.HasChannel(Film::INDIRECT_DIFFUSE_TRANSMIT);
	oclFilm.hasChannelIndirectGlossy = film.HasChannel(Film::INDIRECT_GLOSSY);
	oclFilm.hasChannelIndirectGlossyReflect = film.HasChannel(Film::INDIRECT_GLOSSY_REFLECT);
	oclFilm.hasChannelIndirectGlossyTransmit = film.HasChannel(Film::INDIRECT_GLOSSY_TRANSMIT);
	oclFilm.hasChannelIndirectSpecular = film.HasChannel(Film::INDIRECT_SPECULAR);
	oclFilm.hasChannelIndirectSpecularReflect = film.HasChannel(Film::INDIRECT_SPECULAR_REFLECT);
	oclFilm.hasChannelIndirectSpecularTransmit = film.HasChannel(Film::INDIRECT_SPECULAR_TRANSMIT);
	oclFilm.hasChannelMaterialIDMask = film.HasChannel(Film::MATERIAL_ID_MASK);
	if (oclFilm.hasChannelMaterialIDMask)
		oclFilm.channelMaterialIDMask = film.GetMaskMaterialID(0);
	else
		oclFilm.channelMaterialIDMask = NULL_INDEX;
	oclFilm.hasChannelDirectShadowMask = film.HasChannel(Film::DIRECT_SHADOW_MASK);
	oclFilm.hasChannelIndirectShadowMask = film.HasChannel(Film::INDIRECT_SHADOW_MASK);
	oclFilm.hasChannelUV = film.HasChannel(Film::UV);
	oclFilm.hasChannelRayCount = film.HasChannel(Film::RAYCOUNT);
	oclFilm.hasChannelByMaterialID = film.HasChannel(Film::BY_MATERIAL_ID);
	if (oclFilm.hasChannelByMaterialID)
		oclFilm.channelByMaterialID = film.GetByMaterialID(0);
	else
		oclFilm.channelByMaterialID = NULL_INDEX;
	oclFilm.hasChannelIrradiance = film.HasChannel(Film::IRRADIANCE);
	oclFilm.hasChannelObjectID = film.HasChannel(Film::OBJECT_ID);
	oclFilm.hasChannelObjectIDMask = film.HasChannel(Film::OBJECT_ID_MASK);
	if (oclFilm.hasChannelObjectIDMask)
		oclFilm.channelObjectIDMask = film.GetMaskObjectID(0);
	else
		oclFilm.channelObjectIDMask = NULL_INDEX;
	oclFilm.hasChannelByObjectID = film.HasChannel(Film::BY_OBJECT_ID);
	if (oclFilm.hasChannelByObjectID)
		oclFilm.channelByObjectID = film.GetByObjectID(0);
	else
		oclFilm.channelByObjectID = NULL_INDEX;
	oclFilm.hasChannelSampleCount = film.HasChannel(Film::SAMPLECOUNT);
	oclFilm.hasChannelConvergence = film.HasChannel(Film::CONVERGENCE);
	oclFilm.hasChannelMaterialIDColor = film.HasChannel(Film::MATERIAL_ID_COLOR);
	oclFilm.hasChannelAlbedo = film.HasChannel(Film::ALBEDO);
	oclFilm.hasChannelAvgShadingNormal = film.HasChannel(Film::AVG_SHADING_NORMAL);
	oclFilm.hasChannelNoise = film.HasChannel(Film::NOISE);
	oclFilm.hasChannelUserImportance = film.HasChannel(Film::USER_IMPORTANCE);
}

#endif
