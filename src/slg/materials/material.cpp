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

#include "luxrays/core/geometry/frame.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/materials/material.h"
#include "slg/bsdf/bsdf.h"
#include "slg/textures/fresnel/fresneltexture.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Material
//------------------------------------------------------------------------------

Material::Material(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump) :
		NamedObject("material"),
		matID(0), lightID(0),
		directLightSamplingType(DLS_AUTO), emittedImportance(1.f),
		emittedGain(1.f), emittedPower(0.f), emittedEfficiency(0.f),
		emittedPowerNormalize(true), emittedGainNormalize(false),
		emittedTemperature(-1.f), emittedNormalizeTemperature(false),
		frontTransparencyTex(frontTransp), backTransparencyTex(backTransp),
		emittedTex(emitted), bumpTex(bump), bumpSampleDistance(.001f),
		emissionMap(nullptr), emissionFunc(nullptr),
		interiorVolume(nullptr), exteriorVolume(nullptr),
		glossiness(0.f),
		isVisibleIndirectDiffuse(true), isVisibleIndirectGlossy(true), isVisibleIndirectSpecular(true),
		isShadowCatcher(false), isShadowCatcherOnlyInfiniteLights(false), isPhotonGIEnabled(true),
		isHoldout(false) {
	SetEmittedTheta(90.f);
	UpdateEmittedFactor();
	UpdateAvgPassThroughTransparency();
}

Material::~Material() {
	delete emissionFunc;
}

void Material::SetEmittedTheta(const float theta) {
	if (theta <= 0.f) {
		emittedTheta = 0.f;
		emittedCosThetaMax = 1.f;
	} else if (theta <= 90.f) {
		emittedTheta = theta;

		const float radTheta = Radians(emittedTheta);
		emittedCosThetaMax = cosf(radTheta);
	} else {
		emittedTheta = 90.f;
		emittedCosThetaMax = 0.f;
	}
}

void Material::SetEmissionMap(const ImageMap *map) {
	emissionMap = map;
	delete emissionFunc;
	if (emissionMap)
		emissionFunc = new SampleableSphericalFunction(new ImageMapSphericalFunction(emissionMap));
	else
		emissionFunc = nullptr;
}

Spectrum Material::GetPassThroughTransparency(const HitPoint &hitPoint,
		const luxrays::Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const {
	const Texture *transparencyTex = (hitPoint.intoObject != backTracing) ? frontTransparencyTex : backTransparencyTex;
	
	if (transparencyTex) {
		const float weight = Clamp(transparencyTex->GetFloatValue(hitPoint), 0.f, 1.f);

		return (passThroughEvent > weight) ? Spectrum(1.f) : Spectrum(0.f);
	} else
		return Spectrum(0.f);
}

Spectrum Material::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	if (emittedTex) {
		return (emittedFactor * (usePrimitiveArea ? oneOverPrimitiveArea : 1.f)) *
				emittedTex->GetSpectrumValue(hitPoint).Clamp();
	} else
		return Spectrum();
}

float Material::GetEmittedRadianceY(const float oneOverPrimitiveArea) const {
	if (emittedTex)
		return emittedFactor.Y() * (usePrimitiveArea ? oneOverPrimitiveArea : 1.f) * Max(emittedTex->Y(), 0.f);
	else
		return 0.f;
}

void Material::Bump(HitPoint *hitPoint) const {
    if (bumpTex) {
		hitPoint->shadeN = bumpTex->Bump(*hitPoint, bumpSampleDistance);

		// Update dpdu and dpdv so they are still orthogonal to shadeN 
		hitPoint->dpdu = Cross(hitPoint->shadeN, Cross(hitPoint->dpdu, hitPoint->shadeN));
		hitPoint->dpdv = Cross(hitPoint->shadeN, Cross(hitPoint->dpdv, hitPoint->shadeN));
	}
}

Spectrum Material::Albedo(const HitPoint &hitPoint) const {
	return Spectrum(1.f);
}

