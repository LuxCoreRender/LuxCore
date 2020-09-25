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

#include "slg/textures/fresnel/fresneltexture.h"
#include "slg/materials/glossycoating.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// GlossyCoating material
//------------------------------------------------------------------------------

GlossyCoatingMaterial::GlossyCoatingMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Material *mB, const Texture *ks, const Texture *u, const Texture *v,
		const Texture *ka, const Texture *d, const Texture *i, const bool mbounce) :
			Material(frontTransp, backTransp, emitted, bump), matBase(mB), Ks(ks), nu(u), nv(v),
			Ka(ka), depth(d), index(i), multibounce(mbounce) {
	glossiness = Min(ComputeGlossiness(nu, nv), matBase->GetGlossiness());
}

const Volume *GlossyCoatingMaterial::GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const {
	if (interiorVolume)
		return interiorVolume;
	else
		return matBase->GetInteriorVolume(hitPoint, passThroughEvent);
}

const Volume *GlossyCoatingMaterial::GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const {
	if (exteriorVolume)
		return exteriorVolume;
	else
		return matBase->GetExteriorVolume(hitPoint, passThroughEvent);
}

void GlossyCoatingMaterial::UpdateAvgPassThroughTransparency() {
	avgPassThroughTransparency = matBase->GetAvgPassThroughTransparency();
}

Spectrum GlossyCoatingMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const {
	return matBase->GetPassThroughTransparency(hitPoint, localFixedDir,
			passThroughEvent, backTracing);
}

float GlossyCoatingMaterial::GetEmittedRadianceY(const float oneOverPrimitiveArea) const {
	if (emittedTex)
		return Material::GetEmittedRadianceY(oneOverPrimitiveArea);
	else
		return matBase->GetEmittedRadianceY(oneOverPrimitiveArea);
}

Spectrum GlossyCoatingMaterial::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	if (emittedTex)
		return Material::GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);
	else
		return matBase->GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);
}

Spectrum GlossyCoatingMaterial::Albedo(const HitPoint &hitPoint) const {
	return matBase->Albedo(hitPoint);
}

Spectrum GlossyCoatingMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Frame frame(hitPoint.GetFrame());
	const float cosi = fabsf(localLightDir.z);
	const float coso = fabsf(localEyeDir.z);

	const float sideTest = Dot(frame.ToWorld(localEyeDir), hitPoint.geometryN) * Dot(frame.ToWorld(localLightDir), hitPoint.geometryN);
	if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection

		HitPoint hitPointBase(hitPoint);
		matBase->Bump(&hitPointBase);
		const Frame frameBase(hitPointBase.GetFrame());
		const Vector lightDirBase = frameBase.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirBase = frameBase.ToLocal(frame.ToWorld(localEyeDir));

		const Spectrum baseF = matBase->Evaluate(hitPointBase, lightDirBase, eyeDirBase, event, directPdfW, reversePdfW);
		if (!(localLightDir.z > 0.f)) {
			// Back face: no coating
			return baseF;
		}

		// I have always to initialized baseF pdf because it is used below
		if (baseF.Black()) {
			if (directPdfW)
				*directPdfW = 0.f;
			if (reversePdfW)
				*reversePdfW = 0.f;
			
			*event = NONE;
		}

		// Front face: coating+base
		*event |= GLOSSY | REFLECT;
		Spectrum ks = Ks->GetSpectrumValue(hitPoint);
		const float i = index->GetFloatValue(hitPoint);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);

		const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
		const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		if (directPdfW) {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, hitPoint.fromLight ? localLightDir : localEyeDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = wBase * *directPdfW +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localLightDir, localEyeDir);
		}

		if (reversePdfW) {
			const float wCoatingR = SchlickBSDF_CoatingWeight(ks, hitPoint.fromLight ? localEyeDir : localLightDir);
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW = wBaseR * *reversePdfW +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localEyeDir, localLightDir);
		}

		// Absorption
		const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
		const float d = depth->GetFloatValue(hitPoint);
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const Vector H(Normalize(localLightDir + localEyeDir));
		const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localEyeDir, H));

		const Spectrum coatingF = SchlickBSDF_CoatingF(true, ks, roughness, anisotropy, multibounce,
			localLightDir, localEyeDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		return coatingF + absorption * (Spectrum(1.f) - S) * baseF;
	} else if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission

		HitPoint hitPointBase(hitPoint);
		matBase->Bump(&hitPointBase);
		const Frame frameBase(hitPointBase.GetFrame());
		const Vector lightDirBase = frameBase.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirBase = frameBase.ToLocal(frame.ToWorld(localEyeDir));

		const Spectrum baseF = matBase->Evaluate(hitPointBase, lightDirBase, eyeDirBase, event, directPdfW, reversePdfW);
		// I have always to initialized baseF pdf because it is used below
		if (baseF.Black()) {
			if (directPdfW)
				*directPdfW = 0.f;
			if (reversePdfW)
				*reversePdfW = 0.f;

			*event = NONE;
		}

		*event |= GLOSSY | TRANSMIT;

		Spectrum ks = Ks->GetSpectrumValue(hitPoint);
		const float i = index->GetFloatValue(hitPoint);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);

		if (directPdfW) {
			const Vector localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
			const float wCoating = (localFixedDir.z > DEFAULT_COS_EPSILON_STATIC) ? SchlickBSDF_CoatingWeight(ks, localFixedDir) : 0.f;
			const float wBase = 1.f - wCoating;

			*directPdfW *= wBase;
		}

		if (reversePdfW) {
			const Vector localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;
			const float wCoatingR = (localSampledDir.z > DEFAULT_COS_EPSILON_STATIC) ? SchlickBSDF_CoatingWeight(ks, localSampledDir) : 0.f;
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW *= wBaseR;
		}

		// Absorption
		const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
		const float d = depth->GetFloatValue(hitPoint);
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const Vector H(Normalize(Vector(localLightDir.x + localEyeDir.x, localLightDir.y + localEyeDir.y,
			localLightDir.z - localEyeDir.z)));
		const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localEyeDir, H));

		// Filter base layer, the square root is just a heuristic
		// so that a sheet coated on both faces gets a filtering factor
		// of 1-S like a reflection
		return absorption * Sqrt(Spectrum(1.f) - S) * baseF;
	} else
		return Spectrum();
}

