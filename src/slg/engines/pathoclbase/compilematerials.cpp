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

void CompiledScene::CompileMaterials() {
	CompileTextures();

	const u_int materialsCount = scene->matDefs.GetSize();
	SLG_LOG("Compile " << materialsCount << " Materials");

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedMaterialTypes.clear();

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
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const u_int objCount = scene->objDefs.GetSize();
	meshMats.resize(objCount);
	for (u_int i = 0; i < objCount; ++i) {
		const Material *m = scene->objDefs.GetSceneObject(i)->GetMaterial();
		meshMats[i] = scene->matDefs.GetMaterialIndex(m);
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

static void AddMaterialSourceGlue(stringstream &source,
		const string &matName, const u_int i, const string &funcName1, const string &funcName2,
		const string &returnType, const string &args,  const string &params, const bool hasReturn = true) {
	source <<
			returnType << " Material_Index" << i << "_" << funcName1 << "(" << args << ") {\n"
			"\t" <<
			(hasReturn ? "return " : " ") <<
			matName << "Material_" << funcName2 << "(" << params << ");\n"
			"}\n";
}

static void AddMaterialSource(stringstream &source,
		const string &matName, const u_int i, const string &params) {
	AddMaterialSourceGlue(source, matName, i, "GetEventTypes", "GetEventTypes", "BSDFEvent",
			"__global const Material *material MATERIALS_PARAM_DECL", "");
	AddMaterialSourceGlue(source, matName, i, "IsDelta", "IsDelta", "bool",
			"__global const Material *material MATERIALS_PARAM_DECL", "");
	AddMaterialSourceGlue(source, matName, i, "Evaluate", "ConstEvaluate", "float3",
			"__global const Material *material, "
				"__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir, "
				"BSDFEvent *event, float *directPdfW "
				"MATERIALS_PARAM_DECL",
			"hitPoint, lightDir, eyeDir, event, directPdfW" +
				((params.length() > 0) ? (", " + params) : ""));
	AddMaterialSourceGlue(source, matName, i, "Sample", "ConstSample", "float3",
			"__global const Material *material, __global HitPoint *hitPoint, "
				"const float3 fixedDir, float3 *sampledDir, "
				"const float u0, const float u1,\n"
				"#if defined(PARAM_HAS_PASSTHROUGH)\n"
				"\tconst float passThroughEvent,\n"
				"#endif\n"
				"\tfloat *pdfW, float *cosSampledDir, BSDFEvent *event, "
				"const BSDFEvent requestedEvent "
				"MATERIALS_PARAM_DECL",
			"hitPoint, fixedDir, sampledDir, u0, u1,\n"
				"#if defined(PARAM_HAS_PASSTHROUGH)\n"
				"\t\tpassThroughEvent,\n"
				"#endif\n"
				"\t\tpdfW,  cosSampledDir, event, requestedEvent" +
				((params.length() > 0) ? (", " + params) : ""));

	source << "#if defined(PARAM_HAS_PASSTHROUGH)\n";
	AddMaterialSourceGlue(source, matName, i, "GetPassThroughTransparency", "GetPassThroughTransparency", "float3",
			"__global const Material *material, __global HitPoint *hitPoint, "
				"const float3 localFixedDir, const float passThroughEvent "
				"MATERIALS_PARAM_DECL",
			"material, hitPoint, localFixedDir, passThroughEvent TEXTURES_PARAM");
	source << "#endif\n";
}

static void AddMaterialSourceStandardImplGetEmittedRadiance(stringstream &source, const u_int i) {
	AddMaterialSourceGlue(source, "", i, "GetEmittedRadiance", "GetEmittedRadianceNoMix", "float3",
			"__global const Material *material, __global HitPoint *hitPoint, const float oneOverPrimitiveArea MATERIALS_PARAM_DECL",
			"material, hitPoint TEXTURES_PARAM");
}

static void AddMaterialSourceStandardImplGetvolume(stringstream &source, const u_int i) {
	source << "#if defined(PARAM_HAS_VOLUMES)\n";
	AddMaterialSourceGlue(source, "", i, "GetInteriorVolume", "GetInteriorVolumeNoMix", "uint",
			"__global const Material *material, __global HitPoint *hitPoint, "
				"const float passThroughEvent MATERIALS_PARAM_DECL",
			"material");
	AddMaterialSourceGlue(source, "", i, "GetExteriorVolume", "GetExteriorVolumeNoMix", "uint",
			"__global const Material *material, __global HitPoint *hitPoint, "
				"const float passThroughEvent MATERIALS_PARAM_DECL",
			"material");
	source << "#endif\n";
}

static void AddMaterialSourceSwitch(stringstream &source,
		const u_int count, const string &funcName, const string &calledFuncName,
		const string &returnType, const string &defaultReturnValue,
		const string &args,  const string &params, const bool hasReturn = true) {
	source << returnType << " Material_" << funcName << "(" << args << ") { \n"
			"\t__global const Material *mat = &mats[index];\n"
			"\tswitch (index) {\n";
	for (u_int i = 0; i < count; ++i) {
		source << "\t\tcase " << i << ":\n";
		source << "\t\t\t" <<
				(hasReturn ? "return " : "") <<
				"Material_Index" << i << "_" << calledFuncName << "(" << params << ");\n";
		if (!hasReturn)
			source << "\t\t\tbreak;\n";
	}

	if (hasReturn) {
		source << "\t\tdefault:\n"
				"\t\t\treturn " << defaultReturnValue<< ";\n";
	}

	source <<
			"\t}\n"
			"}\n";
}

static string ClothPreset2String(const slg::ocl::ClothPreset preset) {
	switch (preset) {
		case slg::ocl::DENIM:
			return "DENIM";
		case slg::ocl::SILKSHANTUNG:
			return "SILKSHANTUNG";
		case slg::ocl::SILKCHARMEUSE:
			return "SILKCHARMEUSE";
		case slg::ocl::COTTONTWILL:
			return "COTTONTWILL";
		case slg::ocl::WOOLGARBARDINE:
			return "WOOLGARBARDINE";
		case slg::ocl::POLYESTER:
			return "POLYESTER";
		default:
			throw runtime_error("Unknown slg::ocl::ClothPreset in ClothPreset2String(): " + ToString(preset));
	}
}

string CompiledScene::GetMaterialsEvaluationSourceCode() const {
	// Generate the source code for each material that reference other materials
	// and constant materials
	stringstream source;

	const u_int materialsCount = mats.size();
	for (u_int i = 0; i < materialsCount; ++i) {
		const slg::ocl::Material *mat = &mats[i];

		switch (mat->type) {
			case slg::ocl::MATTE: {
				AddMaterialSource(source, "Matte", i,
						AddTextureSourceCall(texs, "Spectrum", "material->matte.kdTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::ROUGHMATTE: {
				AddMaterialSource(source, "RoughMatte", i,
						AddTextureSourceCall(texs, "Float", "material->roughmatte.sigmaTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->roughmatte.kdTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::ARCHGLASS: {
				AddMaterialSource(source, "ArchGlass", i,
						AddTextureSourceCall(texs, "Spectrum", "material->archglass.ktTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->archglass.krTexIndex") + ", " +
						"ExtractExteriorIors(hitPoint, material->archglass.exteriorIorTexIndex TEXTURES_PARAM)" + ", " +
						"ExtractInteriorIors(hitPoint, material->archglass.interiorIorTexIndex TEXTURES_PARAM)");
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::CARPAINT: {
				AddMaterialSource(source, "CarPaint", i,
						AddTextureSourceCall(texs, "Spectrum", "material->carpaint.KaTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.depthTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->carpaint.KdTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->carpaint.Ks1TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.M1TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.R1TexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->carpaint.Ks2TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.M2TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.R2TexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->carpaint.Ks3TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.M3TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->carpaint.R3TexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::CLOTH: {
				AddMaterialSource(source, "Cloth", i,
						ClothPreset2String(mat->cloth.Preset) + ", " +
						"material->cloth.Repeat_U, " +
						"material->cloth.Repeat_V, " +
						"material->cloth.specularNormalization, " +
						AddTextureSourceCall(texs, "Spectrum", "material->cloth.Warp_KsIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->cloth.Weft_KsIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->cloth.Warp_KdIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->cloth.Weft_KdIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::GLASS: {
				AddMaterialSource(source, "Glass", i,
						AddTextureSourceCall(texs, "Spectrum", "material->glass.ktTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glass.krTexIndex") + ", " +
						"ExtractExteriorIors(hitPoint, material->glass.exteriorIorTexIndex TEXTURES_PARAM)" + ", " +
						"ExtractInteriorIors(hitPoint, material->glass.interiorIorTexIndex TEXTURES_PARAM)");
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::GLOSSY2: {
				AddMaterialSource(source, "Glossy2", i,
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSY2_INDEX)\n" +
						AddTextureSourceCall(texs, "Float", "material->glossy2.indexTexIndex") + ", " +
						"\n#endif\n" +
						AddTextureSourceCall(texs, "Float", "material->glossy2.nuTexIndex") + ", " +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSY2_ANISOTROPIC)\n" +
						AddTextureSourceCall(texs, "Float", "material->glossy2.nvTexIndex") + ", " +
						"\n#endif\n" +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSY2_ABSORPTION)\n" +
						AddTextureSourceCall(texs, "Spectrum", "material->glossy2.kaTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossy2.depthTexIndex") + ", " +
						"\n#endif\n" +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSY2_MULTIBOUNCE)\n" +
						"material->glossy2.multibounce, " +
						"\n#endif\n" +
						AddTextureSourceCall(texs, "Spectrum", "material->glossy2.kdTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glossy2.ksTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::MATTETRANSLUCENT: {
				AddMaterialSource(source, "MatteTranslucent", i,
						AddTextureSourceCall(texs, "Spectrum", "material->matteTranslucent.krTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->matteTranslucent.ktTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::ROUGHMATTETRANSLUCENT: {
				AddMaterialSource(source, "RoughMatteTranslucent", i,
						AddTextureSourceCall(texs, "Spectrum", "material->roughmatteTranslucent.krTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->roughmatteTranslucent.ktTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->roughmatteTranslucent.sigmaTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::METAL2: {
				string nTexString, kTexString;
				if (mat->metal2.fresnelTexIndex != NULL_INDEX) {
					// Get the fresnel texture used
					const slg::ocl::Texture &fresnelTex = texs[mat->metal2.fresnelTexIndex];

					switch (fresnelTex.type) {
						case slg::ocl::FRESNELCOLOR_TEX:
							nTexString = "FresnelApproxN3(" + AddTextureSourceCall(texs, "Spectrum", "texs[material->metal2.fresnelTexIndex].fresnelColor.krIndex") + ")";
							kTexString = "FresnelApproxK3(" + AddTextureSourceCall(texs, "Spectrum", "texs[material->metal2.fresnelTexIndex].fresnelColor.krIndex") + ")";
							break;
						case slg::ocl::FRESNELCONST_TEX:
							nTexString = ToOCLString(fresnelTex.fresnelConst.n);
							kTexString = ToOCLString(fresnelTex.fresnelConst.k);
							break;
						default:
							throw runtime_error("Unknown fresnel texture type in CompiledScene::GetMaterialsEvaluationSourceCode(): " + boost::lexical_cast<string>(fresnelTex.type));
							break;
					}
				} else {
					nTexString = AddTextureSourceCall(texs, "Spectrum", "material->metal2.nTexIndex");
					kTexString = AddTextureSourceCall(texs, "Spectrum", "material->metal2.kTexIndex");
				}
				AddMaterialSource(source, "Metal2", i,
						AddTextureSourceCall(texs, "Float", "material->metal2.nuTexIndex") + ", " +
						"\n#if defined(PARAM_ENABLE_MAT_METAL2_ANISOTROPIC)\n" +
						AddTextureSourceCall(texs, "Float", "material->metal2.nvTexIndex") + ", " +
						"\n#endif\n" +
						nTexString + ", " +
						kTexString);
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::MIRROR: {
				AddMaterialSource(source, "Mirror", i,
						AddTextureSourceCall(texs, "Spectrum", "material->mirror.krTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::NULLMAT: {
				AddMaterialSource(source, "Null", i, "");
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::ROUGHGLASS: {
				AddMaterialSource(source, "RoughGlass", i,
						AddTextureSourceCall(texs, "Spectrum", "material->roughglass.ktTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->roughglass.krTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->roughglass.nuTexIndex") + ", " +
						"\n#if defined(PARAM_ENABLE_MAT_ROUGHGLASS_ANISOTROPIC)\n" +
						AddTextureSourceCall(texs, "Float", "material->roughglass.nvTexIndex") + ", " +
						"\n#endif\n" +
						"ExtractExteriorIors(hitPoint, material->roughglass.exteriorIorTexIndex TEXTURES_PARAM)" + ", " +
						"ExtractInteriorIors(hitPoint, material->roughglass.interiorIorTexIndex TEXTURES_PARAM)");
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::VELVET: {
				AddMaterialSource(source, "Velvet", i,
						AddTextureSourceCall(texs, "Spectrum", "material->velvet.kdTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->velvet.p1TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->velvet.p2TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->velvet.p3TexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->velvet.thicknessTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::GLOSSYTRANSLUCENT: {
				AddMaterialSource(source, "GlossyTranslucent", i,
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_INDEX)\n" +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.indexTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.indexbfTexIndex") + ", " +
						"\n#endif\n" +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.nuTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.nubfTexIndex") + ", " +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ANISOTROPIC)\n" +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.nvTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.nvbfTexIndex") + ", " +
						"\n#endif\n" +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_ABSORPTION)\n" +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.kaTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.kabfTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.depthTexIndex") + ", " +
						AddTextureSourceCall(texs, "Float", "material->glossytranslucent.depthbfTexIndex") + ", " +
						"\n#endif\n" +
						"\n#if defined(PARAM_ENABLE_MAT_GLOSSYTRANSLUCENT_MULTIBOUNCE)\n" +
						"material->glossytranslucent.multibounce, " +
						"material->glossytranslucent.multibouncebf, " +
						"\n#endif\n" +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.kdTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.ktTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.ksTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->glossytranslucent.ksbfTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::CLEAR_VOL: {
				AddMaterialSource(source, "ClearVol", i, "");
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::HOMOGENEOUS_VOL: {
				AddMaterialSource(source, "HomogeneousVol", i,
						AddTextureSourceCall(texs, "Spectrum", "material->volume.homogenous.sigmaSTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->volume.homogenous.sigmaATexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->volume.homogenous.gTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::HETEROGENEOUS_VOL: {
				AddMaterialSource(source, "HeterogeneousVol", i,
						AddTextureSourceCall(texs, "Spectrum", "material->volume.heterogenous.sigmaSTexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->volume.heterogenous.sigmaATexIndex") + ", " +
						AddTextureSourceCall(texs, "Spectrum", "material->volume.heterogenous.gTexIndex"));
				AddMaterialSourceStandardImplGetEmittedRadiance(source, i);
				AddMaterialSourceStandardImplGetvolume(source, i);
				break;
			}
			case slg::ocl::MIX: {
				// MIX material uses a template .cl file
				string mixSrc = slg::ocl::KernelSource_materialdefs_template_mix;
				boost::replace_all(mixSrc, "<<CS_MIX_MATERIAL_INDEX>>", ToString(i));
				boost::replace_all(mixSrc, "<<CS_MAT_A_MATERIAL_INDEX>>", ToString(mat->mix.matAIndex));
				boost::replace_all(mixSrc, "<<CS_MAT_B_MATERIAL_INDEX>>", ToString(mat->mix.matBIndex));
				boost::replace_all(mixSrc, "<<CS_FACTOR_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->mix.mixFactorTexIndex"));
				source << mixSrc;
				break;
			}
			case slg::ocl::GLOSSYCOATING: {
				// GLOSSYCOATING material uses a template .cl file
				string glossycoatingSrc = slg::ocl::KernelSource_materialdefs_template_glossycoating;
				boost::replace_all(glossycoatingSrc, "<<CS_GLOSSYCOATING_MATERIAL_INDEX>>", ToString(i));
				boost::replace_all(glossycoatingSrc, "<<CS_MAT_BASE_MATERIAL_INDEX>>", ToString(mat->glossycoating.matBaseIndex));
				boost::replace_all(glossycoatingSrc, "<<CS_KS_TEXTURE>>", AddTextureSourceCall(texs, "Spectrum", "material->glossycoating.ksTexIndex"));
				boost::replace_all(glossycoatingSrc, "<<CS_NU_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.nuTexIndex"));
				boost::replace_all(glossycoatingSrc, "<<CS_NV_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.nvTexIndex"));
				boost::replace_all(glossycoatingSrc, "<<CS_KA_TEXTURE>>", AddTextureSourceCall(texs, "Spectrum", "material->glossycoating.kaTexIndex"));
				boost::replace_all(glossycoatingSrc, "<<CS_DEPTH_TEXTURE>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.depthTexIndex"));
				boost::replace_all(glossycoatingSrc, "<<CS_INDEX_TEXTURE_>>", AddTextureSourceCall(texs, "Float", "material->glossycoating.indexTexIndex"));
				if (mat->glossycoating.multibounce)
					boost::replace_all(glossycoatingSrc, "<<CS_MB_FLAG>>", "true");
				else
					boost::replace_all(glossycoatingSrc, "<<CS_MB_FLAG>>", "false");
				source << glossycoatingSrc;
				break;
			}
			default:
				throw runtime_error("Unknown material in CompiledScene::GetMaterialsEvaluationSourceCode(): " + boost::lexical_cast<string>(mat->type));
				break;
		}
	}

	// Generate the code for generic Material_GetEventTypes
	AddMaterialSourceSwitch(source, materialsCount, "GetEventTypes", "GetEventTypes", "BSDFEvent", "NONE",
			"const uint index MATERIALS_PARAM_DECL",
			"mat MATERIALS_PARAM");

	// Generate the code for generic Material_IsDelta()
	AddMaterialSourceSwitch(source, materialsCount, "IsDelta", "IsDelta", "bool", "true",
			"const uint index MATERIALS_PARAM_DECL",
			"mat MATERIALS_PARAM");

	// Generate the code for generic Material_GetEmittedRadianceWithMix()
	AddMaterialSourceSwitch(source, materialsCount, "GetEmittedRadianceWithMix", "GetEmittedRadiance", "float3", "BLACK",
			"const uint index, __global HitPoint *hitPoint, const float oneOverPrimitiveArea MATERIALS_PARAM_DECL",
			"mat, hitPoint, oneOverPrimitiveArea MATERIALS_PARAM");

	// Generate the code for generic Material_Evaluate()
	AddMaterialSourceSwitch(source, materialsCount, "Evaluate", "Evaluate", "float3", "BLACK",
			"const uint index, "
				"__global HitPoint *hitPoint, const float3 lightDir, const float3 eyeDir, "
				"BSDFEvent *event, float *directPdfW "
				"MATERIALS_PARAM_DECL",
			"mat, hitPoint, lightDir, eyeDir, event, directPdfW MATERIALS_PARAM");

	// Generate the code for generic Material_Sample()
	AddMaterialSourceSwitch(source, materialsCount, "Sample", "Sample", "float3", "BLACK",
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

	// Generate the code for generic Material_GetPassThroughTransparency()
	source << "#if defined(PARAM_HAS_PASSTHROUGH)\n";
	AddMaterialSourceSwitch(source, materialsCount, "GetPassThroughTransparency", "GetPassThroughTransparency", "float3", "BLACK",
			"const uint index, __global HitPoint *hitPoint, "
				"const float3 localFixedDir, const float oneOverPrimitiveArea MATERIALS_PARAM_DECL",
			"mat, hitPoint, localFixedDir, oneOverPrimitiveArea MATERIALS_PARAM");
	source << "#endif\n";

	// Generate the code for generic Material_GetInteriorVolumeWithMix() and Material_GetExteriorVolumeWithMix()
	source << "#if defined(PARAM_HAS_VOLUMES)\n";
	AddMaterialSourceSwitch(source, materialsCount, "GetInteriorVolumeWithMix", "GetInteriorVolume", "uint", "NULL_INDEX",
			"const uint index, __global HitPoint *hitPoint, const float passThroughEvent MATERIALS_PARAM_DECL",
			"mat, hitPoint, passThroughEvent MATERIALS_PARAM");
	AddMaterialSourceSwitch(source, materialsCount, "GetExteriorVolumeWithMix", "GetExteriorVolume", "uint", "NULL_INDEX",
			"const uint index, __global HitPoint *hitPoint, const float passThroughEvent MATERIALS_PARAM_DECL",
			"mat, hitPoint, passThroughEvent MATERIALS_PARAM");
	source << "#endif\n";

	// This glue is used to have fast path in case the first material is not MIX
	source <<
			"float3 Material_GetEmittedRadiance(const uint matIndex,\n"
			"		__global HitPoint *hitPoint, const float oneOverPrimitiveArea\n"
			"		MATERIALS_PARAM_DECL) {\n"
			"	__global const Material *material = &mats[matIndex];\n"
			"	float3 result;\n"
			"#if defined (PARAM_ENABLE_MAT_MIX)\n"
			"	if (material->type == MIX)\n"
			"		result = Material_GetEmittedRadianceWithMix(matIndex, hitPoint, oneOverPrimitiveArea\n"
			"				MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"#if defined (PARAM_ENABLE_MAT_MIX)\n"
			"	if (material->type == GLOSSYCOATING)\n"
			"		result = Material_GetEmittedRadianceWithMix(matIndex, hitPoint, oneOverPrimitiveArea\n"
			"				MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"		result = Material_GetEmittedRadianceNoMix(material, hitPoint\n"
			"				TEXTURES_PARAM);\n"
			"	return 	VLOAD3F(material->emittedFactor.c) * (material->usePrimitiveArea ? oneOverPrimitiveArea : 1.f) * result;\n"
			"}\n"
			"#if defined(PARAM_HAS_VOLUMES)\n"
			"uint Material_GetInteriorVolume(const uint matIndex,\n"
			"		__global HitPoint *hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"		, const float passThroughEvent\n"
			"#endif\n"
			"		MATERIALS_PARAM_DECL) {\n"
			"	__global const Material *material = &mats[matIndex];\n"
			"#if defined (PARAM_ENABLE_MAT_MIX)\n"
			"	if (material->type == MIX)\n"
			"		return Material_GetInteriorVolumeWithMix(matIndex, hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"			, passThroughEvent\n"
			"#endif\n"
			"			MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"#if defined (PARAM_ENABLE_MAT_GLOSSYCOATING)\n"
			"	if (material->type == GLOSSYCOATING)\n"
			"		return Material_GetInteriorVolumeWithMix(matIndex, hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"			, passThroughEvent\n"
			"#endif\n"
			"			MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"		return Material_GetInteriorVolumeNoMix(material);\n"
			"}\n"
			"uint Material_GetExteriorVolume(const uint matIndex,\n"
			"		__global HitPoint *hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"		, const float passThroughEvent\n"
			"#endif\n"
			"		MATERIALS_PARAM_DECL) {\n"
			"	__global const Material *material = &mats[matIndex];\n"
			"#if defined (PARAM_ENABLE_MAT_MIX)\n"
			"	if (material->type == MIX)\n"
			"		return Material_GetExteriorVolumeWithMix(matIndex, hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"			, passThroughEvent\n"
			"#endif\n"
			"			MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"#if defined (PARAM_ENABLE_MAT_GLOSSYCOATING)\n"
			"	if (material->type == GLOSSYCOATING)\n"
			"		return Material_GetExteriorVolumeWithMix(matIndex, hitPoint\n"
			"#if defined(PARAM_HAS_PASSTHROUGH)\n"
			"			, passThroughEvent\n"
			"#endif\n"
			"			MATERIALS_PARAM);\n"
			"	else\n"
			"#endif\n"
			"		return Material_GetExteriorVolumeNoMix(material);\n"
			"}\n"
			"#endif\n";

	return source.str();
}

#endif