Spectrum Material::EvaluateTotal(const HitPoint &hitPoint) const {
	// This is the generic implementation using Monte Carlo integration to
	// compute the result.
	//
	// Note: Matte material has a special fast path.

	assert (!hitPoint.fromLight);

	// Note: may be, this should become a configurable parameter
	const u_int samplesCount = 64;

	Spectrum result;
	for (u_int i = 0; i < samplesCount; ++i) {
		const float u1 = RadicalInverse(i, 3);
		const float u2 = RadicalInverse(i, 5);
		const float u3 = RadicalInverse(i, 7);
		
		const Vector fixedDir = UniformSampleHemisphere(u1, u2);
		const float fixedDirPdf = UniformHemispherePdf(u1, u2);

		BSDFEvent event;
		Vector sampledDir;
		float sampledDirPdf;
		Spectrum bsdfSample = Sample(hitPoint, fixedDir, &sampledDir,
					u1, u2,	u3, &sampledDirPdf, &event);

		if (!bsdfSample.Black() && (CosTheta(sampledDir) > 0.f))
				result += bsdfSample * CosTheta(fixedDir) / fixedDirPdf;
    }

    return result / (M_PI * samplesCount);
}

void Material::UpdateEmittedFactor() {
	const Spectrum emittedTemperatureScale = (emittedTemperature >= 0.f) ? TemperatureToWhitePoint(emittedTemperature, emittedNormalizeTemperature) : Spectrum(1.f);

	if (emittedTex) {
		const float normalizePowerFactor = emittedPowerNormalize ? (1.f / Max(emittedTex->Y(), 0.f)) : 1.f;

		emittedFactor = emittedGain * (emittedPower * emittedEfficiency  * normalizePowerFactor);
		if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN()) {
			const float normalizeGainFactor = emittedGainNormalize ? (1.f / Max(emittedTex->Y(), 0.f)) : 1.f;

			emittedFactor = emittedGain * normalizeGainFactor;
			usePrimitiveArea = false;
		} else {
			if (emittedTheta == 0.f) {
				// Nothing to do
			} else if (emittedTheta < 90.f)
				emittedFactor /= (1.f - emittedCosThetaMax) * (2.f * M_PI);
			else
				emittedFactor /= M_PI;
			usePrimitiveArea = true;
		}
	} else {
		const float normalizeGainFactor = emittedGainNormalize ? (1.f / Max(emittedTex->Y(), 0.f)) : 1.f;

		emittedFactor = emittedGain * normalizeGainFactor;
		usePrimitiveArea = false;
	}
	
	emittedFactor *= emittedTemperatureScale;
}

void Material::UpdateAvgPassThroughTransparency() {
	avgPassThroughTransparency = frontTransparencyTex ? Clamp(frontTransparencyTex->Filter(), 0.f, 1.f) : 1.f;
}

