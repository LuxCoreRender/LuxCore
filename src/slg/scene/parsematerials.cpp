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
#include "slg/textures/fresnel/fresnelpreset.h"
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
	boost::unordered_map<const Material *, bool> cachedIsLightSource;

	BOOST_FOREACH(const string &key, matKeys) {
		const string matName = Property::ExtractField(key, 2);
		if (matName == "")
			throw runtime_error("Syntax error in material definition: " + matName);

		if (matDefs.IsMaterialDefined(matName)) {
			const Material *oldMat = matDefs.GetMaterial(matName);
			cachedIsLightSource[oldMat] = oldMat->IsLightSource();
		}
	}

	// Now I can update the materials
	BOOST_FOREACH(const string &key, matKeys) {
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
		const u_int index = defaultMaterialIDIndex++;
		const u_int matID = ((u_int)(RadicalInverse(index + 1, 2) * 255.f + .5f)) |
				(((u_int)(RadicalInverse(index + 1, 3) * 255.f + .5f)) << 8) |
				(((u_int)(RadicalInverse(index + 1, 5) * 255.f + .5f)) << 16);
		Material *newMat = CreateMaterial(matID, matName, props);

		if (matDefs.IsMaterialDefined(matName)) {
			// A replacement for an existing material
			const Material *oldMat = matDefs.GetMaterial(matName);

			// Check if it is a volume
			if (dynamic_cast<const Volume *>(oldMat))
				throw runtime_error("You can not replace a material with the volume: " + matName);

			matDefs.DefineMaterial(newMat);

			// If old material was emitting light, delete all TriangleLight
			if (cachedIsLightSource[oldMat])
				lightDefs.DeleteLightSourceByMaterial(oldMat);
				
			// Replace old material direct references with new one
			objDefs.UpdateMaterialReferences(oldMat, newMat);

			// If new material is emitting light, create all TriangleLight
			if (newMat->IsLightSource())
				objDefs.DefineIntersectableLights(lightDefs, newMat);

			// Check if the old material was or the new material is a light source
			if (cachedIsLightSource[oldMat] || newMat->IsLightSource())
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
		} else {
			// Only a new Material
			matDefs.DefineMaterial(newMat);

			// Check if the new material is a light source
			if (newMat->IsLightSource())
				editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
		}
	}

	editActions.AddActions(MATERIALS_EDIT | MATERIAL_TYPES_EDIT);
}

