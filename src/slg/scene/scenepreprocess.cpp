/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
		const u_int *filmSubRegion, const bool useVisibilityMap) {
	if (lightDefs.GetSize() == 0) {
		throw runtime_error("The scene doesn't include any light source (note: volume emission doesn't count for this check)");

		// There may be only a volume emitting light. However I ignore this case
		// because a lot of code has been written assuming that there is always
		// at least one light source (i.e. for direct light sampling).
		/*bool hasEmittingVolume = false;
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (vol && vol->GetVolumeEmissionTexture() &&
					(vol->GetVolumeEmissionTexture()->Y() > 0.f)) {
				hasEmittingVolume = true;
				break;
			}
		}

		if (!hasEmittingVolume)
			throw runtime_error("The scene doesn't include any light source");*/
	}

	//--------------------------------------------------------------------------
	// Check if I have to stop the LuxRays Context
	//--------------------------------------------------------------------------

	bool contextStopped;
	if (editActions.Has(GEOMETRY_EDIT) ||
			(editActions.Has(GEOMETRY_TRANS_EDIT) &&
			!dataSet->DoesAllAcceleratorsSupportUpdate())) {
		// Stop all intersection devices
		ctx->Stop();

		// To avoid reference to the DataSet de-allocated inside UpdateDataSet()
		ctx->SetDataSet(NULL);
		
		contextStopped = true;
	} else
		contextStopped = !ctx->IsRunning();

	//--------------------------------------------------------------------------
	// Check if I have to update the camera
	//--------------------------------------------------------------------------
	
	if (editActions.Has(CAMERA_EDIT))
		PreprocessCamera(filmWidth, filmHeight, filmSubRegion);

	//--------------------------------------------------------------------------
	// Check if I have to rebuild the dataset
	//--------------------------------------------------------------------------

	if (editActions.Has(GEOMETRY_EDIT) || (editActions.Has(GEOMETRY_TRANS_EDIT) &&
			!dataSet->DoesAllAcceleratorsSupportUpdate())) {
		// Rebuild the data set
		delete dataSet;
		dataSet = new DataSet(ctx);

		// Add all objects
		for (u_int i = 0; i < objDefs.GetSize(); ++i)
			dataSet->Add(objDefs.GetSceneObject(i)->GetExtMesh());

		dataSet->Preprocess();
	} else if(editActions.Has(GEOMETRY_TRANS_EDIT)) {
		// I have only to update the DataSet bounding boxes
		dataSet->UpdateBBoxes();
	}

	//--------------------------------------------------------------------------
	// Update the scene bounding sphere
	//--------------------------------------------------------------------------
	
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
		lightDefs.Preprocess(this);
	}

	//--------------------------------------------------------------------------
	// Check if I have to start the context
	//--------------------------------------------------------------------------

	if (contextStopped) {
		// Set the LuxRays DataSet
		ctx->SetDataSet(dataSet);

		// Restart all intersection devices
		ctx->Start();
	} else if (dataSet->DoesAllAcceleratorsSupportUpdate() &&
			editActions.Has(GEOMETRY_TRANS_EDIT)) {
		// Update the DataSet
		ctx->UpdateDataSet();
	}
	
	//--------------------------------------------------------------------------
	// Only at this point I can safely trace rays
	//--------------------------------------------------------------------------
	
	// For the auto-focus ray and auto-volume
	if (editActions.Has(CAMERA_EDIT))
		camera->UpdateAuto(this);

	// And fir visibility maps
	if (useVisibilityMap)
		lightDefs.UpdateVisibilityMaps(this);

	//--------------------------------------------------------------------------
	// Reset the edit actions
	//--------------------------------------------------------------------------

	editActions.Reset();	
}
