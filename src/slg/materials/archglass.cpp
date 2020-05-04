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
#include "slg/materials/archglass.h"
#include "slg/materials/thinfilmcoating.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

ArchGlassMaterial::ArchGlassMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Texture *refl, const Texture *trans,
		const Texture *exteriorIorFact, const Texture *interiorIorFact,
		const Texture *filmThickness, const Texture *filmIor) :
			Material(frontTransp, backTransp, emitted, bump),
			Kr(refl), Kt(trans), exteriorIor(exteriorIorFact), interiorIor(interiorIorFact),
			filmThickness(filmThickness), filmIor(filmIor) {
}

Spectrum ArchGlassMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum ArchGlassMaterial::EvalSpecularReflection(const HitPoint &hitPoint,
		const Vector &localFixedDir, const Spectrum &kr,
		const float nc, const float nt,
		Vector *localSampledDir,
		const float localFilmThickness, const float localFilmIor) {
	if (kr.Black())
		return Spectrum();

	const float costheta = CosTheta(localFixedDir);
	if (costheta <= 0.f)
		return Spectrum();

	*localSampledDir = Vector(-localFixedDir.x, -localFixedDir.y, localFixedDir.z);

	const float ntc = nt / nc;
	const Spectrum result = kr * FresnelTexture::CauchyEvaluate(ntc, costheta);
	
	if (localFilmThickness > 0.f) {
		const Spectrum filmColor = CalcFilmColor(localFixedDir, localFilmThickness, localFilmIor);
		return result * filmColor;
	}
	return result;
}

Spectrum ArchGlassMaterial::EvalSpecularTransmission(const HitPoint &hitPoint,
		const Vector &localFixedDir, const Spectrum &kt,
		const float nc, const float nt, Vector *localSampledDir) {
	if (kt.Black())
		return Spectrum();

	// Note: there can not be total internal reflection for 
	
	*localSampledDir = -localFixedDir;

	const float ntc = nt / nc;
	const float costheta = CosTheta(localFixedDir);
	const bool entering = (costheta > 0.f);
	float ce;
	if (!hitPoint.fromLight) {
		if (entering)
			ce = 0.f;
		else
			ce = FresnelTexture::CauchyEvaluate(ntc, -costheta);
	} else {
		if (entering)
			ce = FresnelTexture::CauchyEvaluate(ntc, costheta);
		else
			ce = 0.f;
	}
	const float factor = 1.f - ce;
	const float result = (1.f + factor * factor) * ce;

	return (1.f - result) * kt;
}

Spectrum ArchGlassMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event, const BSDFEvent eventHint) const {
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);

	const float nc = ExtractExteriorIors(hitPoint, exteriorIor);
	const float nt = ExtractInteriorIors(hitPoint, interiorIor);

	Vector transLocalSampledDir; 
	const Spectrum trans = EvalSpecularTransmission(hitPoint, localFixedDir,
			kt, nc, nt, &transLocalSampledDir);
	
	const float localFilmThickness = filmThickness ? filmThickness->GetFloatValue(hitPoint) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? filmIor->GetFloatValue(hitPoint) : 1.f;
	Vector reflLocalSampledDir;
	const Spectrum refl = EvalSpecularReflection(hitPoint, localFixedDir,
			kr, nc, nt, &reflLocalSampledDir, localFilmThickness, localFilmIor);

	// Decide to transmit or reflect
	float threshold;
	if (!refl.Black() && (eventHint != TRANSMIT)) {
		if (!trans.Black() && (eventHint != REFLECT)) {
			// Importance sampling
			const float reflFilter = refl.Filter();
			const float transFilter = trans.Filter();
			threshold = transFilter / (reflFilter + transFilter);
		} else
			threshold = 0.f;
	} else {
		// ArchGlassMaterial::Sample() can be called only if ArchGlassMaterial::GetPassThroughTransparency()
		// has detected a reflection or a mixed reflection/transmission.
		// Here, there was no reflection at all so I return black.
		return Spectrum();
	}

	Spectrum result;
	if (passThroughEvent < threshold) {
		// Transmit

		*localSampledDir = transLocalSampledDir;

		*event = SPECULAR | TRANSMIT;
		*pdfW = threshold;
		
		result = trans;
	} else {
		// Reflect
		*localSampledDir = reflLocalSampledDir;

		*event = SPECULAR | REFLECT;
		*pdfW = 1.f - threshold;

		result = refl;
	}

	return result / *pdfW;
}