Spectrum GlossyCoatingMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Frame frame(hitPoint.GetFrame());
	Spectrum ks = Ks->GetSpectrumValue(hitPoint);
	const float i = index->GetFloatValue(hitPoint);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = ks.Clamp(0.f, 1.f);

	const float wCoating = (localFixedDir.z > DEFAULT_COS_EPSILON_STATIC) ? SchlickBSDF_CoatingWeight(ks, localFixedDir) : 0.f;
	const float wBase = 1.f - wCoating;

	const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	float basePdf, coatingPdf;
	Spectrum baseF, coatingF;

	if (passThroughEvent < wBase) {
		HitPoint hitPointBase(hitPoint);
		matBase->Bump(&hitPointBase);
		const Frame frameBase(hitPointBase.GetFrame());
		const Vector fixedDirBase = frameBase.ToLocal(frame.ToWorld(localFixedDir));
		// Sample base layer
		baseF = matBase->Sample(hitPointBase, fixedDirBase, localSampledDir, u0, u1, passThroughEvent / wBase,
			&basePdf, event);
		assert (baseF.IsValid());

		if (baseF.Black())
			return Spectrum();

		baseF *= basePdf;
		*localSampledDir = frame.ToLocal(frameBase.ToWorld(*localSampledDir));

		// Don't add the coating scattering if the base sampled
		// component is specular	
		if (!(*event & SPECULAR)) {
			coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, multibounce,
				localFixedDir, *localSampledDir);
			assert (coatingF.IsValid());

			coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, *localSampledDir);
			assert (IsValid(coatingPdf));
		} else
			coatingPdf = 0.f;
	} else {
		// Sample coating BxDF
		coatingF = SchlickBSDF_CoatingSampleF(hitPoint.fromLight, ks, roughness, anisotropy, multibounce,
			localFixedDir, localSampledDir, u0, u1, &coatingPdf);
		assert (coatingF.IsValid());

		if (coatingF.Black())
			return Spectrum();
		assert (IsValid(coatingPdf));

		if (fabsf(CosTheta(*localSampledDir)) < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		coatingF *= coatingPdf;

		HitPoint hitPointBase(hitPoint);
		matBase->Bump(&hitPointBase);
		const Frame frameBase(hitPointBase.GetFrame());
		const Vector localLightDir = frameBase.ToLocal(frame.ToWorld(hitPoint.fromLight ? localFixedDir : *localSampledDir));
		const Vector localEyeDir = frameBase.ToLocal(frame.ToWorld(hitPoint.fromLight ? *localSampledDir : localFixedDir));

		baseF = matBase->Evaluate(hitPointBase, localLightDir, localEyeDir, event, &basePdf);
		// I have always to initialized baseF pdf because it is used below
		if (baseF.Black())
			basePdf = 0.f;
		*event = GLOSSY | REFLECT;
	}

	// Absorption
	const float cosi = fabsf(localFixedDir.z);
	const float coso = fabsf(localSampledDir->z);
	const Spectrum alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const float d = depth->GetFloatValue(hitPoint);
	const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

	// If Dot(woW, ng) is too small, set sideTest to 0 to discard the result
	// and avoid numerical instability
	const float cosWo = Dot(frame.ToWorld(localFixedDir), hitPoint.geometryN);
	const float sideTest = (fabsf(cosWo) < DEFAULT_EPSILON_STATIC) ? 0.f : Dot(frame.ToWorld(*localSampledDir), hitPoint.geometryN) / cosWo;
	Spectrum result;
	if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection
		if (!(cosWo > 0.f)) {
			// Back face reflection: no coating
			result = baseF;
			assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());
		} else {
			// Front face reflection: coating+base

			// Coating fresnel factor
			const Vector H(Normalize(localFixedDir + *localSampledDir));
			const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localFixedDir, H));

			// blend in base layer Schlick style
			// coatingF already takes fresnel factor S into account
			result = coatingF + absorption * (Spectrum(1.f) - S) * baseF;
			assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());
		}
	} else if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission
		// Coating fresnel factor
		Vector H(localFixedDir.x + localSampledDir->x, localFixedDir.y + localSampledDir->y,
				localFixedDir.z - localSampledDir->z);
		const float HLength = H.Length();

		Spectrum S;
		// I have to handle the case when HLength is 0.0 (or nearly 0.f) in
		// order to avoid NaN
		if (HLength < DEFAULT_EPSILON_STATIC)
			S = 0.f;
		else {
			// Normalize
			H /= HLength;
			S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localFixedDir, H));
		}

		// Filter base layer, the square root is just a heuristic
		// so that a sheet coated on both faces gets a filtering factor
		// of 1-S like a reflection
		result = absorption * Sqrt(Spectrum(1.f) - S) * baseF;
		assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());
	} else
		return Spectrum();

	*pdfW = coatingPdf * wCoating + basePdf * wBase;

	result /= *pdfW;
	assert (!result.IsNaN() && !result.IsInf() && !result.IsNeg());
	
	return result;
}

void GlossyCoatingMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Frame frame(hitPoint.GetFrame());
	HitPoint hitPointBase(hitPoint);
	matBase->Bump(&hitPointBase);
	const Frame frameBase(hitPointBase.GetFrame());
	const Vector lightDirBase = frameBase.ToLocal(frame.ToWorld(localLightDir));
	const Vector eyeDirBase = frameBase.ToLocal(frame.ToWorld(localEyeDir));
	matBase->Pdf(hitPointBase, lightDirBase, eyeDirBase, directPdfW, reversePdfW);
	const Vector localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	if (!(localFixedDir.z > 0.f))
		return;

	Spectrum ks = Ks->GetSpectrumValue(hitPoint);
	const float i = index->GetFloatValue(hitPoint);
	if (i > 0.f) {
		const float ti = (i - 1.f) / (i + 1.f);
		ks *= ti * ti;
	}
	ks = ks.Clamp(0.f, 1.f);

	const float u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
	const float u2 = u * u;
	const float v2 = v * v;
	const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
	const float roughness = u * v;

	if (directPdfW) {
		const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
		const float wBase = 1.f - wCoating;

		*directPdfW = wBase * *directPdfW +
			wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localLightDir, localEyeDir);
	}

	if (reversePdfW) {
		const float wCoatingR = SchlickBSDF_CoatingWeight(ks, hitPoint.fromLight ? localEyeDir : localLightDir);
		const float wBaseR = 1.f - wCoatingR;

		*reversePdfW = wBaseR * *reversePdfW +
			wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localEyeDir, localLightDir);
	}
}

void GlossyCoatingMaterial::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (matBase == oldMat)
		matBase = newMat;
	
	// Update volumes too
	Material::UpdateMaterialReferences(oldMat, newMat);
}

bool GlossyCoatingMaterial::IsReferencing(const Material *mat) const {
	if (mat == this)
		return true;

	return matBase->IsReferencing(mat);
}

void GlossyCoatingMaterial::AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
	Material::AddReferencedMaterials(referencedMats);

	matBase->AddReferencedMaterials(referencedMats);
}

void GlossyCoatingMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	matBase->AddReferencedTextures(referencedTexs);
	Ks->AddReferencedTextures(referencedTexs);
	nu->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
	Ka->AddReferencedTextures(referencedTexs);
	depth->AddReferencedTextures(referencedTexs);
	index->AddReferencedTextures(referencedTexs);
}

void GlossyCoatingMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Ks == oldTex)
		Ks = newTex;
	if (nu == oldTex)
		nu = newTex;
	if (nv == oldTex)
		nv = newTex;
	if (Ka == oldTex)
		Ka = newTex;
	if (depth == oldTex)
		depth = newTex;
	if (index == oldTex)
		index = newTex;

	// Always update glossiness just in case matBase->GetGlossiness() has changed
	const float coatGlossiness = ComputeGlossiness(nu, nv);
	if (matBase->GetEventTypes() & GLOSSY)
		glossiness = Min(coatGlossiness, matBase->GetGlossiness());
	else
		glossiness = coatGlossiness;
}

Properties GlossyCoatingMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("glossycoating"));
	props.Set(Property("scene.materials." + name + ".base")(matBase->GetName()));
	props.Set(Property("scene.materials." + name + ".ks")(Ks->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ka")(Ka->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".d")(depth->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".index")(index->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".multibounce")(multibounce));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
