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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "slg/scene/scene.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"

#include "slg/volumes/clear.h"
#include "slg/volumes/heterogenous.h"
#include "slg/volumes/homogenous.h"

#include <atomic>

using namespace std;
using namespace luxrays;
using namespace slg;

namespace slg {
extern atomic<u_int> defaultMaterialIDIndex;
}

void Scene::ParseVolumes(const Properties &props) {
	vector<string> matKeys = props.GetAllUniqueSubNames("scene.volumes");
	BOOST_FOREACH(const string &key, matKeys) {
		// Extract the volume name
		const string volName = Property::ExtractField(key, 2);
		if (volName == "")
			throw runtime_error("Syntax error in volume definition: " + volName);

		SDL_LOG("Volume definition: " << volName);
		// In order to have harlequin colors with MATERIAL_ID output
		const u_int index = defaultMaterialIDIndex++;
		const u_int volID = ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
		// Volumes are just a special kind of materials so they are stored
		// in matDefs too.
		Material *newMat = CreateVolume(volID, volName, props);

		if (matDefs.IsMaterialDefined(volName)) {
			// A replacement for an existing material
			const Material *oldMat = matDefs.GetMaterial(volName);

			// Check if it is not a volume
			if (!dynamic_cast<const Volume *>(oldMat))
				throw runtime_error("You can not replace a volume with the material: " + volName);

			// Volumes can not be a (directly sampled) light source
			//const bool wasLightSource = oldMat->IsLightSource();

			matDefs.DefineMaterial(newMat);

			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMat, newMat);
			//lightDefs.UpdateMaterialReferences(oldMat, newMat);

			// Check also the camera volume
			if (camera)
				camera->UpdateVolumeReferences(static_cast<const Volume *>(oldMat), static_cast<const Volume *>(newMat));

			// Check also the world default volume
			if (defaultWorldVolume == oldMat)
				defaultWorldVolume = static_cast<const Volume *>(newMat);

			// Check if the old material was or the new material is a light source
			//if (wasLightSource || newMat->IsLightSource())
			//	editActions.AddAction(LIGHTS_EDIT);
		} else {
			// Only a new Material
			matDefs.DefineMaterial(newMat);
		}
	}

	if (props.IsDefined("scene.world.volume.default")) {
		const string volName = props.Get("scene.world.volume.default").Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for default world volume");
		defaultWorldVolume = v;

		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
	}

	if (matKeys.size() > 0)
		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

Volume *Scene::CreateVolume(const u_int defaultVolID, const string &volName, const Properties &props) {
	const string propName = "scene.volumes." + volName;
	const string volType = props.Get(Property(propName + ".type")("homogenous")).Get<string>();

	const Texture *iorTex = GetTexture(props.Get(Property(propName + ".ior")(1.f)));
	const Texture *emissionTex = props.IsDefined(propName + ".emission") ?
		GetTexture(props.Get(Property(propName + ".emission")(0.f, 0.f, 0.f))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	Volume *vol;
	if (volType == "clear") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));

		vol = new ClearVolume(iorTex, emissionTex, absorption);
	} else if (volType == "homogeneous") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		const Texture *scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		const Texture *asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = new HomogeneousVolume(iorTex, emissionTex, absorption, scattering, asymmetry, multiScattering);
	} else if (volType == "heterogeneous") {
		const Texture *absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		const Texture *scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		const Texture *asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const float stepSize =  props.Get(Property(propName + ".steps.size")(1.f)).Get<float>();
		const u_int maxStepsCount =  props.Get(Property(propName + ".steps.maxcount")(32u)).Get<u_int>();
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = new HeterogeneousVolume(iorTex, emissionTex, absorption, scattering, asymmetry, stepSize, maxStepsCount, multiScattering);
	} else
		throw runtime_error("Unknown volume type: " + volType);

	vol->SetName(volName);
	vol->SetID(props.Get(Property(propName + ".id")(defaultVolID)).Get<u_int>());

	vol->SetVolumeLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	vol->SetPriority(props.Get(Property(propName + ".priority")(0)).Get<int>());

	vol->SetPhotonGIEnabled(props.Get(Property(propName + ".photongi.enable")(false)).Get<bool>());

	return vol;
}