Material *Scene::CreateMaterial(const u_int defaultMatID, const string &matName, const Properties &props) {
	const string propName = "scene.materials." + matName;
	const string matType = props.Get(Property(propName + ".type")("matte")).Get<string>();

	// For compatibility with the past
	const Texture *transparencyTex = props.IsDefined(propName + ".transparency") ?
		GetTexture(props.Get(Property(propName + ".transparency")(Spectrum(0.f)))) : NULL;

	const Texture *frontTransparencyTex = props.IsDefined(propName + ".transparency.front") ?
		GetTexture(props.Get(Property(propName + ".transparency.front")(Spectrum(0.f)))) : transparencyTex;
	const Texture *backTransparencyTex = props.IsDefined(propName + ".transparency.back") ?
		GetTexture(props.Get(Property(propName + ".transparency.back")(Spectrum(0.f)))) : transparencyTex;

	const Texture *emissionTex = props.IsDefined(propName + ".emission") ?
		GetTexture(props.Get(Property(propName + ".emission")(Spectrum(0.f)))) : NULL;
	// Required to remove light source while editing the scene
	if (emissionTex && (
			((emissionTex->GetType() == CONST_FLOAT) && (((ConstFloatTexture *)emissionTex)->GetValue() == 0.f)) ||
			((emissionTex->GetType() == CONST_FLOAT3) && (((ConstFloat3Texture *)emissionTex)->GetColor().Black()))))
		emissionTex = NULL;

	const Texture *bumpTex = props.IsDefined(propName + ".bumptex") ?
		GetTexture(props.Get(Property(propName + ".bumptex")(1.f))) : NULL;
    if (!bumpTex) {
        const Texture *normalTex = props.IsDefined(propName + ".normaltex") ?
            GetTexture(props.Get(Property(propName + ".normaltex")(1.f))) : NULL;

        if (normalTex) {
			const float scale = Max(0.f, props.Get(Property(propName + ".normaltex.scale")(1.f)).Get<float>());

            Texture *implBumpTex = new NormalMapTexture(normalTex, scale);
			implBumpTex->SetName(NamedObject::GetUniqueName("Implicit-NormalMapTexture"));
            texDefs.DefineTexture(implBumpTex);
			
			bumpTex = implBumpTex;
        }
    }

    const float bumpSampleDistance = props.Get(Property(propName + ".bumpsamplingdistance")(.001f)).Get<float>();

	Material *mat;
	if (matType == "matte") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.75f, .75f, .75f)));

		mat = new MatteMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd);
	} else if (matType == "roughmatte") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.75f, .75f, .75f)));
		const Texture *sigma = GetTexture(props.Get(Property(propName + ".sigma")(0.f)));

		mat = new RoughMatteMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, sigma);
	} else if (matType == "mirror") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));

		mat = new MirrorMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr);
	} else if (matType == "glass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		const Texture *exteriorIor = NULL;
		const Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.5f)));

		const Texture *cauchyB = NULL;
		if (props.IsDefined(propName + ".cauchyb"))
			cauchyB = GetTexture(props.Get(Property(propName + ".cauchyb")(0.f, 0.f, 0.f)));
		// For compatibility with the past
		else if (props.IsDefined(propName + ".cauchyc")){
			SLG_LOG("WARNING: deprecated property " + propName + ".cauchyc");
			cauchyB = GetTexture(props.Get(Property(propName + ".cauchyc")(0.f, 0.f, 0.f)));
		}
		
		const Texture *filmThickness = NULL;
		if (props.IsDefined(propName + ".filmthickness"))
			filmThickness = GetTexture(props.Get(Property(propName + ".filmthickness")(0.f)));
		
		const Texture *filmIor = NULL;
		if (props.IsDefined(propName + ".filmior"))
			filmIor = GetTexture(props.Get(Property(propName + ".filmior")(1.5f)));

		mat = new GlassMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor, cauchyB, filmThickness, filmIor);
	} else if (matType == "archglass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		const Texture *exteriorIor = NULL;
		const Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.f)));
			
		const Texture *filmThickness = NULL;
		if (props.IsDefined(propName + ".filmthickness"))
			filmThickness = GetTexture(props.Get(Property(propName + ".filmthickness")(0.f)));
		
		const Texture *filmIor = NULL;
		if (props.IsDefined(propName + ".filmior"))
			filmIor = GetTexture(props.Get(Property(propName + ".filmior")(1.5f)));

		mat = new ArchGlassMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor, filmThickness, filmIor);
	} else if (matType == "mix") {
		const Material *matA = matDefs.GetMaterial(props.Get(Property(propName + ".material1")("mat1")).Get<string>());
		const Material *matB = matDefs.GetMaterial(props.Get(Property(propName + ".material2")("mat2")).Get<string>());
		const Texture *mix = GetTexture(props.Get(Property(propName + ".amount")(.5f)));

		MixMaterial *mixMat = new MixMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, matA, matB, mix);

		// Check if there is a loop in Mix material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (mixMat->IsReferencing(mixMat))
			throw runtime_error("There is a loop in Mix material definition: " + matName);

		mat = mixMat;
	} else if (matType == "null") {
		mat = new NullMaterial(frontTransparencyTex, backTransparencyTex);
	} else if (matType == "mattetranslucent") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(.5f, .5f, .5f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));

		mat = new MatteTranslucentMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt);
	} else if (matType == "roughmattetranslucent") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(.5f, .5f, .5f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));
		const Texture *sigma = GetTexture(props.Get(Property(propName + ".sigma")(0.f)));

		mat = new RoughMatteTranslucentMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt, sigma);
	} else if (matType == "glossy2") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		const Texture *ks = GetTexture(props.Get(Property(propName + ".ks")(.5f, .5f, .5f)));
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));
		const Texture *index = GetTexture(props.Get(Property(propName + ".index")(0.f, 0.f, 0.f)));
		const bool multibounce = props.Get(Property(propName + ".multibounce")(false)).Get<bool>();

		mat = new Glossy2Material(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "metal2") {
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));

		const Texture *n, *k;
		if (props.IsDefined(propName + ".preset") || props.IsDefined(propName + ".name")) {
			FresnelTexture *presetTex = AllocFresnelPresetTex(props, propName);
			presetTex->SetName(NamedObject::GetUniqueName(matName + "-Implicit-FresnelPreset"));
			texDefs.DefineTexture(presetTex);
			
			mat = new Metal2Material(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, presetTex, nu, nv);
		} else if (props.IsDefined(propName + ".fresnel")) {
			const Texture *tex = GetTexture(props.Get(Property(propName + ".fresnel")(5.f)));
			if (!dynamic_cast<const FresnelTexture *>(tex))
				throw runtime_error("Metal2 fresnel property requires a fresnel texture: " + matName);

			const FresnelTexture *fresnelTex = (const FresnelTexture *)tex;
			mat = new Metal2Material(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, fresnelTex, nu, nv);
		} else {
			n = GetTexture(props.Get(Property(propName + ".n")(.5f, .5f, .5f)));
			k = GetTexture(props.Get(Property(propName + ".k")(.5f, .5f, .5f)));
			mat = new Metal2Material(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, n, k, nu, nv);
		}
	} else if (matType == "roughglass") {
		const Texture *kr = GetTexture(props.Get(Property(propName + ".kr")(1.f, 1.f, 1.f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(1.f, 1.f, 1.f)));

		const Texture *exteriorIor = NULL;
		const Texture *interiorIor = NULL;
		// For compatibility with the past
		if (props.IsDefined(propName + ".ioroutside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".ioroutside");
			exteriorIor = GetTexture(props.Get(Property(propName + ".ioroutside")(1.f)));
		} else if (props.IsDefined(propName + ".exteriorior"))
			exteriorIor = GetTexture(props.Get(Property(propName + ".exteriorior")(1.f)));
		// For compatibility with the past
		if (props.IsDefined(propName + ".iorinside")) {
			SLG_LOG("WARNING: deprecated property " + propName + ".iorinside");
			interiorIor = GetTexture(props.Get(Property(propName + ".iorinside")(1.5f)));
		} else if (props.IsDefined(propName + ".interiorior"))
			interiorIor = GetTexture(props.Get(Property(propName + ".interiorior")(1.5f)));

		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		
		const Texture *filmThickness = NULL;
		if (props.IsDefined(propName + ".filmthickness"))
			filmThickness = GetTexture(props.Get(Property(propName + ".filmthickness")(0.f)));
		
		const Texture *filmIor = NULL;
		if (props.IsDefined(propName + ".filmior"))
			filmIor = GetTexture(props.Get(Property(propName + ".filmior")(1.5f)));

		mat = new RoughGlassMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kr, kt, exteriorIor, interiorIor, nu, nv, filmThickness, filmIor);
	} else if (matType == "velvet") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		const Texture *p1 = GetTexture(props.Get(Property(propName + ".p1")(-2.0f)));
		const Texture *p2 = GetTexture(props.Get(Property(propName + ".p2")(20.0f)));
		const Texture *p3 = GetTexture(props.Get(Property(propName + ".p3")(2.0f)));
		const Texture *thickness = GetTexture(props.Get(Property(propName + ".thickness")(0.1f)));

		mat = new VelvetMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, p1, p2, p3, thickness);
	} else if (matType == "cloth") {
		slg::ocl::ClothPreset preset = slg::ocl::DENIM;

		if (props.IsDefined(propName + ".preset")) {
			const string type = props.Get(Property(propName + ".preset")("denim")).Get<string>();

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
		const Texture *weft_kd = GetTexture(props.Get(Property(propName + ".weft_kd")(.5f, .5f, .5f)));
		const Texture *weft_ks = GetTexture(props.Get(Property(propName + ".weft_ks")(.5f, .5f, .5f)));
		const Texture *warp_kd = GetTexture(props.Get(Property(propName + ".warp_kd")(.5f, .5f, .5f)));
		const Texture *warp_ks = GetTexture(props.Get(Property(propName + ".warp_ks")(.5f, .5f, .5f)));
		const float repeat_u = props.Get(Property(propName + ".repeat_u")(100.0f)).Get<float>();
		const float repeat_v = props.Get(Property(propName + ".repeat_v")(100.0f)).Get<float>();

		mat = new ClothMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, preset, weft_kd, weft_ks, warp_kd, warp_ks, repeat_u, repeat_v);
	} else if (matType == "carpaint") {
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));

        mat = NULL; // To remove a GCC warning
		string preset = props.Get(Property(propName + ".preset")("")).Get<string>();
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
				const Texture *kd = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-kd"))
					(CarPaintMaterial::data[i].kd[0], CarPaintMaterial::data[i].kd[1], CarPaintMaterial::data[i].kd[2]));
				const Texture *ks1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks1"))
					(CarPaintMaterial::data[i].ks1[0], CarPaintMaterial::data[i].ks1[1], CarPaintMaterial::data[i].ks1[2]));
				const Texture *ks2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks2"))
					(CarPaintMaterial::data[i].ks2[0], CarPaintMaterial::data[i].ks2[1], CarPaintMaterial::data[i].ks2[2]));
				const Texture *ks3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-ks3"))
					(CarPaintMaterial::data[i].ks3[0], CarPaintMaterial::data[i].ks3[1], CarPaintMaterial::data[i].ks3[2]));
				const Texture *r1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r1"))
					(CarPaintMaterial::data[i].r1));
				const Texture *r2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r2"))
					(CarPaintMaterial::data[i].r2));
				const Texture *r3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-r3"))
					(CarPaintMaterial::data[i].r3));
				const Texture *m1 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m1"))
					(CarPaintMaterial::data[i].m1));
				const Texture *m2 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m2"))
					(CarPaintMaterial::data[i].m2));
				const Texture *m3 = GetTexture(Property(NamedObject::GetUniqueName(matName + "-Implicit-" + preset + "-m3"))
					(CarPaintMaterial::data[i].m3));
				mat = new CarPaintMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, ks1, ks2, ks3, m1, m2, m3, r1, r2, r3, ka, d);
			}
		}

		// preset can be reset above if the name is not found
		if (preset == "") {
			const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(CarPaintMaterial::data[0].kd[0], CarPaintMaterial::data[0].kd[1], CarPaintMaterial::data[0].kd[2])));
			const Texture *ks1 = GetTexture(props.Get(Property(propName + ".ks1")(CarPaintMaterial::data[0].ks1[0], CarPaintMaterial::data[0].ks1[1], CarPaintMaterial::data[0].ks1[2])));
			const Texture *ks2 = GetTexture(props.Get(Property(propName + ".ks2")(CarPaintMaterial::data[0].ks2[0], CarPaintMaterial::data[0].ks2[1], CarPaintMaterial::data[0].ks2[2])));
			const Texture *ks3 = GetTexture(props.Get(Property(propName + ".ks3")(CarPaintMaterial::data[0].ks3[0], CarPaintMaterial::data[0].ks3[1], CarPaintMaterial::data[0].ks3[2])));
			const Texture *r1 = GetTexture(props.Get(Property(propName + ".r1")(CarPaintMaterial::data[0].r1)));
			const Texture *r2 = GetTexture(props.Get(Property(propName + ".r2")(CarPaintMaterial::data[0].r2)));
			const Texture *r3 = GetTexture(props.Get(Property(propName + ".r3")(CarPaintMaterial::data[0].r3)));
			const Texture *m1 = GetTexture(props.Get(Property(propName + ".m1")(CarPaintMaterial::data[0].m1)));
			const Texture *m2 = GetTexture(props.Get(Property(propName + ".m2")(CarPaintMaterial::data[0].m2)));
			const Texture *m3 = GetTexture(props.Get(Property(propName + ".m3")(CarPaintMaterial::data[0].m3)));
			mat = new CarPaintMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, ks1, ks2, ks3, m1, m2, m3, r1, r2, r3, ka, d);
		}
	} else if (matType == "glossytranslucent") {
		const Texture *kd = GetTexture(props.Get(Property(propName + ".kd")(.5f, .5f, .5f)));
		const Texture *kt = GetTexture(props.Get(Property(propName + ".kt")(.5f, .5f, .5f)));
		const Texture *ks = GetTexture(props.Get(Property(propName + ".ks")(.5f, .5f, .5f)));
		const Texture *ks_bf = GetTexture(props.Get(Property(propName + ".ks_bf")(.5f, .5f, .5f)));
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nu_bf = GetTexture(props.Get(Property(propName + ".uroughness_bf")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		const Texture *nv_bf = GetTexture(props.Get(Property(propName + ".vroughness_bf")(.1f)));
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *ka_bf = GetTexture(props.Get(Property(propName + ".ka_bf")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));
		const Texture *d_bf = GetTexture(props.Get(Property(propName + ".d_bf")(0.f)));
		const Texture *index = GetTexture(props.Get(Property(propName + ".index")(0.f, 0.f, 0.f)));
		const Texture *index_bf = GetTexture(props.Get(Property(propName + ".index_bf")(0.f, 0.f, 0.f)));
		const bool multibounce = props.Get(Property(propName + ".multibounce")(false)).Get<bool>();
		const bool multibounce_bf = props.Get(Property(propName + ".multibounce_bf")(false)).Get<bool>();

		mat = new GlossyTranslucentMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, kd, kt, ks, ks_bf, nu, nu_bf, nv, nv_bf,
			ka, ka_bf, d, d_bf, index, index_bf, multibounce, multibounce_bf);
	} else if (matType == "glossycoating") {
		const Material *matBase = matDefs.GetMaterial(props.Get(Property(propName + ".base")("")).Get<string>());
		const Texture *ks = GetTexture(props.Get(Property(propName + ".ks")(.5f, .5f, .5f)));
		const Texture *nu = GetTexture(props.Get(Property(propName + ".uroughness")(.1f)));
		const Texture *nv = GetTexture(props.Get(Property(propName + ".vroughness")(.1f)));
		const Texture *ka = GetTexture(props.Get(Property(propName + ".ka")(0.f, 0.f, 0.f)));
		const Texture *d = GetTexture(props.Get(Property(propName + ".d")(0.f)));
		const Texture *index = GetTexture(props.Get(Property(propName + ".index")(0.f, 0.f, 0.f)));
		const bool multibounce = props.Get(Property(propName + ".multibounce")(false)).Get<bool>();

		mat = new GlossyCoatingMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, matBase, ks, nu, nv, ka, d, index, multibounce);
	} else if (matType == "disney") {
		const Texture *baseColor = GetTexture(props.Get(Property(propName + ".basecolor")(.5f, .5f, .5f)));
		const Texture *subsurface = GetTexture(props.Get(Property(propName + ".subsurface")(0.f)));
		const Texture *roughness = GetTexture(props.Get(Property(propName + ".roughness")(0.f)));
		const Texture *metallic = GetTexture(props.Get(Property(propName + ".metallic")(0.f)));
		const Texture *specular = GetTexture(props.Get(Property(propName + ".specular")(0.f)));
		const Texture *specularTint = GetTexture(props.Get(Property(propName + ".speculartint")(0.f)));
		const Texture *clearcoat = GetTexture(props.Get(Property(propName + ".clearcoat")(0.f)));
		const Texture *clearcoatGloss = GetTexture(props.Get(Property(propName + ".clearcoatgloss")(0.f)));
		const Texture *anisotropic = GetTexture(props.Get(Property(propName + ".anisotropic")(0.f)));
		const Texture *sheen = GetTexture(props.Get(Property(propName + ".sheen")(0.f)));
		const Texture *sheenTint = GetTexture(props.Get(Property(propName + ".sheentint")(0.f)));
		
		const Texture *filmAmount = NULL;
		if (props.IsDefined(propName + ".filmamount"))
			filmAmount = GetTexture(props.Get(Property(propName + ".filmamount")(1.f)));
		
		const Texture *filmThickness = NULL;
		if (props.IsDefined(propName + ".filmthickness"))
			filmThickness = GetTexture(props.Get(Property(propName + ".filmthickness")(0.f)));
		
		const Texture *filmIor = NULL;
		if (props.IsDefined(propName + ".filmior"))
			filmIor = GetTexture(props.Get(Property(propName + ".filmior")(1.5f)));

		mat = new DisneyMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, baseColor, subsurface, roughness, metallic,
			specular, specularTint, clearcoat, clearcoatGloss, anisotropic, sheen, sheenTint, filmAmount, filmThickness, filmIor);
	} else if (matType == "twosided") {
		const Material *frontMat = matDefs.GetMaterial(props.Get(Property(propName + ".frontmaterial")("front")).Get<string>());
		const Material *backMat = matDefs.GetMaterial(props.Get(Property(propName + ".backmaterial")("back")).Get<string>());

		TwoSidedMaterial *twoSided = new TwoSidedMaterial(frontTransparencyTex, backTransparencyTex, emissionTex, bumpTex, frontMat, backMat);

		// Check if there is a loop in Two-sided material definition
		// (Note: this can not really happen at the moment because forward
		// declarations are not supported)
		if (twoSided->IsReferencing(twoSided))
			throw runtime_error("There is a loop in Two-sided material definition: " + matName);

		mat = twoSided;
	} else
		throw runtime_error("Unknown material type: " + matType);

	mat->SetName(matName);

	mat->SetID(props.Get(Property(propName + ".id")(defaultMatID)).Get<u_int>());
	mat->SetBumpSampleDistance(bumpSampleDistance);

	// Gain is not really a color so I avoid to use GetColor()
	mat->SetEmittedGain(props.Get(Property(propName + ".emission.gain")(Spectrum(1.f))).Get<Spectrum>());
	mat->SetEmittedPower(Max(0.f, props.Get(Property(propName + ".emission.power")(0.f)).Get<float>()));
	mat->SetEmittedPowerNormalize(props.Get(Property(propName + ".emission.normalizebycolor")(true)).Get<bool>());
	mat->SetEmittedGainNormalize(props.Get(Property(propName + ".emission.gain.normalizebycolor")(false)).Get<bool>());
	mat->SetEmittedEfficency(Max(0.f, props.Get(Property(propName + ".emission.efficency")(0.f)).Get<float>()));
	mat->SetEmittedTheta(Clamp(props.Get(Property(propName + ".emission.theta")(90.f)).Get<float>(), 0.f, 90.f));
	mat->SetLightID(props.Get(Property(propName + ".emission.id")(0u)).Get<u_int>());
	mat->SetEmittedImportance(props.Get(Property(propName + ".emission.importance")(1.f)).Get<float>());
	mat->SetEmittedTemperature(props.Get(Property(propName + ".emission.temperature")(-1.f)).Get<float>());
	mat->SetEmittedTemperatureNormalize(props.Get(Property(propName + ".emission.temperature.normalize")(false)).Get<float>());

	mat->SetPassThroughShadowTransparency(GetColor(Property(propName + ".transparency.shadow")(Spectrum(0.f))));

	const string dlsType = props.Get(Property(propName + ".emission.directlightsampling.type")("AUTO")).Get<string>();
	if (dlsType == "ENABLED")
		mat->SetDirectLightSamplingType(DLS_ENABLED);
	else if (dlsType == "DISABLED")
		mat->SetDirectLightSamplingType(DLS_DISABLED);
	else if (dlsType == "AUTO")
		mat->SetDirectLightSamplingType(DLS_AUTO);
	else
		throw runtime_error("Unknown material emission direct sampling type: " + dlsType);

	mat->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
	mat->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
	mat->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());
	
	mat->SetShadowCatcher(props.Get(Property(propName + ".shadowcatcher.enable")(false)).Get<bool>());
	mat->SetShadowCatcherOnlyInfiniteLights(props.Get(Property(propName + ".shadowcatcher.onlyinfinitelights")(false)).Get<bool>());

	mat->SetPhotonGIEnabled(props.Get(Property(propName + ".photongi.enable")(true)).Get<bool>());
	mat->SetHoldout(props.Get(Property(propName + ".holdout.enable")(false)).Get<bool>());

	// Check if there is a image or IES map
	const ImageMap *emissionMap = CreateEmissionMap(propName + ".emission", props);
	if (emissionMap) {
		// There is one
		mat->SetEmissionMap(emissionMap);
	}

	// Interior volumes
	if (props.IsDefined(propName + ".volume.interior")) {
		const string volName = props.Get(Property(propName + ".volume.interior")("vol1")).Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for material interior volume: " + matName);
		mat->SetInteriorVolume(v);
	}

	// Exterior volumes
	if (props.IsDefined(propName + ".volume.exterior")) {
		const string volName = props.Get(Property(propName + ".volume.exterior")("vol2")).Get<string>();
		const Material *m = matDefs.GetMaterial(volName);
		const Volume *v = dynamic_cast<const Volume *>(m);
		if (!v)
			throw runtime_error(volName + " is not a volume and can not be used for material exterior volume: " + matName);
		mat->SetExteriorVolume(v);
	}

	return mat;
}
