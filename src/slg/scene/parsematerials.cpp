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

#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <optional>
#include <typeinfo>

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include "luxrays/core/color/color.h"
#include "luxrays/usings.h"
#include "slg/materials/material.h"
#include "slg/scene/scene.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"
#include "slg/textures/fresnel/fresnelpreset.h"
#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/textures/normalmap.h"

#include "slg/materials/archglass.h"
#include "slg/materials/carpaint.h"
#include "slg/materials/cloth.h"
#include "slg/materials/glass.h"
#include "slg/materials/glossy2.h"
#include "slg/materials/glossycoating.h"
#include "slg/materials/glossytranslucent.h"
#include "slg/materials/matte.h"
#include "slg/materials/mattetranslucent.h"
#include "slg/materials/metal2.h"
#include "slg/materials/mirror.h"
#include "slg/materials/mix.h"
#include "slg/materials/null.h"
#include "slg/materials/roughglass.h"
#include "slg/materials/roughmatte.h"
#include "slg/materials/roughmattetranslucent.h"
#include "slg/materials/velvet.h"
#include "slg/materials/disney.h"
#include "slg/materials/twosided.h"

#include "slg/textures/texture.h"
#include "slg/usings.h"
#include "slg/utils/filenameresolver.h"

using namespace std;
using namespace luxrays;
using namespace slg;

namespace slg {
atomic<u_int> defaultMaterialIDIndex(0);
}

