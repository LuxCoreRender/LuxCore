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

#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"

#include "slg/materials/archglass.h"
#include "slg/materials/carpaint.h"
#include "slg/materials/cloth.h"
#include "slg/materials/glass.h"
#include "slg/materials/glossy2.h"
#include "slg/materials/glossycoatting.h"
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

#include "slg/volumes/clear.h"
#include "slg/volumes/heterogenous.h"
#include "slg/volumes/homogenous.h"

using namespace std;
using namespace luxrays;
using namespace slg;

static bool IsTexConstant(const Texture *tex) {
	return (dynamic_cast<const ConstFloatTexture *>(tex)) ||
			(dynamic_cast<const ConstFloat3Texture *>(tex));
}

static bool IsMaterialDynamic(const slg::ocl::Material *material) {
		return (material->type == slg::ocl::MIX) || (material->type == slg::ocl::GLOSSYCOATING);
}

static float GetTexConstantFloatValue(const Texture *tex) {
	// Check if a texture is constant and return the value
	const ConstFloatTexture *cft = dynamic_cast<const ConstFloatTexture *>(tex);
	if (cft)
		return cft->GetValue();
	const ConstFloat3Texture *cf3t = dynamic_cast<const ConstFloat3Texture *>(tex);
	if (cf3t)
		return cf3t->GetColor().Y();

	return numeric_limits<float>::infinity();
}

void CompiledScene::AddEnabledMaterialCode() {
	// Optionally include the code for the specified materials in order to reduce
	// the number of OpenCL kernel compilation that may be required

	if (enabledCode.count(Material::MaterialType2String(MATTE))) usedMaterialTypes.insert(MATTE);
	if (enabledCode.count(Material::MaterialType2String(MIRROR))) usedMaterialTypes.insert(MIRROR);
	if (enabledCode.count(Material::MaterialType2String(GLASS))) usedMaterialTypes.insert(GLASS);
	if (enabledCode.count(Material::MaterialType2String(ARCHGLASS))) usedMaterialTypes.insert(ARCHGLASS);
	if (enabledCode.count(Material::MaterialType2String(MIX))) usedMaterialTypes.insert(MIX);
	if (enabledCode.count(Material::MaterialType2String(NULLMAT))) usedMaterialTypes.insert(NULLMAT);
	if (enabledCode.count(Material::MaterialType2String(MATTETRANSLUCENT))) usedMaterialTypes.insert(MATTETRANSLUCENT);
	if (enabledCode.count(Material::MaterialType2String(GLOSSY2))) usedMaterialTypes.insert(GLOSSY2);
	if (enabledCode.count(Material::MaterialType2String(METAL2))) usedMaterialTypes.insert(METAL2);
	if (enabledCode.count(Material::MaterialType2String(ROUGHGLASS))) usedMaterialTypes.insert(ROUGHGLASS);
	if (enabledCode.count(Material::MaterialType2String(VELVET))) usedMaterialTypes.insert(VELVET);
	if (enabledCode.count(Material::MaterialType2String(CLOTH))) usedMaterialTypes.insert(CLOTH);
	if (enabledCode.count(Material::MaterialType2String(CARPAINT))) usedMaterialTypes.insert(CARPAINT);
	if (enabledCode.count(Material::MaterialType2String(ROUGHMATTE))) usedMaterialTypes.insert(ROUGHMATTE);
	if (enabledCode.count(Material::MaterialType2String(ROUGHMATTETRANSLUCENT))) usedMaterialTypes.insert(ROUGHMATTETRANSLUCENT);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYTRANSLUCENT))) usedMaterialTypes.insert(GLOSSYTRANSLUCENT);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYCOATING))) usedMaterialTypes.insert(GLOSSYCOATING);
	// Volumes
	if (enabledCode.count(Material::MaterialType2String(HOMOGENEOUS_VOL))) usedMaterialTypes.insert(HOMOGENEOUS_VOL);
	if (enabledCode.count(Material::MaterialType2String(CLEAR_VOL))) usedMaterialTypes.insert(CLEAR_VOL);
	if (enabledCode.count(Material::MaterialType2String(HETEROGENEOUS_VOL))) usedMaterialTypes.insert(HETEROGENEOUS_VOL);
	// The following types are used (in PATHOCL CompiledScene class) only to
	// recognize the usage of some specific material option
	if (enabledCode.count(Material::MaterialType2String(GLOSSY2_ANISOTROPIC))) usedMaterialTypes.insert(GLOSSY2_ANISOTROPIC);
	if (enabledCode.count(Material::MaterialType2String(GLOSSY2_ABSORPTION))) usedMaterialTypes.insert(GLOSSY2_ABSORPTION);
	if (enabledCode.count(Material::MaterialType2String(GLOSSY2_INDEX))) usedMaterialTypes.insert(GLOSSY2_INDEX);
	if (enabledCode.count(Material::MaterialType2String(GLOSSY2_MULTIBOUNCE))) usedMaterialTypes.insert(GLOSSY2_MULTIBOUNCE);

	if (enabledCode.count(Material::MaterialType2String(GLOSSYTRANSLUCENT_ANISOTROPIC))) usedMaterialTypes.insert(GLOSSYTRANSLUCENT_ANISOTROPIC);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYTRANSLUCENT_ABSORPTION))) usedMaterialTypes.insert(GLOSSYTRANSLUCENT_ABSORPTION);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYTRANSLUCENT_INDEX))) usedMaterialTypes.insert(GLOSSYTRANSLUCENT_INDEX);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYTRANSLUCENT_MULTIBOUNCE))) usedMaterialTypes.insert(GLOSSYTRANSLUCENT_MULTIBOUNCE);

	if (enabledCode.count(Material::MaterialType2String(GLOSSYCOATING_ANISOTROPIC))) usedMaterialTypes.insert(GLOSSYCOATING_ANISOTROPIC);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYCOATING_ABSORPTION))) usedMaterialTypes.insert(GLOSSYCOATING_ABSORPTION);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYCOATING_INDEX))) usedMaterialTypes.insert(GLOSSYCOATING_INDEX);
	if (enabledCode.count(Material::MaterialType2String(GLOSSYCOATING_MULTIBOUNCE))) usedMaterialTypes.insert(GLOSSYCOATING_MULTIBOUNCE);

	if (enabledCode.count(Material::MaterialType2String(METAL2_ANISOTROPIC))) usedMaterialTypes.insert(METAL2_ANISOTROPIC);
	if (enabledCode.count(Material::MaterialType2String(ROUGHGLASS_ANISOTROPIC))) usedMaterialTypes.insert(ROUGHGLASS_ANISOTROPIC);
}

