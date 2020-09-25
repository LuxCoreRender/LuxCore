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
#include "slg/materials/glossytranslucent.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Glossy Translucent material
//
// LuxRender GlossyTranslucent material porting.
//------------------------------------------------------------------------------

GlossyTranslucentMaterial::GlossyTranslucentMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *kd, const Texture *kt, const Texture *ks, const Texture *ks2,
		const Texture *u, const Texture *u2, const Texture *v, const Texture *v2,
		const Texture *ka, const Texture *ka2, const Texture *d, const Texture *d2,
		const Texture *i, const Texture *i2, const bool mbounce, const bool mbounce2) :
			Material(frontTransp, backTransp, emitted, bump), Kd(kd), Kt(kt), Ks(ks), Ks_bf(ks2), nu(u), nu_bf(u2),
			nv(v), nv_bf(v2), Ka(ka), Ka_bf(ka2), depth(d), depth_bf(d2), index(i),
			index_bf(i2), multibounce(mbounce), multibounce_bf(mbounce2) {
	glossiness = Min(ComputeGlossiness(nu, nv), ComputeGlossiness(nu_bf, nv_bf));
}

Spectrum GlossyTranslucentMaterial::Albedo(const HitPoint &hitPoint) const {
	return Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
}

Spectrum GlossyTranslucentMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;
	const float cosi = fabsf(localLightDir.z);
	const float coso = fabsf(localEyeDir.z);

	const Frame frame(hitPoint.GetFrame());
	const float sideTest = Dot(frame.ToWorld(localFixedDir), hitPoint.geometryN) * Dot(frame.ToWorld(localSampledDir), hitPoint.geometryN);

	if (sideTest < -DEFAULT_COS_EPSILON_STATIC) {
		// Transmission
		*event = DIFFUSE | TRANSMIT;

		if (directPdfW)
			*directPdfW = fabsf(localSampledDir.z) * (INV_PI * .5f);
		if (reversePdfW)
			*reversePdfW = fabsf(localFixedDir.z) * (INV_PI * .5f);

		const Vector H(Normalize(Vector(localLightDir.x + localEyeDir.x, localLightDir.y + localEyeDir.y,
			localLightDir.z - localEyeDir.z)));
		const float u = AbsDot(localLightDir, H);
		Spectrum ks = Ks->GetSpectrumValue(hitPoint);
		float i = index->GetFloatValue(hitPoint);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);
		const Spectrum S1 = FresnelTexture::SchlickEvaluate(ks, u);

		ks = Ks_bf->GetSpectrumValue(hitPoint);
		i = index_bf->GetFloatValue(hitPoint);
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);
		const Spectrum S2 = FresnelTexture::SchlickEvaluate(ks, u);
		Spectrum S(Sqrt((Spectrum(1.f) - S1) * (Spectrum(1.f) - S2)));
		if (localLightDir.z > 0.f) {
			S *= Exp(Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * -(depth->GetFloatValue(hitPoint) / cosi) +
				Ka_bf->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * -(depth_bf->GetFloatValue(hitPoint) / coso));
		} else {
			S *= Exp(Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * -(depth->GetFloatValue(hitPoint) / coso) +
				Ka_bf->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * -(depth_bf->GetFloatValue(hitPoint) / cosi));
		}

		return (INV_PI * cosi) * S * Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) *
			(Spectrum(1.f) - Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f));
	} else if (sideTest > DEFAULT_COS_EPSILON_STATIC) {
		// Reflection
		*event = GLOSSY | REFLECT;

		const Spectrum baseF = Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * INV_PI * cosi;
		Spectrum ks, alpha;
		float i, u, v, d;
		bool mbounce;
		if (localEyeDir.z >= 0.f) {
			ks = Ks->GetSpectrumValue(hitPoint);
			i = index->GetFloatValue(hitPoint);
			u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
			alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
			d = depth->GetFloatValue(hitPoint);
			mbounce = multibounce;
		} else {
			ks = Ks_bf->GetSpectrumValue(hitPoint);
			i = index_bf->GetFloatValue(hitPoint);
			u = Clamp(nu_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
			alpha = Ka_bf->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
			d = depth_bf->GetFloatValue(hitPoint);
			mbounce = multibounce_bf;
		}

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		if (directPdfW) {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = .5f * (wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir));
		}

		if (reversePdfW) {
			const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW = .5f * (wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir));
		}

		// Absorption
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const Vector H(Normalize(localFixedDir + localSampledDir));
		const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(localSampledDir, H));

		const Spectrum coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, mbounce,
			localFixedDir, localSampledDir);

		// Blend in base layer Schlick style
		// assumes coating bxdf takes fresnel factor S into account

		return coatingF + absorption * (Spectrum(1.f) - S) * baseF;
	} else
		return Spectrum();
}

