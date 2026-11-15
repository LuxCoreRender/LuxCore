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

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>

#include "luxrays/usings.h"
#include "slg/scene/scene.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"

#include "slg/cameras/camera.h"
#include "slg/usings.h"
#include "slg/volumes/clear.h"
#include "slg/volumes/heterogenous.h"
#include "slg/volumes/homogenous.h"

using namespace std;
using namespace luxrays;
using namespace slg;

namespace slg {
extern atomic<u_int> defaultMaterialIDIndex;
}

void Scene::ParseVolumes(const Properties &props) {
	vector<string> matKeys = props.GetAllUniqueSubNames("scene.volumes");
	// Validate the names serially first, so the reported error is the same
	// as in the serial version
	for(const string &key: matKeys) {
		const string volName = Property::ExtractField(key, 2);
		if (volName == "")
			throw runtime_error("Syntax error in volume definition: " + volName);
	}

	// A redefinition of an already defined volume takes the replacement path
	// (it updates object/light/camera references and the trash); keep the
	// fully serial path in that case to preserve those semantics exactly.
	bool serialPath = false;
	for(const string &key: matKeys) {
		if (matDefs.IsMaterialDefined(Property::ExtractField(key, 2)))
			serialPath = true;
	}

	if (!serialPath) {
		// Create the volumes in parallel; define them serially in the
		// original key order so material insertion order and harlequin IDs
		// are unchanged. CreateVolume only reads props and uses GetTexture
		// (serialized by texDefMtx) at this point.
		const size_t volTotal = matKeys.size();
		std::vector<u_int> volIDs(volTotal);
		for (size_t i = 0; i < volTotal; ++i) {
			// In order to have harlequin colors with MATERIAL_ID output
			const u_int index = defaultMaterialIDIndex++;
			volIDs[i] = ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
					(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
					(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
		}
		std::vector<MaterialUPtr> vols(volTotal);
		tbb::parallel_for(tbb::blocked_range<size_t>(0, volTotal),
			[&](const tbb::blocked_range<size_t> &r) {
				for (size_t i = r.begin(); i < r.end(); ++i)
					vols[i] = CreateVolume(volIDs[i], Property::ExtractField(matKeys[i], 2), props);
			});
		for (size_t i = 0; i < volTotal; ++i) {
			const string volName = Property::ExtractField(matKeys[i], 2);
			SDL_LOG("Volume definition: " << volName);
			// Volumes are just a special kind of materials so they are stored
			// in matDefs too.
			matDefs.DefineMaterial(std::move(vols[i]));
		}
	} else {
	for(const string &key: matKeys) {
		// Extract the volume name
		const string volName = Property::ExtractField(key, 2);

		SDL_LOG("Volume definition: " << volName);
		// In order to have harlequin colors with MATERIAL_ID output
		const u_int index = defaultMaterialIDIndex++;
		const u_int volID = ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
		// Volumes are just a special kind of materials so they are stored
		// in matDefs too.
		auto newMat = CreateVolume(volID, volName, props);

		if (matDefs.IsMaterialDefined(volName)) {
			// A replacement for an existing material
			MaterialConstRef oldMatRef = matDefs.GetMaterial(volName);

			// Check if it is not a volume
			if (!dynamic_cast<const Volume *>(&oldMatRef))
				throw runtime_error("You can not replace a volume with the material: " + volName);

			// Volumes can not be a (directly sampled) light source
			//const bool wasLightSource = oldMat->IsLightSource();

			auto [newMatRef, oldMatPtr] = matDefs.DefineMaterial(std::move(newMat));
			assert(oldMatRef == *oldMatPtr);

			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMatRef, newMatRef);
			//lightDefs.UpdateMaterialReferences(oldMat, newMat);

			// Check also the camera volume
			if (camera)
				camera->UpdateVolumeReferences(
					static_cast<const Volume &>(oldMatRef),
					static_cast<const Volume &>(newMatRef)
				);
			// Check also the light source volumes
			lightDefs.UpdateVolumeReferences(
				static_cast<const Volume &>(oldMatRef),
				static_cast<Volume &>(newMatRef)
			);

			// Check also the world default volume
			if (defaultWorldVolume == &oldMatRef)
				defaultWorldVolume = static_cast<const Volume *>(&newMatRef);

			// Check if the old material was or the new material is a light source
			//if (wasLightSource || newMat->IsLightSource())
			//	editActions.AddAction(LIGHTS_EDIT);

			moveToTrash(std::move(oldMatPtr));
		} else {
			// Only a new Material
			matDefs.DefineMaterial(std::move(newMat));
		}
	}
	}

	if (props.IsDefined("scene.world.volume.default")) {
		const string volName = props.Get("scene.world.volume.default").Get<string>();
		MaterialConstRef m = matDefs.GetMaterial(volName);
		auto* v = dynamic_cast<const Volume *>(&m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for default world volume");
		defaultWorldVolume = VolumeConstPtr(v);

		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
	}

	if (matKeys.size() > 0)
		editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

VolumeUPtr Scene::CreateVolume(const u_int defaultVolID, const string &volName, const Properties &props) {
	const string propName = "scene.volumes." + volName;
	const string volType = props.Get(Property(propName + ".type")("homogenous")).Get<string>();

	auto& iorTex = GetTexture(props.Get(Property(propName + ".ior")(1.f)));
	auto emissionTex = props.IsDefined(propName + ".emission") ?
		TextureConstPtr(
			&GetTexture(props.Get(Property(propName + ".emission")(0.f, 0.f, 0.f)))
		) :
		TextureConstPtr(nullptr);
	// Required to remove light source while editing the scene
	if (
		emissionTex &&
		(
			(
				(emissionTex->GetType() == CONST_FLOAT) &&
				(static_cast<const ConstFloatTexture &>(*emissionTex).GetValue() == 0.f)
			) ||
			(
				(emissionTex->GetType() == CONST_FLOAT3) &&
				(static_cast<const ConstFloat3Texture &>(*emissionTex).GetColor().Black())
			)
		)
	) {
		emissionTex = nullptr;
	}

	VolumeUPtr vol;
	if (volType == "clear") {
		auto& absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));

		vol = std::make_unique<ClearVolume>(iorTex, emissionTex, absorption);
	} else if (volType == "homogeneous") {
		auto& absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		auto& scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		auto& asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = std::make_unique<HomogeneousVolume>(iorTex, emissionTex, absorption, scattering, asymmetry, multiScattering);
	} else if (volType == "heterogeneous") {
		auto& absorption = GetTexture(props.Get(Property(propName + ".absorption")(0.f, 0.f, 0.f)));
		auto& scattering = GetTexture(props.Get(Property(propName + ".scattering")(0.f, 0.f, 0.f)));
		auto& asymmetry = GetTexture(props.Get(Property(propName + ".asymmetry")(0.f, 0.f, 0.f)));
		const float stepSize =  props.Get(Property(propName + ".steps.size")(1.f)).Get<double>();
		const u_int maxStepsCount =  props.Get(Property(propName + ".steps.maxcount")(32u)).Get<u_int>();
		const bool multiScattering =  props.Get(Property(propName + ".multiscattering")(false)).Get<bool>();

		vol = std::make_unique<HeterogeneousVolume>(iorTex, emissionTex, absorption, scattering, asymmetry, stepSize, maxStepsCount, multiScattering);
	} else
		throw runtime_error("Unknown volume type: " + volType);

	vol->SetName(volName);
	vol->SetID(props.Get(Property(propName + ".id")(defaultVolID)).Get<u_int>());

	vol->SetVolumeLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	vol->SetPriority(props.Get(Property(propName + ".priority")(0)).Get<int>());

	vol->SetPhotonGIEnabled(props.Get(Property(propName + ".photongi.enable")(false)).Get<bool>());

	return vol;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
