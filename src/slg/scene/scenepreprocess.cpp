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

#include "luxrays/core/dataset.h"
#include "luxrays/core/intersectiondevice.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scene preprocess
//------------------------------------------------------------------------------

void Scene::PreprocessCamera(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion) {
	camera->Update(filmWidth, filmHeight, filmSubRegion);
}

void Scene::Preprocess(Context *ctx, const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion, const bool useRTMode) {
	//--------------------------------------------------------------------------
	// Check if I have to update geometry
	//--------------------------------------------------------------------------

	if (!dataSet || editActions.Has(GEOMETRY_EDIT) ||
			(editActions.Has(GEOMETRY_TRANS_EDIT) &&
				!dataSet->DoesAllAcceleratorsSupportUpdate())) {
		if (ctx->IsRunning()) {
			// Stop all intersection devices
			ctx->Stop();
		}

		// Rebuild the data set
		delete dataSet;
		dataSet = new DataSet(ctx);

		// Add all objects
		for (u_int i = 0; i < objDefs.GetSize(); ++i)
			dataSet->Add(objDefs.GetSceneObject(i)->GetExtMesh());

		dataSet->Preprocess();

		// Set the LuxRays DataSet
		ctx->SetDataSet(dataSet);

		// Restart all intersection devices
		ctx->Start();
	} else if(editActions.Has(GEOMETRY_TRANS_EDIT)) {
		// I have only to update the DataSet bounding boxes
		dataSet->UpdateBBoxes();
		ctx->UpdateDataSet();
	}
	
	// Only at this point I can safely trace rays

	//--------------------------------------------------------------------------
	// Check if I have to update the camera
	//--------------------------------------------------------------------------
	
	if (editActions.Has(CAMERA_EDIT))
		PreprocessCamera(filmWidth, filmHeight, filmSubRegion);

	// Update auto-focus and auto-volume
	camera->UpdateAuto(this);

	// At this point, both the data set and the camera are updated
	const BBox sceneBBox = Union(dataSet->GetBBox(), camera->GetBBox());
	sceneBSphere = sceneBBox.BoundingSphere();		
	
	//--------------------------------------------------------------------------
	// Check if something has changed in light sources
	//--------------------------------------------------------------------------

	if (editActions.Has(GEOMETRY_EDIT) ||
			editActions.Has(GEOMETRY_TRANS_EDIT) ||
			editActions.Has(MATERIALS_EDIT) ||
			editActions.Has(MATERIAL_TYPES_EDIT) ||
			editActions.Has(LIGHTS_EDIT) ||
			editActions.Has(LIGHT_TYPES_EDIT) ||
			editActions.Has(IMAGEMAPS_EDIT)) {
		lightDefs.Preprocess(this, useRTMode);
	}

	// And for visibility maps
	lightDefs.UpdateVisibilityMaps(this, useRTMode);

	//--------------------------------------------------------------------------
	// Reset the edit actions
	//--------------------------------------------------------------------------

	editActions.Reset();
}
