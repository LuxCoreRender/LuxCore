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

void CompiledScene::CompileSceneObjects() {
	wasSceneObjectsCompiled = true;

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const u_int objCount = scene->objDefs.GetSize();
	sceneObjs.resize(objCount);
	for (u_int i = 0; i < objCount; ++i) {
		slg::ocl::SceneObject &oclScnObj = sceneObjs[i];
		const SceneObject *scnObj = scene->objDefs.GetSceneObject(i);

		oclScnObj.objectID = scnObj->GetID();

		const Material *m = scnObj->GetMaterial();
		oclScnObj.materialIndex = scene->matDefs.GetMaterialIndex(m);

		const ImageMap *bakeMap = scnObj->GetBakeMap();
		if (bakeMap) {
			oclScnObj.bakeMapIndex = scene->imgMapCache.GetImageMapIndex(bakeMap);
			switch (scnObj->GetBakeMapType()) {
				case COMBINED:
					oclScnObj.bakeMapType = slg::ocl::COMBINED;
					break;
				case LIGHTMAP:
					oclScnObj.bakeMapType = slg::ocl::LIGHTMAP;
					break;
				default:
					throw runtime_error("Unknown bake map type in CompiledScene::CompileSceneObjects(): " + ToString(scnObj->GetBakeMapType()));
			}
			oclScnObj.bakeMapUVIndex = scnObj->GetBakeMapUVIndex();
		} else {
			oclScnObj.bakeMapIndex = NULL_INDEX;
			oclScnObj.bakeMapUVIndex = NULL_INDEX;
		}

		oclScnObj.cameraInvisible = scnObj->IsCameraInvisible();
	}
}

#endif
