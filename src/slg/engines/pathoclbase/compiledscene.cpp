/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

CompiledScene::CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS) {
	scene = scn;
	film = flm;
	maxMemPageSize = (u_int)Min<size_t>(maxMemPageS, 0xffffffffu);

	lightsDistribution = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete[] lightsDistribution;
}

string CompiledScene::ToOCLString(const slg::ocl::Spectrum &v) {
	return "(float3)(" + ToString(v.c[0]) + ", " + ToString(v.c[1]) + ", " + ToString(v.c[2]) + ")";
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileMaterials();
	if (editActions.Has(GEOMETRY_EDIT) || editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileSceneObjects();
	if (editActions.Has(LIGHTS_EDIT) || editActions.Has(LIGHT_TYPES_EDIT))
		CompileLights();
	if (editActions.Has(IMAGEMAPS_EDIT))
		CompileImageMaps();

	// For some debugging
//	cout << "=========================================================\n";
//	cout << GetTexturesEvaluationSourceCode();
//	cout << "=========================================================\n";
//	cout << GetMaterialsEvaluationSourceCode();
//	cout << "=========================================================\n";
}

bool CompiledScene::IsMaterialCompiled(const MaterialType type) const {
	return (usedMaterialTypes.find(type) != usedMaterialTypes.end());
}

bool CompiledScene::IsTextureCompiled(const TextureType type) const {
	return (usedTextureTypes.find(type) != usedTextureTypes.end());
}

bool CompiledScene::IsImageMapFormatCompiled(const ImageMapStorage::StorageType type) const {
	return (usedImageMapFormats.find(type) != usedImageMapFormats.end());
}

bool CompiledScene::IsImageMapChannelCountCompiled(const u_int count) const {
	return (usedImageMapChannels.find(count) != usedImageMapChannels.end());
}

bool CompiledScene::HasBumpMaps() const {
	return useBumpMapping;
}

bool CompiledScene::RequiresPassThrough() const {
	return (IsMaterialCompiled(GLASS) ||
			IsMaterialCompiled(ARCHGLASS) ||
			IsMaterialCompiled(MIX) ||
			IsMaterialCompiled(NULLMAT) ||
			IsMaterialCompiled(MATTETRANSLUCENT) ||
			IsMaterialCompiled(ROUGHMATTETRANSLUCENT) ||
			IsMaterialCompiled(GLOSSY2) ||
			IsMaterialCompiled(ROUGHGLASS) ||
			IsMaterialCompiled(CARPAINT) ||
			IsMaterialCompiled(GLOSSYTRANSLUCENT) ||
			IsMaterialCompiled(GLOSSYCOATING) ||
			IsMaterialCompiled(CLEAR_VOL) ||
			IsMaterialCompiled(HOMOGENEOUS_VOL) ||
			IsMaterialCompiled(HETEROGENEOUS_VOL));
}

bool CompiledScene::HasVolumes() const {
	return IsMaterialCompiled(HOMOGENEOUS_VOL) ||
			IsMaterialCompiled(CLEAR_VOL) ||
			IsMaterialCompiled(HETEROGENEOUS_VOL) ||
			// Volume rendering may be required to evaluate the IOR
			IsMaterialCompiled(GLASS) ||
			IsMaterialCompiled(ARCHGLASS) ||
			IsMaterialCompiled(ROUGHGLASS);
}

#endif