Spectrum ArchGlassMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const Vector &localFixedDir, const float passThroughEvent, const bool backTracing) const {
	const Spectrum kt = Kt->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);
	const Spectrum kr = Kr->GetSpectrumValue(hitPoint).Clamp(0.f, 1.f);

	const float nc = ExtractExteriorIors(hitPoint, exteriorIor);
	const float nt = ExtractInteriorIors(hitPoint, interiorIor);

	Vector transLocalSampledDir; 
	const Spectrum trans = EvalSpecularTransmission(hitPoint, localFixedDir,
			kt, nc, nt, &transLocalSampledDir);
	
	const float localFilmThickness = filmThickness ? filmThickness->GetFloatValue(hitPoint) : 0.f;
	const float localFilmIor = (localFilmThickness > 0.f && filmIor) ? filmIor->GetFloatValue(hitPoint) : 1.f;
	Vector reflLocalSampledDir;
	const Spectrum refl = EvalSpecularReflection(hitPoint, localFixedDir,
			kr, nc, nt, &reflLocalSampledDir, localFilmThickness, localFilmIor);

	// Decide to transmit or reflect
	float threshold;
	if (!refl.Black()) {
		if (!trans.Black()) {
			// Importance sampling
			const float reflFilter = refl.Filter();
			const float transFilter = trans.Filter();
			threshold = transFilter / (reflFilter + transFilter);
			
			if (passThroughEvent < threshold) {
				// Transmit
				return trans / threshold;
			} else {
				// Reflect
				return Spectrum();
			}
		} else
			return Spectrum();
	} else {
		if (!trans.Black()) {
			// Transmit

			// threshold = 1 so I avoid the / threshold
			return trans;
		} else
			return Spectrum();
	}
}

void ArchGlassMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	Kr->AddReferencedTextures(referencedTexs);
	Kt->AddReferencedTextures(referencedTexs);
	if (exteriorIor)
		exteriorIor->AddReferencedTextures(referencedTexs);
	if (interiorIor)
		interiorIor->AddReferencedTextures(referencedTexs);
	if (filmThickness)
		filmThickness->AddReferencedTextures(referencedTexs);
	if (filmIor)
		filmIor->AddReferencedTextures(referencedTexs);
}

void ArchGlassMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	Material::UpdateTextureReferences(oldTex, newTex);

	if (Kr == oldTex)
		Kr = newTex;
	if (Kt == oldTex)
		Kt = newTex;
	if (exteriorIor == oldTex)
		exteriorIor = newTex;
	if (interiorIor == oldTex)
		interiorIor = newTex;
	if (filmThickness == oldTex)
		filmThickness = newTex;
	if (filmIor == oldTex)
		filmIor = newTex;
}

Properties ArchGlassMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("archglass"));
	props.Set(Property("scene.materials." + name + ".kr")(Kr->GetSDLValue()));
	props.Set(Property("scene.materials." + name + ".kt")(Kt->GetSDLValue()));
	if (exteriorIor)
		props.Set(Property("scene.materials." + name + ".exteriorior")(exteriorIor->GetSDLValue()));
	if (interiorIor)
		props.Set(Property("scene.materials." + name + ".interiorior")(interiorIor->GetSDLValue()));
	if (filmThickness)
		props.Set(Property("scene.materials." + name + ".filmthickness")(filmThickness->GetSDLValue()));
	if (filmIor)
		props.Set(Property("scene.materials." + name + ".filmior")(filmIor->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