Properties Material::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	luxrays::Properties props;

	const string name = GetName();
	if (frontTransparencyTex)
		props.Set(Property("scene.materials." + name + ".transparency.front")(frontTransparencyTex->GetSDLValue()));
	if (backTransparencyTex)
		props.Set(Property("scene.materials." + name + ".transparency.back")(backTransparencyTex->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".transparency.shadow")(passThroughShadowTransparency));
	props.Set(Property("scene.materials." + name + ".id")(matID));

	props.Set(Property("scene.materials." + name + ".emission.gain")(emittedGain));
	props.Set(Property("scene.materials." + name + ".emission.power")(emittedPower));
	props.Set(Property("scene.materials." + name + ".emission.normalizebycolor")(emittedPowerNormalize));
	props.Set(Property("scene.materials." + name + ".emission.efficiency")(emittedEfficiency));
	props.Set(Property("scene.materials." + name + ".emission.theta")(emittedTheta));
	props.Set(Property("scene.materials." + name + ".emission.id")(lightID));
	props.Set(Property("scene.materials." + name + ".emission.importance")(emittedImportance));
	if (emittedTex)
		props.Set(Property("scene.materials." + name + ".emission")(emittedTex->GetSDLValue()));
	if (emissionMap) {
		const string fileName = useRealFileName ?
			emissionMap->GetName() : imgMapCache.GetSequenceFileName(emissionMap);
		props.Set(Property("scene.materials." + name + ".emission.mapfile")(fileName));
		props.Set(emissionMap->ToProperties("scene.materials." + name, false));
	}
	props.Set(Property("scene.materials." + name + ".emission.temperature")(emittedTemperature));
	props.Set(Property("scene.materials." + name + ".emission.temperature.normalize")(emittedNormalizeTemperature));

	switch (directLightSamplingType) {
		case DLS_ENABLED:
			props.Set(Property("scene.materials." + name + ".emission.directlightsampling.type")("ENABLED"));
			break;
		case DLS_DISABLED:
			props.Set(Property("scene.materials." + name + ".emission.directlightsampling.type")("DISABLED"));
			break;
		case DLS_AUTO:
			props.Set(Property("scene.materials." + name + ".emission.directlightsampling.type")("AUTO"));
			break;
		default:
			throw runtime_error("Unknown MaterialEmissionDLSType in Material::ToProperties(): " + ToString(directLightSamplingType));
	}

	if (bumpTex)
		props.Set(Property("scene.materials." + name + ".bumptex")(bumpTex->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".bumpsamplingdistance")(bumpSampleDistance));

	if (interiorVolume)
		props.Set(Property("scene.materials." + name + ".volume.interior")(interiorVolume->GetName()));
	if (exteriorVolume)
		props.Set(Property("scene.materials." + name + ".volume.exterior")(exteriorVolume->GetName()));
		
	props.Set(Property("scene.materials." + name + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	props.Set(Property("scene.materials." + name + ".shadowcatcher.enable")(isShadowCatcher));
	props.Set(Property("scene.materials." + name + ".shadowcatcher.onlyinfinitelights")(isShadowCatcherOnlyInfiniteLights));
	
	props.Set(Property("scene.materials." + name + ".photongi.enable")(isPhotonGIEnabled));
	props.Set(Property("scene.materials." + name + ".holdout.enable")(isHoldout));

	return props;
}

void Material::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (oldMat == interiorVolume)
		interiorVolume = (const Volume *)newMat;
	if (oldMat == exteriorVolume)
		exteriorVolume = (const Volume *)newMat;
}

void Material::AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
	referencedMats.insert(this);
	if (interiorVolume)
		referencedMats.insert((const Material *)interiorVolume);
	if (exteriorVolume)
		referencedMats.insert((const Material *)exteriorVolume);
}

void Material::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	if (frontTransparencyTex)
		frontTransparencyTex->AddReferencedTextures(referencedTexs);
	if (backTransparencyTex)
		backTransparencyTex->AddReferencedTextures(referencedTexs);
	if (emittedTex)
		emittedTex->AddReferencedTextures(referencedTexs);
	if (bumpTex)
		bumpTex->AddReferencedTextures(referencedTexs);
}

void Material::AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	if (emissionMap)
		referencedImgMaps.insert(emissionMap);
}

// Update any reference to oldTex with newTex
void Material::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	if (frontTransparencyTex == oldTex) {
		frontTransparencyTex = newTex;
		UpdateAvgPassThroughTransparency();
	}
	if (backTransparencyTex == oldTex)
		backTransparencyTex = newTex;
	if (emittedTex == oldTex)
		emittedTex = newTex;
	if (bumpTex == oldTex)
		bumpTex = newTex;
}

string Material::MaterialType2String(const MaterialType type) {
	switch (type) {
		case MATTE: return "MATTE";
		case MIRROR: return "MIRROR";
		case GLASS: return "GLASS";
		case ARCHGLASS: return "ARCHGLASS";
		case MIX: return "MIX";
		case NULLMAT: return "NULLMAT";
		case MATTETRANSLUCENT: return "MATTETRANSLUCENT";
		case GLOSSY2: return "GLOSSY2";
		case METAL2: return "METAL2";
		case ROUGHGLASS: return "ROUGHGLASS";
		case VELVET: return "VELVET";
		case CLOTH: return "CLOTH";
		case CARPAINT: return "CARPAINT";
		case ROUGHMATTE: return "ROUGHMATTE";
		case ROUGHMATTETRANSLUCENT: return "ROUGHMATTETRANSLUCENT";
		case GLOSSYTRANSLUCENT: return "GLOSSYTRANSLUCENT";
		case GLOSSYCOATING: return "GLOSSYCOATING";
		case DISNEY: return "DISNEY";
		case TWOSIDED: return "TWOSIDED";

		// Volumes
		case HOMOGENEOUS_VOL: return "HOMOGENEOUS_VOL";
		case CLEAR_VOL: return "CLEAR_VOL";
		case HETEROGENEOUS_VOL: return "HETEROGENEOUS_VOL";

		default:
			throw runtime_error("Unknown material type in Material::MaterialType2String(): " + ToString(type));
	}
}