void Scene::ParseMaterials(const Properties &props) {
	vector<string> matKeys = props.GetAllUniqueSubNames("scene.materials");
	if (matKeys.size() == 0) {
		// There are not material definitions
		return;
	}

	// Cache isLightSource values before we go deleting materials (required for
	// updating mix material)
	std::unordered_map<const Material * , bool> cachedIsLightSource;

	for(const string &key: matKeys) {
		const string matName = Property::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material definition: " + matName);

		if (matDefs.IsMaterialDefined(matName)) {
			auto& oldMat = matDefs.GetMaterial(matName);
			cachedIsLightSource[&oldMat] = oldMat.IsLightSource();
		}
	}

	// Pre-assign the material IDs serially: the ID feeds the harlequin
	// MATERIAL_ID output, so it must not depend on the scheduling
	const size_t matCount = matKeys.size();
	vector<u_int> matIDs(matCount);
	for (size_t i = 0; i < matCount; ++i)
		matIDs[i] = defaultMaterialIDIndex++;
	auto matIDFromIndex = [](const u_int index) {
		return ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
	};

	// Create the materials in parallel. CreateMaterial only reads props and
	// the already-parsed texDefs/matDefs (volumes come before materials), and
	// matDefs is not modified during this phase, so the concurrent reads are
	// safe. Materials that reference another material by name (mix, twosided,
	// ...) are created serially in the insert loop below, because their
	// reference may be defined later in the list
	vector<MaterialUPtr> newMats(matCount);
	vector<char> parallelOK(matCount, 0);
	for (size_t i = 0; i < matCount; ++i) {
		const string propName = "scene.materials." + Property::ExtractField(matKeys[i], 2);
		parallelOK[i] = !props.IsDefined(propName + ".material1") &&
			!props.IsDefined(propName + ".material2") &&
			!props.IsDefined(propName + ".base") &&
			!props.IsDefined(propName + ".frontmaterial") &&
			!props.IsDefined(propName + ".backmaterial");
	}
	if (matCount > 1) {
		tbb::parallel_for(tbb::blocked_range<size_t>(0, matCount),
			[&](const tbb::blocked_range<size_t> &r) {
				for (size_t i = r.begin(); i < r.end(); ++i) {
					if (!parallelOK[i])
						continue;
					const string matName = Property::ExtractField(matKeys[i], 2);
					newMats[i] = CreateMaterial(matIDFromIndex(matIDs[i]), matName, props);
				}
			});
	}

	// Now I can update the materials
	for (size_t ki = 0; ki < matCount; ++ki) {
		const string &key = matKeys[ki];
		// Extract the material name
		const string matName = Property::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material definition: " + matName);

		if (matDefs.IsMaterialDefined(matName)) {
			SDL_LOG("Material re-definition: " << matName);
		} else {
			SDL_LOG("Material definition: " << matName);
		}

		// In order to have harlequin colors with MATERIAL_ID output
		auto newMat = std::move(newMats[ki]);
		if (!newMat)
			newMat = CreateMaterial(matIDFromIndex(matIDs[ki]), matName, props);

		if (matDefs.IsMaterialDefined(matName)) {
			//// A replacement for an existing material
			//auto& oldMat = matDefs.GetMaterial(matName);

			// Add to material list
			auto [newMatRef, oldMatPtr] = matDefs.DefineMaterial(std::move(newMat));
			auto& oldMatRef = *oldMatPtr;

			// The mat should not be a volume: let's check it
			try {
				dynamic_cast<const Volume&>(oldMatRef);
				throw runtime_error(
					"You can not replace a material with the volume: " + matName
				);
			} catch(std::bad_cast&) {
			}

			// If old material was emitting light, delete all TriangleLight
			if (cachedIsLightSource[&oldMatRef])
				lightDefs.DeleteLightSourceByMaterial(oldMatRef);

			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMatRef, newMatRef);

			// If new material is emitting light, create all TriangleLight
			if (newMatRef.IsLightSource())
				objDefs.DefineIntersectableLights(lightDefs, newMatRef);

			// Check if the old material was or the new material is a light source
			if (cachedIsLightSource[&oldMatRef] || newMatRef.IsLightSource())
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

			moveToTrash(std::move(oldMatPtr));
		} else {

			// Only a new Material
			auto [newMatRef, oldMatPtr] = matDefs.DefineMaterial(std::move(newMat));
			assert(!oldMatPtr);
			// Check if the new material is a light source
			if (newMatRef.IsLightSource())
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

MaterialUPtr Scene::CreateMaterial(
	const u_int defaultMatID, const string &matName, const Properties &props
) {
	// A few helpful constants
	const string propName = "scene.materials." + matName;
	static constexpr auto nullTex = TextureConstPtr(nullptr);
	const auto zeroSpectrum = Spectrum(0.f);
	const auto oneSpectrum = Spectrum(1.f);

	// A few helpers to facilitate texture property parsing:

	// Parse required texture
	auto parseTex = [&](
		const std::string_view suffix,
		Spectrum defaultVal
	) -> TexturePtr {
		assert(suffix[0] != '.');
		auto& tex = GetTexture(
			props.Get(Property(propName + "." + std::string(suffix))(defaultVal))
		);

		return TexturePtr(std::addressof(tex));
	};

	// Parse optional texture
	auto parseTextureConstPtr = [&](  // Parse optional texture
		const std::string suffix,
		Spectrum defaultVal,
		TextureConstPtr fallbackTex=TextureConstPtr(nullptr)
	) -> TextureConstPtr {
		assert(suffix[0] != '.');
		return props.IsDefined(propName + "." + suffix) ?
		TexturePtr(
			&GetTexture(props.Get(Property(propName + "." + suffix)(defaultVal)))
		) :
		fallbackTex;
	};

	// Parse float
	auto parseFloat = [&](
		const std::string_view suffix,
		float defaultVal
	) -> float {
		assert(suffix[0] != '.');
		return props.Get(Property(propName + "." + std::string(suffix))(defaultVal))
			.Get<double>();
	};

	// Parse boolean
	auto parseBool = [&](
		const std::string_view suffix,
		bool defaultVal
	) -> bool {
		assert(suffix[0] != '.');
		return props.Get(Property(propName + "." + std::string(suffix))(defaultVal))
			.Get<bool>();
	};

	// Parse string
	auto parseString = [&](
		const std::string_view suffix,
		std::string defaultVal
	) -> std::string {
		assert(suffix[0] != '.');
		return props.Get(Property(propName + "." + std::string(suffix))(defaultVal))
			.Get<std::string>();
	};

	// Check whether texture is defined
	auto isDefined = [&](const std::string_view suffix) -> bool {
		assert(suffix[0] != '.');
		return props.IsDefined(propName + "." + std::string(suffix));
	};

	// Warn for deprecated property
	auto warnDeprecated = [&](std::string_view suffix) {
		assert(suffix[0] != '.');
		SLG_LOG("WARNING: deprecated property " + propName + "." + std::string(suffix));
	};

	// PARSING STARTS HERE
	const string matType = parseString("type", "matte");
	// For compatibility with the past
	auto transparencyTex = parseTextureConstPtr("transparency", Spectrum(0.f));

	auto frontTransparencyTex = parseTextureConstPtr(
		"transparency.front", Spectrum(0.f), transparencyTex
	);

	auto backTransparencyTex = parseTextureConstPtr(
		"transparency.back", {Spectrum(0.f)}, transparencyTex
	);

	// Start non-legacy parsing
	auto emissionTex = parseTextureConstPtr("emission", {Spectrum(0.f)});

	// Required to remove light source while editing the scene
	if (emissionTex && (
		((emissionTex->GetType() == CONST_FLOAT) && ((static_cast<const ConstFloatTexture*>(emissionTex.get()))->GetValue() == 0.f)) ||
		((emissionTex->GetType() == CONST_FLOAT3) && ((static_cast<const ConstFloat3Texture*>(emissionTex.get()))->GetColor().Black()))))
	{
		emissionTex = nullTex;
	}

	auto bumpTex = parseTextureConstPtr("bumptex", {1.f});
    if (!bumpTex) {
		auto normalTex = parseTextureConstPtr("normaltex", {1.f});

        if (normalTex) {
			const float scale = std::max(0.f, parseFloat("normaltex.scale", {1.0}));

            auto implBumpTex = std::make_unique<NormalMapTexture>(*normalTex, scale);
			implBumpTex->SetName(NamedObject::GetUniqueName("Implicit-NormalMapTexture"));

			auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(implBumpTex));
            bumpTex = TextureConstPtr(&newTexRef);
			moveToTrash(std::move(oldTexPtr));
        }
    }

    const float bumpSampleDistance = parseFloat("bumpsamplingdistance", {.001f});

	MaterialUPtr mat;
	if (matType == "matte") {
		auto kd = parseTex("kd", {.75f, .75f, .75f});

		mat = std::make_unique<MatteMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd
		);
	} else if (matType == "roughmatte") {
		auto kd = parseTex("kd", {.75f, .75f, .75f});
		auto sigma = parseTex("sigma", {.75f, .75f, .75f});

		mat = std::make_unique<RoughMatteMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, sigma
		);
	} else if (matType == "mirror") {
		auto kr = parseTex("kr", {1.f, 1.f, 1.f});

		mat = std::make_unique<MirrorMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr
		);
	} else if (matType == "glass") {
		auto kr = parseTex("kr", {1.f, 1.f, 1.f});
		auto kt = parseTex("kt", {1.f, 1.f, 1.f});

		TextureConstPtr exteriorIor = nullptr;
		TextureConstPtr interiorIor = nullptr;
		// For compatibility with the past
		if (isDefined("ioroutside")) {
			warnDeprecated("ioroutside");
			exteriorIor = parseTex("ioroutside", 1.f);
		} else if (isDefined("exteriorior"))
			exteriorIor = parseTex("exteriorior", 1.f);
		// For compatibility with the past
		if (isDefined("iorinside")) {
			warnDeprecated("iorinside");
			interiorIor = parseTex("iorinside", 1.5f);
		} else if (isDefined("interiorior"))
			interiorIor = parseTex("interiorior", 1.5f);

		TextureConstPtr cauchyB = nullptr;
		if (isDefined("cauchyb"))
			cauchyB = parseTex("cauchyb", {0.f, 0.f, 0.f});
		// For compatibility with the past
		else if (isDefined("cauchyc")) {
			warnDeprecated("cauchyc");
			cauchyB = parseTex("cauchyc", {0.f, 0.f, 0.f});
		}

		TextureConstPtr filmThickness = nullptr;
		if (isDefined("filmthickness"))
			filmThickness = parseTex("filmthickness", {0.f});

		TextureConstPtr filmIor = nullptr;
		if (isDefined("filmior"))
			filmIor = parseTex("filmior", {1.5f});

		mat = std::make_unique<GlassMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt,
			exteriorIor, interiorIor, cauchyB, filmThickness, filmIor
		);
	} else if (matType == "archglass") {
		auto kr = parseTex("kr", {1.f, 1.f, 1.f});
		auto kt = parseTex("kt", {1.f, 1.f, 1.f});

		TextureConstPtr exteriorIor = nullptr;
		TextureConstPtr interiorIor = nullptr;
		// For compatibility with the past
		if (isDefined("ioroutside")) {
			warnDeprecated("ioroutside");
			exteriorIor = parseTex("ioroutside", {1.f});
		} else if (isDefined("exteriorior"))
			exteriorIor = parseTex("exteriorior", {1.f});
		// For compatibility with the past
		if (isDefined("iorinside")) {
			warnDeprecated("iorinside");
			interiorIor = parseTex("iorinside", {1.f});
		} else if (isDefined("interiorior"))
			interiorIor = parseTex("interiorior", {1.f});

		TextureConstPtr filmThickness = nullptr;
		if (isDefined("filmthickness"))
			filmThickness = parseTex("filmthickness", {0.f});

		TextureConstPtr filmIor = nullptr;
		if (isDefined("filmior"))
			filmIor = parseTex("filmior", {1.5f});

		mat = std::make_unique<ArchGlassMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kr, kt, exteriorIor, interiorIor, filmThickness, filmIor
		);
	} else if (matType == "mix") {
		auto& matA = matDefs.GetMaterial(parseString("material1", "mat1"));
		auto& matB = matDefs.GetMaterial(parseString("material2", "mat2"));
		auto mix = parseTex("amount", {.5f});

		auto mixMat = std::make_unique<MixMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			matA, matB, mix
		);

		// Check if there is a loop in Mix material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (mixMat->IsReferencing(*mixMat))
			throw runtime_error("There is a loop in Mix material definition: " + matName);

		mat = std::move(mixMat);
	} else if (matType == "null") {
		mat = std::make_unique<NullMaterial>(frontTransparencyTex, backTransparencyTex);
	} else if (matType == "mattetranslucent") {
		auto kr = parseTex("kr", {.5f, .5f, .5f});
		auto kt = parseTex("kt", {.5f, .5f, .5f});

		mat = std::make_unique<MatteTranslucentMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kr, kt
		);
	} else if (matType == "roughmattetranslucent") {
		auto kr = parseTex("kr", {.5f, .5f, .5f});
		auto kt = parseTex("kt", {.5f, .5f, .5f});
		auto sigma = parseTex("sigma", {0.f});

		mat = std::make_unique<RoughMatteTranslucentMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kr, kt, sigma
		);
	} else if (matType == "glossy2") {
		auto kd = parseTex("kd", {.5f, .5f, .5f});
		auto ks = parseTex("ks", {.5f, .5f, .5f});
		auto nu = parseTex("uroughness", {.1f});
		auto nv = parseTex("vroughness", {.1f});
		auto ka = parseTex("ka", {0.f, 0.f, 0.f});
		auto d = parseTex("d", {0.f});
		auto index = parseTex("index", {0.f, 0.f, 0.f});
		const auto multibounce = parseBool("multibounce", false);
		const auto doublesided = parseBool("doublesided", false);

		mat = std::make_unique<Glossy2Material>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kd, ks, nu, nv, ka, d, index, multibounce, doublesided
		);
	} else if (matType == "metal2") {
		auto nu = parseTex("uroughness", {.1f});
		auto nv = parseTex("vroughness", {.1f});

		TextureConstPtr n, k;
		if (isDefined("preset") || isDefined("name")) {
			FresnelTextureUPtr presetTex = AllocFresnelPresetTex(props, propName);
			const auto texname = NamedObject::GetUniqueName(matName + "-Implicit-FresnelPreset");
			presetTex->SetName(texname);
			auto [newTexRef, oldTexPtr] = texDefs.DefineTexture(std::move(presetTex));
			auto refpreset = FresnelTextureConstPtr(
				dynamic_cast<const FresnelTexture *>(std::addressof(newTexRef))
			);

			mat = std::make_unique<Metal2Material>(
				frontTransparencyTex,
				backTransparencyTex,
				emissionTex,
				bumpTex,
				refpreset,
				nu,
				nv
			);
			moveToTrash(std::move(oldTexPtr));
		} else if (isDefined("fresnel")) {
			auto tex = parseTex("fresnel", {5.f});
			if (!dynamic_cast<const FresnelTexture *>(tex.get()))
				throw runtime_error(
					"Metal2 fresnel property requires a fresnel texture: " + matName
				);

			auto fresnelTex = static_cast<const FresnelTexture *>(tex.get());
			mat = std::make_unique<Metal2Material>(
				frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
				FresnelTextureConstPtr(fresnelTex), nu, nv
			);
		} else {
			n = parseTex("n", {.5f, .5f, .5f});
			k = parseTex("k", {.5f, .5f, .5f});
			mat = std::make_unique<Metal2Material>(
				frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
				n, k, nu, nv
			);
		}
	} else if (matType == "roughglass") {
		auto kr = parseTex("kr", {1.f, 1.f, 1.f});
		auto kt = parseTex("kt", {1.f, 1.f, 1.f});

		TextureConstPtr exteriorIor = nullptr;
		TextureConstPtr interiorIor = nullptr;
		// For compatibility with the past
		if (isDefined("ioroutside")) {
			warnDeprecated("ioroutside");
			exteriorIor = parseTex("ioroutside", {1.f});
		} else if (isDefined("exteriorior"))
			exteriorIor = parseTex("exteriorior", {1.f});
		// For compatibility with the past
		if (isDefined("iorinside")) {
			warnDeprecated("iorinside");
			interiorIor = parseTex("iorinside", {1.5f});
		} else if (isDefined("interiorior"))
			interiorIor = parseTex("interiorior", {1.5f});

		auto nu = parseTex("uroughness", {.1f});
		auto nv = parseTex("vroughness", {.1f});

		TextureConstPtr filmThickness = nullptr;
		if (isDefined("filmthickness"))
			filmThickness = parseTex("filmthickness", {0.f});

		TextureConstPtr filmIor = nullptr;
		if (isDefined("filmior"))
			filmIor = parseTex("filmior", {1.5f});

		mat = std::make_unique<RoughGlassMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kr, kt, exteriorIor, interiorIor, nu, nv, filmThickness, filmIor
		);
	} else if (matType == "velvet") {
		auto kd = parseTex("kd", {.5f, .5f, .5f});
		auto p1 = parseTex("p1", {-2.0f});
		auto p2 = parseTex("p2", {20.0f});
		auto p3 = parseTex("p3", {2.0f});
		auto thickness = parseTex("thickness", {0.1f});

		mat = std::make_unique<VelvetMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kd, p1, p2, p3, thickness
		);
	} else if (matType == "cloth") {
		slg::ocl::ClothPreset preset = slg::ocl::DENIM;

		if (isDefined("preset")) {
			const string type = parseString("preset", "denim");

			if (type == "denim")
				preset = slg::ocl::DENIM;
			else if (type == "silk_charmeuse")
				preset = slg::ocl::SILKCHARMEUSE;
			else if (type == "silk_shantung")
				preset = slg::ocl::SILKSHANTUNG;
			else if (type == "cotton_twill")
				preset = slg::ocl::COTTONTWILL;
			// "Gabardine" was misspelled in the past, ensure backwards-compatibility (fixed in v2.2)
			else if (type == "wool_gabardine" || type == "wool_garbardine")
				preset = slg::ocl::WOOLGABARDINE;
			else if (type == "polyester_lining_cloth")
				preset = slg::ocl::POLYESTER;
		}
		auto weft_kd = parseTex("weft_kd", {.5f, .5f, .5f});
		auto weft_ks = parseTex("weft_ks", {.5f, .5f, .5f});
		auto warp_kd = parseTex("warp_kd", {.5f, .5f, .5f});
		auto warp_ks = parseTex("warp_ks", {.5f, .5f, .5f});
		const float repeat_u = parseFloat("repeat_u", 100.0f);
		const float repeat_v = parseFloat("repeat_v", 100.0f);

		mat = std::make_unique<ClothMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			preset, weft_kd, weft_ks, warp_kd, warp_ks, repeat_u, repeat_v
		);
	} else if (matType == "carpaint") {
		auto ka = parseTex("ka", {0.f, 0.f, 0.f});
		auto d = parseTex("d", {0.f});

		string preset = parseString("preset", "");
		if (preset != "") {
			const int numPaints = CarPaintMaterial::NbPresets();
			int i;
			for (i = 0; i < numPaints; ++i) {
				if (preset == CarPaintMaterial::data[i].name)
					break;
			}

			if (i == numPaints)
				preset = "";
			else {
				auto& cpData = CarPaintMaterial::data;
				auto& kd = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-kd"))
					(cpData[i].kd[0], cpData[i].kd[1], cpData[i].kd[2]));
				auto& ks1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks1"))
					(cpData[i].ks1[0], cpData[i].ks1[1], cpData[i].ks1[2]));
				auto& ks2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks2"))
					(cpData[i].ks2[0], cpData[i].ks2[1], cpData[i].ks2[2]));
				auto& ks3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks3"))
					(cpData[i].ks3[0], cpData[i].ks3[1], cpData[i].ks3[2]));
				auto& r1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r1"))
					(cpData[i].r1));
				auto& r2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r2"))
					(cpData[i].r2));
				auto& r3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r3"))
					(cpData[i].r3));
				auto& m1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m1"))
					(cpData[i].m1));
				auto& m2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m2"))
					(cpData[i].m2));
				auto& m3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m3"))
					(cpData[i].m3));
				mat = std::make_unique<CarPaintMaterial>(
					frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
					TextureConstPtr(&kd),
					TextureConstPtr(&ks1),
					TextureConstPtr(&ks2),
					TextureConstPtr(&ks3),
					TextureConstPtr(&m1),
					TextureConstPtr(&m2),
					TextureConstPtr(&m3),
					TextureConstPtr(&r1),
					TextureConstPtr(&r2),
					TextureConstPtr(&r3),
					ka,
					d
				);
			}
		}

		// preset can be reset above if the name is not found
		if (preset == "") {
			auto& cpData = CarPaintMaterial::data[0];
			auto kd = parseTex( "kd", {cpData.kd[0], cpData.kd[1], cpData.kd[2]});
			auto ks1 = parseTex("ks1", {cpData.ks1[0], cpData.ks1[1], cpData.ks1[2]});
			auto ks2 = parseTex("ks2", {cpData.ks2[0], cpData.ks2[1], cpData.ks2[2]});
			auto ks3 = parseTex("ks3", {cpData.ks3[0], cpData.ks3[1], cpData.ks3[2]});
			auto r1 = parseTex("r1", {cpData.r1});
			auto r2 = parseTex("r2", {cpData.r2});
			auto r3 = parseTex("r3", {cpData.r3});
			auto m1 = parseTex("m1", {cpData.m1});
			auto m2 = parseTex("m2", {cpData.m2});
			auto m3 = parseTex("m3", {cpData.m3});
			mat = std::make_unique<CarPaintMaterial>(
				frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
				kd, ks1, ks2, ks3, m1, m2, m3, r1, r2, r3, ka, d
			);
		}
	} else if (matType == "glossytranslucent") {
		auto kd = parseTex("kd", {.5f, .5f, .5f});
		auto kt = parseTex("kt", {.5f, .5f, .5f});
		auto ks = parseTex("ks", {.5f, .5f, .5f});
		auto ks_bf = parseTex("ks_bf", {.5f, .5f, .5f});
		auto nu = parseTex("uroughness", {.1f});
		auto nu_bf = parseTex("uroughness_bf", {.1f});
		auto nv = parseTex("vroughness", {.1f});
		auto nv_bf = parseTex("vroughness_bf", {.1f});
		auto ka = parseTex("ka", {0.f, 0.f, 0.f});
		auto ka_bf = parseTex("ka_bf", {0.f, 0.f, 0.f});
		auto d = parseTex("d", {0.f});
		auto d_bf = parseTex("d_bf", {0.f});
		auto index = parseTex("index", {0.f, 0.f, 0.f});
		auto index_bf = parseTex("index_bf", {0.f, 0.f, 0.f});
		const bool multibounce = parseBool("multibounce", false);
		const bool multibounce_bf = parseBool("multibounce_bf", false);

		mat = std::make_unique<GlossyTranslucentMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			kd, kt, ks, ks_bf, nu, nu_bf, nv, nv_bf,
			ka, ka_bf, d, d_bf, index, index_bf, multibounce, multibounce_bf
		);
	} else if (matType == "glossycoating") {
		MaterialConstRef matBase = matDefs.GetMaterial(parseString("base", ""));
		auto ks = parseTex("ks", {.5f, .5f, .5f});
		auto nu = parseTex("uroughness", {.1f});
		auto nv = parseTex("vroughness", {.1f});
		auto ka = parseTex("ka", {0.f, 0.f, 0.f});
		auto d = parseTex("d", {0.f});
		auto index = parseTex("index", {0.f, 0.f, 0.f});
		const bool multibounce = parseBool("multibounce", false);

		mat = std::make_unique<GlossyCoatingMaterial>(
			frontTransparencyTex,
			backTransparencyTex,
			emissionTex,
			bumpTex,
			MaterialConstPtr(&matBase),
			ks,
			nu,
			nv,
			ka,
			d,
			index,
			multibounce
		);
	} else if (matType == "disney") {
		auto baseColor = parseTex("basecolor", {.5f, .5f, .5f});
		auto subsurface = parseTex("subsurface", {0.f});
		auto roughness = parseTex("roughness", {0.f});
		auto metallic = parseTex("metallic", {0.f});
		auto specular = parseTex("specular", {0.f});
		auto specularTint = parseTex("speculartint", {0.f});
		auto clearcoat = parseTex("clearcoat", {0.f});
		auto clearcoatGloss = parseTex("clearcoatgloss", {0.f});
		auto anisotropic = parseTex("anisotropic", {0.f});
		auto sheen = parseTex("sheen", {0.f});
		auto sheenTint = parseTex("sheentint", {0.f});

		TextureConstPtr filmAmount = nullptr;
		if (isDefined("filmamount"))
			filmAmount = parseTex("filmamount", {1.f});

		TextureConstPtr filmThickness = nullptr;
		if (isDefined("filmthickness"))
			filmThickness = parseTex("filmthickness", {0.f});

		TextureConstPtr filmIor = nullptr;
		if (isDefined("filmior"))
			filmIor = parseTex("filmior", {1.5f});

		mat = std::make_unique<DisneyMaterial>(
			frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex,
			baseColor, subsurface, roughness, metallic,
			specular, specularTint, clearcoat, clearcoatGloss, anisotropic,
			sheen, sheenTint, filmAmount, filmThickness, filmIor
		);
	} else if (matType == "twosided") {
		MaterialConstRef frontMat = matDefs.GetMaterial(parseString("frontmaterial", "front"));
		MaterialConstRef backMat = matDefs.GetMaterial(parseString("backmaterial", "back"));

		auto twoSided = std::make_unique<TwoSidedMaterial>(
			frontTransparencyTex,
			backTransparencyTex,
			emissionTex,
			bumpTex,
			frontMat,
			backMat
		);

		// Check if there is a loop in Two-sided material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (twoSided->IsReferencing(*twoSided))
			throw runtime_error("There is a loop in Two-sided material definition: " + matName);

		mat = std::move(twoSided);
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetName(matName);

	mat->SetID(props.Get(Property(propName + ".id")(defaultMatID)).Get<u_int>());
	mat->SetBumpSampleDistance(bumpSampleDistance);

	// Gain is not really a color so I avoid to use GetColor()
	mat->SetEmittedGain(props.Get(Property(propName + ".emission.gain")(Spectrum(1.f))).Get<Spectrum>());
	mat->SetEmittedPower(std::max(0.f, parseFloat("emission.power", 0.0)));
	mat->SetEmittedPowerNormalize(parseBool("emission.normalizebycolor", true));
	mat->SetEmittedGainNormalize(parseBool("emission.gain.normalizebycolor", false));
	mat->SetEmittedEfficency(
		std::max(
			0.0,
			props.Get(
				std::move(Property(propName + ".emission.efficiency")(0.0)),
				propName + ".emission.efficency"
			)->Get<double>()
		)
	);
	mat->SetEmittedTheta(std::clamp(parseFloat("emission.theta", 90.0), 0.f, 90.f));
	mat->SetLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	mat->SetEmittedImportance(parseFloat("emission.importance", 1.0));
	mat->SetEmittedTemperature(parseFloat("emission.temperature", -1.f));
	mat->SetEmittedTemperatureNormalize(parseBool("emission.temperature.normalize", false));

	mat->SetPassThroughShadowTransparency(GetColor(props.Get(Property(propName + ".transparency.shadow")(Spectrum(0.f)))));
	if (props.IsDefined(propName + ".transparency.shadowoverride")){
		mat->SetPassThroughShadowTransparencyOverride(
			parseBool("transparency.shadowoverride", false)
		);
	} else {
		mat->SetPassThroughShadowTransparencyOverride(false);
	}

	const auto dlsType = parseString("emission.directlightsampling.type", "AUTO");
	if (dlsType == "ENABLED")
		mat->SetDirectLightSamplingType(DLS_ENABLED);
	else if (dlsType == "DISABLED")
		mat->SetDirectLightSamplingType(DLS_DISABLED);
	else if (dlsType == "AUTO")
		mat->SetDirectLightSamplingType(DLS_AUTO);
	else
		throw runtime_error("Unknown material emission direct sampling type: " + dlsType);

	mat->SetIndirectDiffuseVisibility(parseBool("visibility.indirect.diffuse.enable", true));
	mat->SetIndirectGlossyVisibility(parseBool("visibility.indirect.glossy.enable", true));
	mat->SetIndirectSpecularVisibility(parseBool("visibility.indirect.specular.enable", true));

	mat->SetShadowCatcher(parseBool("shadowcatcher.enable", false));
	mat->SetShadowCatcherOnlyInfiniteLights(parseBool("shadowcatcher.onlyinfinitelights", false));

	mat->SetPhotonGIEnabled(parseBool("photongi.enable", true));
	mat->SetHoldout(parseBool("holdout.enable", false));

	// Check if there is a image or IES map
	auto emissionMap = CreateEmissionMap(propName + ".emission", props);
	if (emissionMap) {
		// There is one
		mat->SetEmissionMap(*emissionMap);
	}

	// Interior volumes
	if (props.IsDefined(propName + ".volume.interior")) {
		const string volName = parseString("volume.interior", "vol1");
		MaterialConstRef m = matDefs.GetMaterial(volName);
		try {
			auto& v = dynamic_cast<const Volume&>(m);
			mat->SetInteriorVolume(v);
		} catch(std::bad_cast&) {
			throw runtime_error(
				volName
				+ " is not a volume and can not be used for material interior volume: "
				+ matName
			);
		}
	}

	// Exterior volumes
	if (props.IsDefined(propName + ".volume.exterior")) {
		const string volName = parseString("volume.exterior", "vol2");
		auto& m = matDefs.GetMaterial(volName);
		try {
			auto& v = dynamic_cast<const Volume&>(m);
			mat->SetExteriorVolume(v);
		} catch(std::bad_cast&) {
			throw runtime_error(volName + " is not a volume and can not be used for material exterior volume: " + matName);
		}
	}

	return mat;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