void CompiledScene::CompileMaterials() {
	CompileTextures();

	const u_int materialsCount = scene->matDefs.GetSize();
	SLG_LOG("Compile " << materialsCount << " Materials");

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedMaterialTypes.clear();
	AddEnabledMaterialCode();

	mats.resize(materialsCount);
	useBumpMapping = false;

	for (u_int i = 0; i < materialsCount; ++i) {
		Material *m = scene->matDefs.GetMaterial(i);
		slg::ocl::Material *mat = &mats[i];
		//SLG_LOG(" Type: " << m->GetType());

		mat->matID = m->GetID();
		mat->lightID = m->GetLightID();
        mat->bumpSampleDistance = m->GetBumpSampleDistance();

		// Material emission
		const Texture *emitTex = m->GetEmitTexture();
		if (emitTex)
			mat->emitTexIndex = scene->texDefs.GetTextureIndex(emitTex);
		else
			mat->emitTexIndex = NULL_INDEX;
		ASSIGN_SPECTRUM(mat->emittedFactor, m->GetEmittedFactor());
		mat->usePrimitiveArea = m->IsUsingPrimitiveArea();

		// Material bump mapping
		const Texture *bumpTex = m->GetBumpTexture();
		if (bumpTex) {
			mat->bumpTexIndex = scene->texDefs.GetTextureIndex(bumpTex);
			useBumpMapping = true;
		} else
			mat->bumpTexIndex = NULL_INDEX;

		mat->samples = m->GetSamples();
		mat->visibility =
				(m->IsVisibleIndirectDiffuse() ? DIFFUSE : NONE) |
				(m->IsVisibleIndirectGlossy() ? GLOSSY : NONE) |
				(m->IsVisibleIndirectSpecular() ? SPECULAR : NONE);

		// Material volumes
		const Material *interiorVol = m->GetInteriorVolume();
		mat->interiorVolumeIndex = interiorVol ? scene->matDefs.GetMaterialIndex(interiorVol) : NULL_INDEX;
		const Material *exteriorVol = m->GetExteriorVolume();
		mat->exteriorVolumeIndex = exteriorVol ? scene->matDefs.GetMaterialIndex(exteriorVol) : NULL_INDEX;

		// Material specific parameters
		usedMaterialTypes.insert(m->GetType());
		switch (m->GetType()) {
			case MATTE: {
				const MatteMaterial *mm = static_cast<MatteMaterial *>(m);

				mat->type = slg::ocl::MATTE;
				mat->matte.kdTexIndex = scene->texDefs.GetTextureIndex(mm->GetKd());
				break;
			}
			case ROUGHMATTE: {
				const RoughMatteMaterial *mm = static_cast<RoughMatteMaterial *>(m);

				mat->type = slg::ocl::ROUGHMATTE;
				mat->roughmatte.kdTexIndex = scene->texDefs.GetTextureIndex(mm->GetKd());
				mat->roughmatte.sigmaTexIndex = scene->texDefs.GetTextureIndex(mm->GetSigma());
				break;
			}
			case MIRROR: {
				const MirrorMaterial *mm = static_cast<MirrorMaterial *>(m);

				mat->type = slg::ocl::MIRROR;
				mat->mirror.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				break;
			}
			case GLASS: {
				const GlassMaterial *gm = static_cast<GlassMaterial *>(m);

				mat->type = slg::ocl::GLASS;
				mat->glass.krTexIndex = scene->texDefs.GetTextureIndex(gm->GetKr());
				mat->glass.ktTexIndex = scene->texDefs.GetTextureIndex(gm->GetKt());
				if (gm->GetExteriorIOR())
					mat->glass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetExteriorIOR());
				else
					mat->glass.exteriorIorTexIndex = NULL_INDEX;
				if (gm->GetInteriorIOR())
					mat->glass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetInteriorIOR());
				else
					mat->glass.interiorIorTexIndex = NULL_INDEX;
				break;
			}
			case ARCHGLASS: {
				const ArchGlassMaterial *am = static_cast<ArchGlassMaterial *>(m);

				mat->type = slg::ocl::ARCHGLASS;
				mat->archglass.krTexIndex = scene->texDefs.GetTextureIndex(am->GetKr());
				mat->archglass.ktTexIndex = scene->texDefs.GetTextureIndex(am->GetKt());
				if (am->GetExteriorIOR())
					mat->archglass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(am->GetExteriorIOR());
				else
					mat->archglass.exteriorIorTexIndex = NULL_INDEX;
				if (am->GetInteriorIOR())
					mat->archglass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(am->GetInteriorIOR());
				else
					mat->archglass.interiorIorTexIndex = NULL_INDEX;
				break;
			}
			case MIX: {
				const MixMaterial *mm = static_cast<MixMaterial *>(m);

				mat->type = slg::ocl::MIX;
				mat->mix.matAIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialA());
				mat->mix.matBIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialB());
				mat->mix.mixFactorTexIndex = scene->texDefs.GetTextureIndex(mm->GetMixFactor());
				break;
			}
			case NULLMAT: {
				mat->type = slg::ocl::NULLMAT;
				break;
			}
			case MATTETRANSLUCENT: {
				const MatteTranslucentMaterial *mt = static_cast<MatteTranslucentMaterial *>(m);

				mat->type = slg::ocl::MATTETRANSLUCENT;
				mat->matteTranslucent.krTexIndex = scene->texDefs.GetTextureIndex(mt->GetKr());
				mat->matteTranslucent.ktTexIndex = scene->texDefs.GetTextureIndex(mt->GetKt());
				break;
			}
			case ROUGHMATTETRANSLUCENT: {
				const RoughMatteTranslucentMaterial *rmt = static_cast<RoughMatteTranslucentMaterial *>(m);

				mat->type = slg::ocl::ROUGHMATTETRANSLUCENT;
				mat->roughmatteTranslucent.krTexIndex = scene->texDefs.GetTextureIndex(rmt->GetKr());
				mat->roughmatteTranslucent.ktTexIndex = scene->texDefs.GetTextureIndex(rmt->GetKt());
				mat->roughmatteTranslucent.sigmaTexIndex = scene->texDefs.GetTextureIndex(rmt->GetSigma());
				break;
			}
			case GLOSSY2: {
				const Glossy2Material *g2m = static_cast<Glossy2Material *>(m);

				mat->type = slg::ocl::GLOSSY2;
				mat->glossy2.kdTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKd());
				mat->glossy2.ksTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKs());

				const Texture *nuTex = g2m->GetNu();
				const Texture *nvTex = g2m->GetNv();
				mat->glossy2.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->glossy2.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(GLOSSY2_ANISOTROPIC);

				const Texture *depthTex = g2m->GetDepth();
				mat->glossy2.kaTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKa());
				mat->glossy2.depthTexIndex = scene->texDefs.GetTextureIndex(depthTex);
				// Check if depth is just 0.0
				if (IsTexConstant(depthTex) && (GetTexConstantFloatValue(depthTex) > 0.f))
					usedMaterialTypes.insert(GLOSSY2_ABSORPTION);

				const Texture *indexTex = g2m->GetIndex();
				mat->glossy2.indexTexIndex = scene->texDefs.GetTextureIndex(indexTex);
				// Check if index is just 0.0
				if (IsTexConstant(indexTex) && (GetTexConstantFloatValue(indexTex) > 0.f))
					usedMaterialTypes.insert(GLOSSY2_INDEX);

				mat->glossy2.multibounce = g2m->IsMultibounce() ? 1 : 0;
				// Check if multibounce is enabled
				if (g2m->IsMultibounce())
					usedMaterialTypes.insert(GLOSSY2_MULTIBOUNCE);
				break;
			}
			case METAL2: {
				const Metal2Material *m2m = static_cast<Metal2Material *>(m);

				mat->type = slg::ocl::METAL2;
				if (m2m->GetFresnel())
					mat->metal2.fresnelTexIndex = scene->texDefs.GetTextureIndex(m2m->GetFresnel());
				else
					mat->metal2.fresnelTexIndex = NULL_INDEX;
				if (m2m->GetN())
					mat->metal2.nTexIndex = scene->texDefs.GetTextureIndex(m2m->GetN());
				else
					mat->metal2.nTexIndex = NULL_INDEX;
				if (m2m->GetK())
					mat->metal2.kTexIndex = scene->texDefs.GetTextureIndex(m2m->GetK());
				else
					mat->metal2.kTexIndex = NULL_INDEX;

				const Texture *nuTex = m2m->GetNu();
				const Texture *nvTex = m2m->GetNv();
				mat->metal2.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->metal2.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(METAL2_ANISOTROPIC);
				break;
			}
			case ROUGHGLASS: {
				const RoughGlassMaterial *rgm = static_cast<RoughGlassMaterial *>(m);

				mat->type = slg::ocl::ROUGHGLASS;
				mat->roughglass.krTexIndex = scene->texDefs.GetTextureIndex(rgm->GetKr());
				mat->roughglass.ktTexIndex = scene->texDefs.GetTextureIndex(rgm->GetKt());
				if (rgm->GetExteriorIOR())
					mat->roughglass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetExteriorIOR());
				else
					mat->roughglass.exteriorIorTexIndex = NULL_INDEX;
				if (rgm->GetInteriorIOR())
					mat->roughglass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetInteriorIOR());
				else
					mat->roughglass.exteriorIorTexIndex = NULL_INDEX;

				const Texture *nuTex = rgm->GetNu();
				const Texture *nvTex = rgm->GetNv();
				mat->roughglass.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->roughglass.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(ROUGHGLASS_ANISOTROPIC);
				break;
			}
			case VELVET: {
				const VelvetMaterial *vm = static_cast<VelvetMaterial *>(m);

				mat->type = slg::ocl::VELVET;
				mat->velvet.kdTexIndex = scene->texDefs.GetTextureIndex(vm->GetKd());
				mat->velvet.p1TexIndex = scene->texDefs.GetTextureIndex(vm->GetP1());
				mat->velvet.p2TexIndex = scene->texDefs.GetTextureIndex(vm->GetP2());
				mat->velvet.p3TexIndex = scene->texDefs.GetTextureIndex(vm->GetP3());
				mat->velvet.thicknessTexIndex = scene->texDefs.GetTextureIndex(vm->GetThickness());
				break;
			}
			case CLOTH: {
				const ClothMaterial *cm = static_cast<ClothMaterial *>(m);

				mat->type = slg::ocl::CLOTH;
				mat->cloth.Preset = cm->GetPreset();
				mat->cloth.Weft_KdIndex = scene->texDefs.GetTextureIndex(cm->GetWeftKd());
				mat->cloth.Weft_KsIndex = scene->texDefs.GetTextureIndex(cm->GetWeftKs());
				mat->cloth.Warp_KdIndex = scene->texDefs.GetTextureIndex(cm->GetWarpKd());
				mat->cloth.Warp_KsIndex = scene->texDefs.GetTextureIndex(cm->GetWarpKs());
				mat->cloth.Repeat_U = cm->GetRepeatU();
				mat->cloth.Repeat_V = cm->GetRepeatV();
				mat->cloth.specularNormalization = cm->GetSpecularNormalization();
				break;
			}
			case CARPAINT: {
				const CarPaintMaterial *cm = static_cast<CarPaintMaterial *>(m);
				mat->type = slg::ocl::CARPAINT;
				mat->carpaint.KdTexIndex = scene->texDefs.GetTextureIndex(cm->Kd);
				mat->carpaint.Ks1TexIndex = scene->texDefs.GetTextureIndex(cm->Ks1);
				mat->carpaint.Ks2TexIndex = scene->texDefs.GetTextureIndex(cm->Ks2);
				mat->carpaint.Ks3TexIndex = scene->texDefs.GetTextureIndex(cm->Ks3);
				mat->carpaint.M1TexIndex = scene->texDefs.GetTextureIndex(cm->M1);
				mat->carpaint.M2TexIndex = scene->texDefs.GetTextureIndex(cm->M2);
				mat->carpaint.M3TexIndex = scene->texDefs.GetTextureIndex(cm->M3);
				mat->carpaint.R1TexIndex = scene->texDefs.GetTextureIndex(cm->R1);
				mat->carpaint.R2TexIndex = scene->texDefs.GetTextureIndex(cm->R2);
				mat->carpaint.R3TexIndex = scene->texDefs.GetTextureIndex(cm->R3);
				mat->carpaint.KaTexIndex = scene->texDefs.GetTextureIndex(cm->Ka);
				mat->carpaint.depthTexIndex = scene->texDefs.GetTextureIndex(cm->depth);
				break;
			}
			case GLOSSYTRANSLUCENT: {
				const GlossyTranslucentMaterial *gtm = static_cast<GlossyTranslucentMaterial *>(m);

				mat->type = slg::ocl::GLOSSYTRANSLUCENT;
				mat->glossytranslucent.kdTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKd());
				mat->glossytranslucent.ktTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKt());
				mat->glossytranslucent.ksTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKs());
				mat->glossytranslucent.ksbfTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKs_bf());

				const Texture *nuTex = gtm->GetNu();
				const Texture *nvTex = gtm->GetNv();
				mat->glossytranslucent.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->glossytranslucent.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				const Texture *nubfTex = gtm->GetNu_bf();
				const Texture *nvbfTex = gtm->GetNv_bf();
				mat->glossytranslucent.nubfTexIndex = scene->texDefs.GetTextureIndex(nubfTex);
				mat->glossytranslucent.nvbfTexIndex = scene->texDefs.GetTextureIndex(nvbfTex);
				// Check if it an anisotropic material
				if ((IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex))) ||
						(IsTexConstant(nubfTex) && IsTexConstant(nvbfTex) &&
						(GetTexConstantFloatValue(nubfTex) != GetTexConstantFloatValue(nvbfTex))))
					usedMaterialTypes.insert(GLOSSYTRANSLUCENT_ANISOTROPIC);

				const Texture *depthTex = gtm->GetDepth();
				mat->glossytranslucent.kaTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKa());
				mat->glossytranslucent.depthTexIndex = scene->texDefs.GetTextureIndex(depthTex);
				const Texture *depthbfTex = gtm->GetDepth_bf();
				mat->glossytranslucent.kabfTexIndex = scene->texDefs.GetTextureIndex(gtm->GetKa_bf());
				mat->glossytranslucent.depthbfTexIndex = scene->texDefs.GetTextureIndex(depthTex);
				// Check if depth is just 0.0
				if ((IsTexConstant(depthTex) && (GetTexConstantFloatValue(depthTex) > 0.f)) || (IsTexConstant(depthbfTex) && (GetTexConstantFloatValue(depthbfTex) > 0.f)))
					usedMaterialTypes.insert(GLOSSYTRANSLUCENT_ABSORPTION);

				const Texture *indexTex = gtm->GetIndex();
				mat->glossytranslucent.indexTexIndex = scene->texDefs.GetTextureIndex(indexTex);
				const Texture *indexbfTex = gtm->GetIndex_bf();
				mat->glossytranslucent.indexbfTexIndex = scene->texDefs.GetTextureIndex(indexTex);
				// Check if index is just 0.0
				if ((IsTexConstant(indexTex) && (GetTexConstantFloatValue(indexTex) > 0.f)) || (IsTexConstant(indexbfTex) && GetTexConstantFloatValue(indexbfTex)))
					usedMaterialTypes.insert(GLOSSYTRANSLUCENT_INDEX);

				mat->glossytranslucent.multibounce = gtm->IsMultibounce() ? 1 : 0;
				mat->glossytranslucent.multibouncebf = gtm->IsMultibounce_bf() ? 1 : 0;
				// Check if multibounce is enabled
				if (gtm->IsMultibounce() || gtm->IsMultibounce_bf())
					usedMaterialTypes.insert(GLOSSYTRANSLUCENT_MULTIBOUNCE);
				break;
			}
			case GLOSSYCOATING: {
				const GlossyCoatingMaterial *gcm = static_cast<GlossyCoatingMaterial *>(m);

				mat->type = slg::ocl::GLOSSYCOATING;
				mat->glossycoating.matBaseIndex = scene->matDefs.GetMaterialIndex(gcm->GetMaterialBase());
				mat->glossycoating.ksTexIndex = scene->texDefs.GetTextureIndex(gcm->GetKs());

				const Texture *nuTex = gcm->GetNu();
				const Texture *nvTex = gcm->GetNv();
				mat->glossycoating.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->glossycoating.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(GLOSSYCOATING_ANISOTROPIC);

				const Texture *depthTex = gcm->GetDepth();
				mat->glossycoating.kaTexIndex = scene->texDefs.GetTextureIndex(gcm->GetKa());
				mat->glossycoating.depthTexIndex = scene->texDefs.GetTextureIndex(depthTex);
				// Check if depth is just 0.0
				if (IsTexConstant(depthTex) && (GetTexConstantFloatValue(depthTex) > 0.f))
					usedMaterialTypes.insert(GLOSSYCOATING_ABSORPTION);

				const Texture *indexTex = gcm->GetIndex();
				mat->glossycoating.indexTexIndex = scene->texDefs.GetTextureIndex(indexTex);
				// Check if index is just 0.0
				if (IsTexConstant(indexTex) && (GetTexConstantFloatValue(indexTex) > 0.f))
					usedMaterialTypes.insert(GLOSSYCOATING_INDEX);

				mat->glossycoating.multibounce = gcm->IsMultibounce() ? 1 : 0;
				// Check if multibounce is enabled
				if (gcm->IsMultibounce())
					usedMaterialTypes.insert(GLOSSYCOATING_MULTIBOUNCE);
				break;
			}
			//------------------------------------------------------------------
			// Volumes
			//------------------------------------------------------------------
			case CLEAR_VOL:
			case HOMOGENEOUS_VOL:
			case HETEROGENEOUS_VOL: {
				const Volume *v = static_cast<ClearVolume *>(m);
				mat->volume.iorTexIndex = v->GetIORTexture() ?
					scene->texDefs.GetTextureIndex(v->GetIORTexture()) :
					NULL_INDEX;
				mat->volume.volumeEmissionTexIndex = v->GetVolumeEmissionTexture() ?
					scene->texDefs.GetTextureIndex(v->GetVolumeEmissionTexture()) :
					NULL_INDEX;
				mat->volume.volumeLightID = v->GetVolumeLightID();
				mat->volume.priority = v->GetPriority();

				switch (m->GetType()) {
					case CLEAR_VOL: {
						const ClearVolume *cv = static_cast<ClearVolume *>(m);
						mat->type = slg::ocl::CLEAR_VOL;
						mat->volume.clear.sigmaATexIndex = scene->texDefs.GetTextureIndex(cv->GetSigmaA());
						break;
					}
					case HOMOGENEOUS_VOL: {
						const HomogeneousVolume *hv = static_cast<HomogeneousVolume *>(m);
						mat->type = slg::ocl::HOMOGENEOUS_VOL;
						mat->volume.homogenous.sigmaATexIndex = scene->texDefs.GetTextureIndex(hv->GetSigmaA());
						mat->volume.homogenous.sigmaSTexIndex = scene->texDefs.GetTextureIndex(hv->GetSigmaS());
						mat->volume.homogenous.gTexIndex = scene->texDefs.GetTextureIndex(hv->GetG());
						mat->volume.homogenous.multiScattering = hv->IsMultiScattering();
						break;
					}
					case HETEROGENEOUS_VOL: {
						const HeterogeneousVolume *hv = static_cast<HeterogeneousVolume *>(m);
						mat->type = slg::ocl::HETEROGENEOUS_VOL;
						mat->volume.heterogenous.sigmaATexIndex = scene->texDefs.GetTextureIndex(hv->GetSigmaA());
						mat->volume.heterogenous.sigmaSTexIndex = scene->texDefs.GetTextureIndex(hv->GetSigmaS());
						mat->volume.heterogenous.gTexIndex = scene->texDefs.GetTextureIndex(hv->GetG());
						mat->volume.heterogenous.stepSize = hv->GetStepSize();
						mat->volume.heterogenous.maxStepsCount = hv->GetMaxStepsCount();
						mat->volume.heterogenous.multiScattering = hv->IsMultiScattering();
						break;
					}
					default:
						throw runtime_error("Unknown volume in CompiledScene::CompileMaterials(): " + boost::lexical_cast<string>(m->GetType()));
				}
				break;
			}
			default:
				throw runtime_error("Unknown material in CompiledScene::CompileMaterials(): " + boost::lexical_cast<string>(m->GetType()));
		}
	}

	//--------------------------------------------------------------------------
	// Default scene volume
	//--------------------------------------------------------------------------
	defaultWorldVolumeIndex = scene->defaultWorldVolume ?
		scene->matDefs.GetMaterialIndex(scene->defaultWorldVolume) : NULL_INDEX;

	const double tEnd = WallClockTime();
	SLG_LOG("Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

//------------------------------------------------------------------------------
// Dynamic OpenCL code generation for material evaluation
//------------------------------------------------------------------------------

static string AddTextureSourceCall(const vector<slg::ocl::Texture> &texs,
		const string &type, const string &index) {
	return "Texture_Get" + type + "Value(" + index + ", hitPoint TEXTURES_PARAM)";
}

static void AddMaterialSourceSwitch(stringstream &source, const vector<slg::ocl::Material> &mats,
		const string &funcName, const string &calledFuncName,
		const string &returnType, const string &defaultReturnValue,
		const string &args,  const string &params, const bool hasReturn = true) {
	source << returnType << " Material_" << funcName << "(" << args << ") { \n"
			"\t__global const Material *mat = &mats[index];\n"
			"\tswitch (index) {\n";
	for (u_int i = 0; i < mats.size(); ++i) {
		if (IsMaterialDynamic(&mats[i])) {
			source << "\t\tcase " << i << ":\n";
			source << "\t\t\t" <<
					(hasReturn ? "return " : "") <<
					"Material_Index" << i << "_" << calledFuncName << "(" << params << ");\n";
			if (!hasReturn)
				source << "\t\t\tbreak;\n";
		}
	}

	if (hasReturn) {
		source << "\t\tdefault:\n"
				"\t\t\treturn " << defaultReturnValue<< ";\n";
	}

	source <<
			"\t}\n"
			"}\n";
}

string CompiledScene::GetMaterialsEvaluationSourceCode() const {
	// Generate the source code for each material that reference other materials
	// and constant materials
	stringstream source;

	const u_int materialsCount = mats.size();
	for (u_int i = 0; i < materialsCount; ++i) {
		const slg::ocl::Material *mat = &mats[i];

		switch (mat->type) {
			case slg::ocl::MATTE:
			case slg::ocl::ROUGHMATTE:
			case slg::ocl::ARCHGLASS:
			case slg::ocl::CARPAINT:
			case slg::ocl::CLOTH:
			case slg::ocl::GLASS:
			case slg::ocl::GLOSSY2:
			case slg::ocl::MATTETRANSLUCENT:
			case slg::ocl::ROUGHMATTETRANSLUCENT:
			case slg::ocl::METAL2:
			case slg::ocl::MIRROR:
			case slg::ocl::NULLMAT:
			case slg::ocl::ROUGHGLASS:
			case slg::ocl::VELVET:
			case slg::ocl::GLOSSYTRANSLUCENT:
			case slg::ocl::CLEAR_VOL:
			case slg::ocl::HOMOGENEOUS_VOL:
			case slg::ocl::HETEROGENEOUS_VOL:
				break;
			case slg::ocl::MIX: {
				// MIX material uses a template .cl file
				string mixSrc = slg::ocl::KernelSource_materialdefs_template_mix;
				boost::replace_all(mixSrc, "<<CS_MIX_MATERIAL_INDEX>>", ToString(i));

				boost::replace_all(mixSrc, "<<CS_MAT_A_MATERIAL_INDEX>>", ToString(mat->mix.matAIndex));
				const bool isDynamicMatA = IsMaterialDynamic(&mats[mat->mix.matAIndex]);
				boost::replace_all(mixSrc, "<<CS_MAT_A_PREFIX>>", isDynamicMatA ?
					("Material_Index" + ToString(mat->mix.matAIndex)) : "Material");
				boost::replace_all(mixSrc, "<<CS_MAT_A_POSTFIX>>", isDynamicMatA ?
					"" : "WithoutDynamic");

				boost::replace_all(mixSrc, "<<CS_MAT_B_MATERIAL_INDEX>>", ToString(mat->mix.matBIndex));
				const bool isDynamicMatB = IsMaterialDynamic(&mats[mat->mix.matBIndex]);
				boost::replace_all(mixSrc, "<<CS_MAT_B_PREFIX>>", isDynamicMatB ?
					("Material_Index" + ToString(mat->mix.matBIndex)) : "Material");
				boost::replace_all(mixSrc, "<<CS_MAT_B_POSTFIX>>", isDynamicMatB ?
					"" : "WithoutDynamic");

				boost::replace_all(mixSrc, "<<CS_FACTOR_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->mix.mixFactorTexIndex"));
				source << mixSrc;
				break;
			}
			case slg::ocl::GLOSSYCOATING: {
				// GLOSSYCOATING material uses a template .cl file
				string glossyCoatingSrc = slg::ocl::KernelSource_materialdefs_template_glossycoating;
				boost::replace_all(glossyCoatingSrc, "<<CS_GLOSSYCOATING_MATERIAL_INDEX>>", ToString(i));

				boost::replace_all(glossyCoatingSrc, "<<CS_MAT_BASE_MATERIAL_INDEX>>", ToString(mat->glossycoating.matBaseIndex));
				const bool isDynamicMatBase = IsMaterialDynamic(&mats[mat->glossycoating.matBaseIndex]);
				boost::replace_all(glossyCoatingSrc, "<<CS_MAT_BASE_PREFIX>>", isDynamicMatBase ?
					("Material_Index" + ToString(mat->glossycoating.matBaseIndex)) : "Material");
				boost::replace_all(glossyCoatingSrc, "<<CS_MAT_BASE_POSTFIX>>", isDynamicMatBase ?
					"" : "WithoutDynamic");

				boost::replace_all(glossyCoatingSrc, "<<CS_KS_TEXTURE>>", AddTextureSourceCall(texs, "Spectrum", "material->glossycoating.ksTexIndex"));
				boost::replace_all(glossyCoatingSrc, "<<CS_NU_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.nuTexIndex"));
				boost::replace_all(glossyCoatingSrc, "<<CS_NV_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.nvTexIndex"));
				boost::replace_all(glossyCoatingSrc, "<<CS_KA_TEXTURE>>", AddTextureSourceCall(texs, "Spectrum", "material->glossycoating.kaTexIndex"));
				boost::replace_all(glossyCoatingSrc, "<<CS_DEPTH_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.depthTexIndex"));
				boost::replace_all(glossyCoatingSrc, "<<CS_INDEX_TEXTURE_>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.indexTexIndex"));
				if (mat->glossycoating.multibounce)
					boost::replace_all(glossyCoatingSrc, "<<CS_MB_FLAG>>", "true");
				else
					boost::replace_all(glossyCoatingSrc, "<<CS_MB_FLAG>>", "false");
				source << glossyCoatingSrc;
				break;
			}
			default:
				throw runtime_error("Unknown material in CompiledScene::GetMaterialsEvaluationSourceCode(): " + boost::lexical_cast<string>(mat->type));
				break;
		}
	}

	// Generate the code for generic Material_GetEventTypesWithDynamic())
	AddMaterialSourceSwitch(source, mats, "GetEventTypesWithDynamic", "GetEventTypes", "BSDFEvent", "NONE",
			"const uint index MATERIALS_PARAM_DECL",
			"mat MATERIALS_PARAM");

	// Generate the code for generic Material_IsDeltaWithDynamic()
	AddMaterialSourceSwitch(source, mats, "IsDeltaWithDynamic", "IsDelta", "bool", "true",
			"const uint index MATERIALS_PARAM_DECL",
			"mat MATERIALS_PARAM");

	// Generate the code for generic Material_EvaluateWithDynamic()
	AddMaterialSourceSwitch(source, mats, "EvaluateWithDynamic", "Evaluate", "float3", "BLACK",
			"const uint index, "
				"__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir, "
				"BSDFEvent *event, float *directPdfW "
				"MATERIALS_PARAM_DECL",
			"mat, hitPoint, lightDir, eyeDir, event, directPdfW MATERIALS_PARAM");

	// Generate the code for generic Material_SampleWithDynamic()
	AddMaterialSourceSwitch(source, mats, "SampleWithDynamic", "Sample", "float3", "BLACK",
			"const uint index, "
				"__global HitPoint *hitPoint, "
				"const float3 fixedDir, float3 *sampledDir, "
				"const float u0, const float u1,\n"
				"#if defined(PARAM_HAS_PASSTHROUGH)\n"
				"\tconst float passThroughEvent,\n"
				"#endif\n"
				"\tfloat *pdfW, float *cosSampledDir, BSDFEvent *event, "
				"const BSDFEvent requestedEvent "
				"MATERIALS_PARAM_DECL",
			"mat, hitPoint, fixedDir, sampledDir, u0, u1,\n"
				"#if defined(PARAM_HAS_PASSTHROUGH)\n"
				"\t\t\tpassThroughEvent,\n"
				"#endif\n"
				"\t\t\tpdfW,  cosSampledDir, event, requestedEvent MATERIALS_PARAM");

	// Generate the code for generic GetPassThroughTransparencyWithDynamic()
	source << "#if defined(PARAM_HAS_PASSTHROUGH)\n";
	AddMaterialSourceSwitch(source, mats, "GetPassThroughTransparencyWithDynamic", "GetPassThroughTransparency", "float3", "BLACK",
			"const uint index, __global HitPoint *hitPoint, "
				"const float3 localFixedDir, const float passThroughEvent MATERIALS_PARAM_DECL",
			"mat, hitPoint, localFixedDir, passThroughEvent MATERIALS_PARAM");
	source << "#endif\n";

	// Generate the code for generic Material_GetEmittedRadianceWithDynamic()
	AddMaterialSourceSwitch(source, mats, "GetEmittedRadianceWithDynamic", "GetEmittedRadiance", "float3", "BLACK",
			"const uint index, __global HitPoint *hitPoint MATERIALS_PARAM_DECL",
			"mat, hitPoint MATERIALS_PARAM");

	// Generate the code for generic Material_GetInteriorVolumeWithDynamic() and Material_GetExteriorVolumeWithDynamic()
	source << "#if defined(PARAM_HAS_VOLUMES)\n";
	AddMaterialSourceSwitch(source, mats, "GetInteriorVolumeWithDynamic", "GetInteriorVolume", "uint", "NULL_INDEX",
			"const uint index, __global HitPoint *hitPoint, const float passThroughEvent MATERIALS_PARAM_DECL",
			"mat, hitPoint, passThroughEvent MATERIALS_PARAM");
	AddMaterialSourceSwitch(source, mats, "GetExteriorVolumeWithDynamic", "GetExteriorVolume", "uint", "NULL_INDEX",
			"const uint index, __global HitPoint *hitPoint, const float passThroughEvent MATERIALS_PARAM_DECL",
			"mat, hitPoint, passThroughEvent MATERIALS_PARAM");
	source << "#endif\n";

	return source.str();
}

#endif