float Material::ComputeGlossiness(const Texture *t1, const Texture *t2, const Texture *t3) {
	const float glossinessT1 = t1 ? t1->Filter() : 1.f;
	const float glossinessT2 = t2 ? t2->Filter() : 1.f;
	const float glossinessT3 = t3 ? t3->Filter() : 1.f;

	return Min(glossinessT1, Min(glossinessT2, glossinessT3));
}

//------------------------------------------------------------------------------
// IOR utilities
//------------------------------------------------------------------------------

float slg::ExtractExteriorIors(const HitPoint &hitPoint, const Texture *exteriorIor) {
	float nc = 1.f;
	if (exteriorIor)
		nc = exteriorIor->GetFloatValue(hitPoint);
	else if (hitPoint.exteriorVolume)
		nc = hitPoint.exteriorVolume->GetIOR(hitPoint);

	return nc;
}

float slg::ExtractInteriorIors(const HitPoint &hitPoint, const Texture *interiorIor) {
	float nt = 1.f;
	if (interiorIor)
		nt = interiorIor->GetFloatValue(hitPoint);
	else if (hitPoint.interiorVolume)
		nt = hitPoint.interiorVolume->GetIOR(hitPoint);

	return nt;
}

//------------------------------------------------------------------------------
// Coating absorption
//------------------------------------------------------------------------------

Spectrum slg::CoatingAbsorption(const float cosi, const float coso,
	const Spectrum &alpha, const float depth) {
	if (depth > 0.f) {
		// 1/cosi+1/coso=(cosi+coso)/(cosi*coso)
		const float depthFactor = depth * (cosi + coso) / (cosi * coso);
		return Exp(alpha * -depthFactor);
	} else
		return Spectrum(1.f);
}

//------------------------------------------------------------------------------
// SchlickDistribution
//------------------------------------------------------------------------------

float slg::SchlickDistribution_SchlickZ(const float roughness, const float cosNH) {
	if (roughness > 0.f) {
		const float cosNH2 = cosNH * cosNH;
		// expanded for increased numerical stability
		const float d = cosNH2 * roughness + (1.f - cosNH2);
		// use double division to avoid overflow in d*d product
		return (roughness / d) / d;
	}
	return 0.f;
}

float slg::SchlickDistribution_SchlickA(const Vector &H, const float anisotropy) {
	const float h = sqrtf(H.x * H.x + H.y * H.y);
	if (h > 0.f) {
		const float w = (anisotropy > 0.f ? H.x : H.y) / h;
		const float p = 1.f - fabsf(anisotropy);
		return sqrtf(p / (p * p + w * w * (1.f - p * p)));
	}

	return 1.f;
}

float slg::SchlickDistribution_D(const float roughness, const Vector &wh,
		const float anisotropy) {
	const float cosTheta = fabsf(wh.z);
	return SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(wh, anisotropy) * INV_PI;
}

float slg::SchlickDistribution_SchlickG(const float roughness,
		const float costheta) {
	return costheta / (costheta * (1.f - roughness) + roughness);
}

float slg::SchlickDistribution_G(const float roughness, const Vector &localFixedDir,
	const Vector &localSampledDir) {
	return SchlickDistribution_SchlickG(roughness, fabsf(localFixedDir.z)) *
			SchlickDistribution_SchlickG(roughness, fabsf(localSampledDir.z));
}

static float GetPhi(const float a, const float b) {
	return M_PI * .5f * sqrtf(a * b / (1.f - a * (1.f - b)));
}