Spectrum GlossyTranslucentMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	if (fabsf(localFixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	if (passThroughEvent < .5f) {
		// Reflection
		Spectrum ks, alpha;
		float i, u, v, d;
		bool mbounce;
		if (localFixedDir.z >= 0.f) {
			ks = Ks->GetSpectrumValue(hitPoint);
			i = index->GetFloatValue(hitPoint);
			u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
			alpha = Ka->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
			d = depth->GetFloatValue(hitPoint);
			mbounce = multibounce;
		} else {
			ks = Ks_bf->GetSpectrumValue(hitPoint);
			i = index_bf->GetFloatValue(hitPoint);
			u = Clamp(nu_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
			alpha = Ka_bf->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
			d = depth_bf->GetFloatValue(hitPoint);
			mbounce = multibounce_bf;
		}
		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
		const float wBase = 1.f - wCoating;

		float basePdf, coatingPdf;
		Spectrum baseF, coatingF;

		if (2.f * passThroughEvent < wBase) {
			// Sample base BSDF (Matte BSDF)
			*localSampledDir = Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, &basePdf);

			const float absCosSampledDir = fabsf(localSampledDir->z);
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return Spectrum();

			baseF = Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * INV_PI * fabsf(hitPoint.fromLight ? localFixedDir.z : absCosSampledDir);

			// Evaluate coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingF(hitPoint.fromLight, ks, roughness, anisotropy, mbounce,
				localFixedDir, *localSampledDir);
			coatingPdf = SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, *localSampledDir);
		} else {
			// Sample coating BSDF (Schlick BSDF)
			coatingF = SchlickBSDF_CoatingSampleF(hitPoint.fromLight, ks, roughness, anisotropy, mbounce,
				localFixedDir, localSampledDir, u0, u1, &coatingPdf);
			if (coatingF.Black())
				return Spectrum();

			const float absCosSampledDir = fabsf(localSampledDir->z);
			if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
				return Spectrum();

			coatingF *= coatingPdf;

			// Evaluate base BSDF (Matte BSDF)
			basePdf = absCosSampledDir * INV_PI;
			baseF = Kd->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f) * INV_PI * fabsf(hitPoint.fromLight ? localFixedDir.z : absCosSampledDir);
		}
		*event = GLOSSY | REFLECT;

		const Frame frame(hitPoint.GetFrame());
		const float sideTest = Dot(frame.ToWorld(localFixedDir), hitPoint.geometryN) * Dot(frame.ToWorld(*localSampledDir), hitPoint.geometryN);
		if (!(sideTest > DEFAULT_COS_EPSILON_STATIC))
			return Spectrum();

		*pdfW = .5f * (coatingPdf * wCoating + basePdf * wBase);

		// Absorption
		const float cosi = fabsf(localSampledDir->z);
		const float coso = fabsf(localFixedDir.z);
		const Spectrum absorption = CoatingAbsorption(cosi, coso, alpha, d);

		// Coating fresnel factor
		const Vector H(Normalize(localFixedDir + *localSampledDir));
		const Spectrum S = FresnelTexture::SchlickEvaluate(ks, AbsDot(*localSampledDir, H));

		// Blend in base layer Schlick style
		// coatingF already takes fresnel factor S into account

		return (coatingF + absorption * (Spectrum(1.f) - S) * baseF) / *pdfW;
	} else {
		// Transmission
		*localSampledDir = -Sgn(localFixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

		const Frame frame(hitPoint.dpdu, hitPoint.dpdv, hitPoint.shadeN);
		const float sideTest = Dot(frame.ToWorld(localFixedDir), hitPoint.geometryN) * Dot(frame.ToWorld(*localSampledDir), hitPoint.geometryN);
		if (!(sideTest < -DEFAULT_COS_EPSILON_STATIC))
			return Spectrum();

		*pdfW *= .5f;

		const float absCosSampledDir = fabsf(localSampledDir->z);
		if (absCosSampledDir < DEFAULT_COS_EPSILON_STATIC)
			return Spectrum();

		// pdfW and the pdf computed inside Evaluate() are exactly the same so
		// it doesn't matter if it is overwritten
		return Evaluate(hitPoint,
				hitPoint.fromLight ? localFixedDir : *localSampledDir,
				hitPoint.fromLight ? *localSampledDir : localFixedDir,
				event, pdfW, NULL) / *pdfW;
	}
}

void GlossyTranslucentMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Vector &localFixedDir = hitPoint.fromLight ? localLightDir : localEyeDir;
	const Vector &localSampledDir = hitPoint.fromLight ? localEyeDir : localLightDir;

	if (localFixedDir.z * localSampledDir.z < 0.f) {
		// Transmission
		if (directPdfW)
			*directPdfW = fabsf(localSampledDir.z) * (INV_PI * .5f);
		if (reversePdfW)
			*reversePdfW = fabsf(localFixedDir.z) * (INV_PI * .5f);
	} else if (localFixedDir.z * localSampledDir.z > 0.f) {
		// Reflection
		Spectrum ks;
		float i, u, v;
		if (localEyeDir.z >= 0.f) {
			ks = Ks->GetSpectrumValue(hitPoint);
			i = index->GetFloatValue(hitPoint);
			u = Clamp(nu->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv->GetFloatValue(hitPoint), 1e-9f, 1.f);
		} else {
			ks = Ks_bf->GetSpectrumValue(hitPoint);
			i = index_bf->GetFloatValue(hitPoint);
			u = Clamp(nu_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
			v = Clamp(nv_bf->GetFloatValue(hitPoint), 1e-9f, 1.f);
		}

		if (i > 0.f) {
			const float ti = (i - 1.f) / (i + 1.f);
			ks *= ti * ti;
		}
		ks = ks.Clamp(0.f, 1.f);

		const float u2 = u * u;
		const float v2 = v * v;
		const float anisotropy = (u2 < v2) ? (1.f - u2 / v2) : u2 > 0.f ? (v2 / u2 - 1.f) : 0.f;
		const float roughness = u * v;

		if (directPdfW) {
			const float wCoating = SchlickBSDF_CoatingWeight(ks, localFixedDir);
			const float wBase = 1.f - wCoating;

			*directPdfW = .5f * (wBase * fabsf(localSampledDir.z * INV_PI) +
				wCoating * SchlickBSDF_CoatingPdf(roughness, anisotropy, localFixedDir, localSampledDir));
		}

		if (reversePdfW) {
			const float wCoatingR = SchlickBSDF_CoatingWeight(ks, localSampledDir);
			const float wBaseR = 1.f - wCoatingR;

			*reversePdfW = .5f * (wBaseR * fabsf(localFixedDir.z * INV_PI) +
				wCoatingR * SchlickBSDF_CoatingPdf(roughness, anisotropy, localSampledDir, localFixedDir));
		}
	} else {
		if (directPdfW)
			*directPdfW = 0.f;
		if (reversePdfW)
			*reversePdfW = 0.f;
	}
}

void GlossyTranslucentMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kd->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	Ks->AddReferencedTextures(referencedTexs);
	Ks_bf->AddReferencedTextures(referencedTexs);
	nu->AddReferencedTextures(referencedTexs);
	nu_bf->AddReferencedTextures(referencedTexs);
	nv->AddReferencedTextures(referencedTexs);
	nv_bf->AddReferencedTextures(referencedTexs);
	Ka->AddReferencedTextures(referencedTexs);
	Ka_bf->AddReferencedTextures(referencedTexs);
	depth->AddReferencedTextures(referencedTexs);
	depth_bf->AddReferencedTextures(referencedTexs);
	index->AddReferencedTextures(referencedTexs);
	index_bf->AddReferencedTextures(referencedTexs);
}

void GlossyTranslucentMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	bool updateGlossiness = false;
	if (Kd == oldTex)
		Kd = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (Ks == oldTex)
		Ks = newTex;
	if (Ks_bf == oldTex)
		Ks_bf = newTex;
	if (nu == oldTex) {
		nu = newTex;
		updateGlossiness = true;
	}
	if (nu_bf == oldTex) {
		nu_bf = newTex;
		updateGlossiness = true;
	}
	if (nv == oldTex) {
		nv = newTex;
		updateGlossiness = true;
	}
	if (nv_bf == oldTex) {
		nv_bf = newTex;
		updateGlossiness = true;
	}
	if (Ka == oldTex)
		Ka = newTex;
	if (Ka_bf == oldTex)
		Ka_bf = newTex;
	if (depth == oldTex)
		depth = newTex;
	if (depth_bf == oldTex)
		depth_bf = newTex;
	if (index == oldTex)
		index = newTex;
	if (index_bf == oldTex)
		index_bf = newTex;
	
	if (updateGlossiness)
		glossiness = Min(ComputeGlossiness(nu, nv), ComputeGlossiness(nu_bf, nv_bf));
}

Properties GlossyTranslucentMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("glossytranslucent"));
	props.Set(Property("scene.materials." + name + ".kd")(Kd->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ks")(Ks->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ks_bf")(Ks_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".uroughness")(nu->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".uroughness_bf")(nu_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".vroughness")(nv->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".vroughness_bf")(nv_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ka")(Ka->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".ka_bf")(Ka_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".d")(depth->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".d_bf")(depth_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".index")(index->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".index_bf")(index_bf->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".multibounce")(multibounce));
	props.Set(Property("scene.materials." + name + ".multibounce_bf")(multibounce_bf));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
