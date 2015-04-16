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

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/frame.h"
#include "slg/materials/material.h"
#include "slg/bsdf/bsdf.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Material
//------------------------------------------------------------------------------

void Material::SetEmissionMap(const ImageMap *map) {
	emissionMap = map;
	delete emissionFunc;
	if (emissionMap)
		emissionFunc = new SampleableSphericalFunction(new ImageMapSphericalFunction(emissionMap));
	else
		emissionFunc = NULL;
}

Spectrum Material::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	if (emittedTex) {
		return (emittedFactor * (usePrimitiveArea ? oneOverPrimitiveArea : 1.f)) *
				emittedTex->GetSpectrumValue(hitPoint) * hitPoint.color;
	} else
		return Spectrum();
}

float Material::GetEmittedRadianceY() const {
	if (emittedTex)
		return emittedFactor.Y() * emittedTex->Y();
	else
		return 0.f;
}

void Material::Bump(HitPoint *hitPoint, const float weight) const {
    if (bumpTex && (weight > 0.f)) {
        const UV duv = weight * bumpTex->GetDuv(*hitPoint, bumpSampleDistance);

        hitPoint->dpdu += duv.u * Vector(hitPoint->shadeN);
        hitPoint->dpdv += duv.v * Vector(hitPoint->shadeN);

        const Normal oldShadeN = hitPoint->shadeN;
        hitPoint->shadeN = Normal(Normalize(Cross(hitPoint->dpdu, hitPoint->dpdv)));

        // The above transform keeps the normal in the original normal
        // hemisphere. If they are opposed, it means UVN was indirect and
        // the normal needs to be reversed.
        hitPoint->shadeN *= (Dot(oldShadeN, hitPoint->shadeN) < 0.f) ? -1.f : 1.f;
    }
}

Properties Material::ToProperties() const {
	luxrays::Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".id")(matID));
	props.Set(Property("scene.materials." + name + ".emission.gain")(emittedGain));
	props.Set(Property("scene.materials." + name + ".emission.power")(emittedPower));
	props.Set(Property("scene.materials." + name + ".emission.efficency")(emittedEfficency));
	props.Set(Property("scene.materials." + name + ".emission.samples")(emittedSamples));
	if (emittedTex)
		props.Set(Property("scene.materials." + name + ".emission")(emittedTex->GetName()));
	if (bumpTex)
		props.Set(Property("scene.materials." + name + ".bumptex")(bumpTex->GetName()));

	if (interiorVolume)
		props.Set(Property("scene.materials." + name + ".volume.interior")(interiorVolume->GetName()));
	if (exteriorVolume)
		props.Set(Property("scene.materials." + name + ".volume.exterior")(exteriorVolume->GetName()));
		
	props.Set(Property("scene.materials." + name + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property("scene.materials." + name + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

void Material::UpdateEmittedFactor() {
	if (emittedTex) {
		emittedFactor = emittedGain * (emittedPower * emittedEfficency / (M_PI * emittedTex->Y()));
		if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN()) {
			emittedFactor = emittedGain;
			usePrimitiveArea = false;
		} else
			usePrimitiveArea = true;
	} else {
		emittedFactor = emittedGain;
		usePrimitiveArea = false;
	}
}

void Material::UpdateMaterialReferences(Material *oldMat, Material *newMat) {
	if (oldMat == interiorVolume)
		interiorVolume = (Volume *)newMat;
	if (oldMat == exteriorVolume)
		exteriorVolume = (Volume *)newMat;
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
	const float cosNH2 = cosNH * cosNH;
	// expanded for increased numerical stability
	const float d = cosNH2 * roughness + (1.f - cosNH2);
	// use double division to avoid overflow in d*d product
	return (roughness / d) / d;
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

	const float G = SchlickDistribution_G(roughness, localFixedDir, localSampledDir);

	// Multibounce - alternative with interreflection in the coating creases
	float factor = SchlickDistribution_D(roughness, wh, anisotropy) * G;
	if (!fromLight)
		factor = factor / 4.f * coso +
				(mbounce ? cosi * Clamp((1.f - G) / (4.f * coso * cosi), 0.f, 1.f) : 0.f);
	else
		factor = factor / (4.f * cosi) + 
				(mbounce ? coso * Clamp((1.f - G) / (4.f * cosi * coso), 0.f, 1.f) : 0.f);

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

	*pdf = specPdf / (4.f * fabsf(cosWH));
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