void slg::SchlickDistribution_SampleH(const float roughness, const float anisotropy,
		const float u0, const float u1, Vector *wh, float *d, float *pdf) {
	float u1x4 = u1 * 4.f;
	const float cos2Theta = u0 / (roughness * (1 - u0) + u0);
	const float cosTheta = sqrtf(cos2Theta);
	const float sinTheta = sqrtf(1.f - cos2Theta);
	const float p = 1.f - fabsf(anisotropy);
	float phi;
	if (u1x4 < 1.f) {
		phi = GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 2.f) {
		u1x4 = 2.f - u1x4;
		phi = M_PI - GetPhi(u1x4 * u1x4, p * p);
	} else if (u1x4 < 3.f) {
		u1x4 -= 2.f;
		phi = M_PI + GetPhi(u1x4 * u1x4, p * p);
	} else {
		u1x4 = 4.f - u1x4;
		phi = M_PI * 2.f - GetPhi(u1x4 * u1x4, p * p);
	}

	if (anisotropy > 0.f)
		phi += M_PI * .5f;

	*wh = Vector(sinTheta * cosf(phi), sinTheta * sinf(phi), cosTheta);
	*d = SchlickDistribution_SchlickZ(roughness, cosTheta) * SchlickDistribution_SchlickA(*wh, anisotropy) * INV_PI;
	*pdf = *d;
}

float slg::SchlickDistribution_Pdf(const float roughness, const Vector &wh,
		const float anisotropy) {
	return SchlickDistribution_D(roughness, wh, anisotropy);
}

//------------------------------------------------------------------------------
// Schlick BSDF
//------------------------------------------------------------------------------

float slg::SchlickBSDF_CoatingWeight(const Spectrum &ks, const Vector &localFixedDir) {
	// Approximate H by using reflection direction for wi
	const float u = fabsf(localFixedDir.z);
	const Spectrum S = FresnelTexture::SchlickEvaluate(ks, u);

	// Ensures coating is never sampled less than half the time
	return .5f * (1.f + S.Filter());
}

Spectrum slg::SchlickBSDF_CoatingF(const bool fromLight, const Spectrum &ks, const float roughness,
		const float anisotropy, const bool mbounce, const Vector &localFixedDir, const Vector &localSampledDir) {
	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir.z);

	const Vector wh(Normalize(localFixedDir + localSampledDir));
	const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localSampledDir, wh));
	assert (S.IsValid());

	const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);

	// Multibounce - alternative with interreflection in the coating creases
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	if (!fromLight)
		factor = factor / (4.f * coso) +
				(mbounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	else
		factor = factor / (4.f * cosi) + 
				(mbounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);
	assert (IsValid(factor));

	return factor * S;
}

Spectrum slg::SchlickBSDF_CoatingSampleF(const bool fromLight, const Spectrum ks,
		const float roughness, const float anisotropy, const bool mbounce,
		const Vector &localFixedDir, Vector *localSampledDir, float u0, float u1, float *pdf) {
	Vector wh;
	float d, specPdf;
	SchlickDistribution_SampleH(roughness, anisotropy, u0, u1, &wh, &d, &specPdf);
	const float cosWH = Dot(localFixedDir, wh);
	*localSampledDir = 2.f * cosWH * wh - localFixedDir;

	if ((fabsf(localSampledDir->z) < DEFAULT_COS_EPSILON_STATIC) || (localFixedDir.z * localSampledDir->z < 0.f))
		return Spectrum();

	const float coso = fabsf(localFixedDir.z);
	const float cosi = fabsf(localSampledDir->z);

	*pdf = specPdf / (4.f * cosWH);
	if (*pdf <= 0.f)
		return Spectrum();

	Spectrum S = FresnelTexture::SchlickEvaluate(ks, fabsf(cosWH));

	const float G = SchlickDistribution_G(roughness, localFixedDir, *localSampledDir);
	if (!fromLight)
		//CoatingF(sw, *wi, wo, f_);
		S *= (d / *pdf) * G / (4.f * coso) + 
				(mbounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) / *pdf : 0.f);
	else
		//CoatingF(sw, wo, *wi, f_);
		S *= (d / *pdf) * G / (4.f * cosi) + 
				(mbounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) / *pdf : 0.f);

	return S;
}

float slg::SchlickBSDF_CoatingPdf(const float roughness, const float anisotropy,
		const Vector &localFixedDir, const Vector &localSampledDir) {
	const Vector wh(Normalize(localFixedDir + localSampledDir));
	return SchlickDistribution_Pdf(roughness, wh, anisotropy) / (4.f * AbsDot(localFixedDir, wh));
}
