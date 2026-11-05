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
#include "slg/cameras/camera.h"
#include "slg/lights/trianglelight.h"
#include <unordered_map>

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Scene preprocess
//------------------------------------------------------------------------------

void Scene::PreprocessCamera(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion) {
	camera->Update(filmWidth, filmHeight, filmSubRegion);
}

// The volume of the light sources without a volume defined by the scene is
// the one including their position (otherwise the light paths of a light
// source inside a volume would ignore it, i.e. its scattering and the volume
// boundary crossings)
void Scene::UpdateLightVolumes() {
	// All the triangles of a mesh light have the same volume
	unordered_map<const SceneObject *, VolumeConstPtr> meshLightVolumes;

	for (u_int i = 0; i < lightDefs.GetSize(); ++i) {
		LightSourceRef light = lightDefs.GetLightSource(i);
		if (!light.autoVolume)
			continue;

		if (light.IsEnvironmental()) {
			// The light paths start from outside the scene
			light.volume = nullptr;
			continue;
		}

		const SceneObject *sceneObject = nullptr;
		if (light.GetType() == TYPE_TRIANGLE) {
			sceneObject = static_cast<const TriangleLight &>(light).sceneObject;
			auto it = meshLightVolumes.find(sceneObject);
			if (it != meshLightVolumes.end()) {
				light.volume = it->second;
				continue;
			}
		}

		// The origin of an emitted ray is the position of the light source
		Ray ray;
		float emissionPdfW;
		light.Emit(*this, 0.f, .5f, .5f, .5f, .5f, .5f, ray, emissionPdfW);
		light.volume = (emissionPdfW > 0.f) ? GetPointVolume(ray) :
			(HasDefaultWorldVolume() ? VolumeConstPtr(&GetDefaultWorldVolume()) : VolumeConstPtr(nullptr));

		if (sceneObject)
			meshLightVolumes[sceneObject] = light.volume;
	}
}

void Scene::Preprocess(Context& ctx, const u_int filmWidth, const u_int filmHeight,
		const u_int *filmSubRegion, const bool useRTMode) {
	//--------------------------------------------------------------------------
	// Check if I have to update geometry
	//--------------------------------------------------------------------------

	if (!dataSet || editActions.Has(GEOMETRY_EDIT) ||
			(editActions.Has(GEOMETRY_TRANS_EDIT) &&
				!dataSet->DoesAllAcceleratorsSupportUpdate())) {
		if (ctx.IsRunning()) {
			// Stop all intersection devices
			ctx.Stop();
		}

		// Rebuild the data set
		dataSet = std::make_unique<DataSet>(ctx);

		// Add all objects
		for (u_int i = 0; i < objDefs.GetSize(); ++i)
			dataSet->Add(objDefs.GetSceneObject(i).GetExtMesh());

		dataSet->Preprocess();

		// Set the LuxRays DataSet
		ctx.SetDataSet(dataSet);

		// Restart all intersection devices
		ctx.Start();
	} else if(editActions.Has(GEOMETRY_TRANS_EDIT)) {
		// I have only to update the DataSet bounding boxes
		dataSet->UpdateBBoxes();
		ctx.UpdateDataSet();
	}
	
	// Only at this point I can safely trace rays

	//--------------------------------------------------------------------------
	// Check if I have to update the camera
	//--------------------------------------------------------------------------
	
	if (editActions.Has(CAMERA_EDIT))
		PreprocessCamera(filmWidth, filmHeight, filmSubRegion);

	// Update auto-focus and auto-volume
	camera->UpdateAuto(*this);

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
		lightDefs.Preprocess(*this, useRTMode);
		UpdateLightVolumes();
	}

	// And for visibility maps
	lightDefs.UpdateVisibilityMaps(*this, useRTMode);

	//--------------------------------------------------------------------------
	// Preprocess image maps according resize policy
	//--------------------------------------------------------------------------

	imgMapCache.Preprocess(*this, useRTMode);

	//--------------------------------------------------------------------------
	// Reset the edit actions
	//--------------------------------------------------------------------------

	editActions.Reset();
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
