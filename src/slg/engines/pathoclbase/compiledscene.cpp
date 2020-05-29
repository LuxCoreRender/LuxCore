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

#include <iosfwd>
#include <limits>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "slg/engines/pathoclbase/compiledscene.h"
#include "slg/kernels/kernels.h"

using namespace std;
using namespace luxrays;
using namespace slg;

CompiledScene::CompiledScene(Scene *scn, const PathTracer *pt) {
	scene = scn;
	pathTracer = pt;
	maxMemPageSize = numeric_limits<size_t>::max();

	lightsDistribution = NULL;
	infiniteLightSourcesDistribution = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete[] lightsDistribution;
	delete[] infiniteLightSourcesDistribution;
}

void CompiledScene::SetMaxMemPageSize(const size_t maxSize) {
	maxMemPageSize = maxSize;
}

void CompiledScene::EnableCode(const std::string &tags) {
	SLG_LOG("Always enabled OpenCL code: " + tags);
	boost::split(enabledCode, tags, boost::is_any_of(" \t"));
}

void CompiledScene::Compile() {
	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	wasCameraCompiled = false;
	wasGeometryCompiled = false;
	wasMaterialsCompiled = false;
	wasSceneObjectsCompiled = false;
	wasLightsCompiled = false;
	wasImageMapsCompiled = false;
	wasPhotonGICompiled = false;

	if (editActions.Has(CAMERA_EDIT))
		CompileCamera();
	// GEOMETRY_TRANS_EDIT is also handled in RenderEngine::EndSceneEdit() if
	// accelerators support updates but still need to update transformations
	// inside mesh description here.
	if (editActions.Has(GEOMETRY_EDIT) || editActions.Has(GEOMETRY_TRANS_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileMaterials();
	if (editActions.Has(GEOMETRY_EDIT) || editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileSceneObjects();
	// GEOMETRY_EDIT and GEOMETRY_TRANS_EDIT are included here because a triangle
	// area light may have been edited
	if (editActions.Has(GEOMETRY_EDIT) || editActions.Has(GEOMETRY_TRANS_EDIT) ||
			editActions.Has(LIGHTS_EDIT) || editActions.Has(LIGHT_TYPES_EDIT))
		CompileLights();
	if (editActions.Has(IMAGEMAPS_EDIT))
		CompileImageMaps();

	if (wasGeometryCompiled || wasMaterialsCompiled || wasSceneObjectsCompiled ||
			wasLightsCompiled || wasImageMapsCompiled)
		CompilePathTracer();
	
	// For some debugging
//	cout << "=========================================================\n";
//	cout << GetTexturesEvaluationSourceCode();
//	cout << "=========================================================\n";
//	cout << GetMaterialsEvaluationSourceCode();
//	cout << "=========================================================\n";
}

string CompiledScene::ToOCLString(const slg::ocl::Spectrum &v) {
	return "(float3)(" + ToString(v.c[0]) + ", " + ToString(v.c[1]) + ", " + ToString(v.c[2]) + ")";
}

#endif
