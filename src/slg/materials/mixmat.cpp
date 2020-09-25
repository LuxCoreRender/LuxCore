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

#include "slg/materials/mix.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

MixMaterial::MixMaterial(const Texture *frontTransp, const Texture *backTransp,
		const Texture *emitted, const Texture *bump,
		const Material *mA, const Material *mB, const Texture *mix) :
			Material(frontTransp, backTransp, emitted, bump),
			matA(mA), matB(mB), mixFactor(mix) {
	Preprocess();
}

BSDFEvent MixMaterial::GetEventTypesImpl() const {
	return (matA->GetEventTypes() | matB->GetEventTypes());
}

bool MixMaterial::IsLightSourceImpl() const {
	return (Material::IsLightSource() || matA->IsLightSource() || matB->IsLightSource());
}

bool MixMaterial::IsDeltaImpl() const {
	return (matA->IsDelta() && matB->IsDelta());
}

void MixMaterial::Preprocess() {
	// Cache values for performance with very large material node trees

	eventTypes = GetEventTypesImpl();

	if (matA->GetEventTypes() & GLOSSY) {
		if (matB->GetEventTypes() & GLOSSY)
			glossiness = Min(matA->GetGlossiness(), matB->GetGlossiness());
		else
			glossiness = matA->GetGlossiness();
	} else {
		if (matB->GetEventTypes() & GLOSSY)
			glossiness = matB->GetGlossiness();
		else
			glossiness = 0.f;
	}

	isLightSource = IsLightSourceImpl();
	isDelta = IsDeltaImpl();
}

const Volume *MixMaterial::GetInteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const {
	if (interiorVolume)
		return interiorVolume;
	else {
		const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (passThroughEvent < weight1)
			return matA->GetInteriorVolume(hitPoint, passThroughEvent / weight1);
		else
			return matB->GetInteriorVolume(hitPoint, (passThroughEvent - weight1) / weight2);
	}
}

const Volume *MixMaterial::GetExteriorVolume(const HitPoint &hitPoint,
		const float passThroughEvent) const {
	if (exteriorVolume)
		return exteriorVolume;
	else {
		const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (passThroughEvent < weight1)
			return matA->GetExteriorVolume(hitPoint, passThroughEvent / weight1);
		else
			return matB->GetExteriorVolume(hitPoint, (passThroughEvent - weight1) / weight2);
	}
}

void MixMaterial::UpdateAvgPassThroughTransparency() {
	if (frontTransparencyTex || backTransparencyTex)
		Material::UpdateAvgPassThroughTransparency();
	else {
		const float avgMix = mixFactor->Filter();

		const float weight2 = Clamp(avgMix, 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		avgPassThroughTransparency = weight1 * matA->GetAvgPassThroughTransparency() +
				weight2 * matB->GetAvgPassThroughTransparency();
	}
}

Spectrum MixMaterial::GetPassThroughTransparency(const HitPoint &hitPoint,
		const Vector &localFixedDir, const float passThroughEvent,
		const bool backTracing) const {
	if (frontTransparencyTex || backTransparencyTex) {
		return Material::GetPassThroughTransparency(hitPoint, localFixedDir,
				passThroughEvent, backTracing);
	} else {
		const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (passThroughEvent < weight1) {
			return matA->GetPassThroughTransparency(hitPoint, localFixedDir,
					passThroughEvent / weight1, backTracing);
		} else {
			return matB->GetPassThroughTransparency(hitPoint, localFixedDir,
					(passThroughEvent - weight1) / weight2, backTracing);
		}
	}
}

float MixMaterial::GetEmittedRadianceY(const float oneOverPrimitiveArea) const {
	if (emittedTex)
		return Material::GetEmittedRadianceY(oneOverPrimitiveArea);
	else
		return luxrays::Lerp(mixFactor->Y(), matA->GetEmittedRadianceY(oneOverPrimitiveArea), matB->GetEmittedRadianceY(oneOverPrimitiveArea));
}

Spectrum MixMaterial::GetEmittedRadiance(const HitPoint &hitPoint, const float oneOverPrimitiveArea) const {
	if (emittedTex)
		return Material::GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);
	else {
		Spectrum result;

		const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
		const float weight1 = 1.f - weight2;

		if (matA->IsLightSource() && (weight1 > 0.f))
			result += weight1 * matA->GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);
		if (matB->IsLightSource() && (weight2 > 0.f))
			result += weight2 * matB->GetEmittedRadiance(hitPoint, oneOverPrimitiveArea);

		return result;
	}
}

Spectrum MixMaterial::Albedo(const HitPoint &hitPoint) const {
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	return weight1 * matA->Albedo(hitPoint) + weight2 * matB->Albedo(hitPoint);
}

Spectrum MixMaterial::Evaluate(const HitPoint &hitPoint,
	const Vector &localLightDir, const Vector &localEyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const Frame frame(hitPoint.GetFrame());
	Spectrum result;

	// This test is usually done by BSDF::Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (Sgn(localLightDir.z) != Sgn(localEyeDir.z));
	
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (directPdfW)
		*directPdfW = 0.f;
	if (reversePdfW)
		*reversePdfW = 0.f;

	BSDFEvent eventMatA = matA->GetEventTypes();
	if ((weight1 > 0.f) &&
			((!isTransmitEval && (eventMatA & REFLECT)) ||
			(isTransmitEval && (eventMatA & TRANSMIT)))) {
		HitPoint hitPointA(hitPoint);
		matA->Bump(&hitPointA);
		const Frame frameA(hitPointA.GetFrame());
		const Vector lightDirA = frameA.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirA = frameA.ToLocal(frame.ToWorld(localEyeDir));
		float directPdfWMatA, reversePdfWMatA;
		const Spectrum matAResult = matA->Evaluate(hitPointA, lightDirA, eyeDirA, &eventMatA, &directPdfWMatA, &reversePdfWMatA);
		if (!matAResult.Black()) {
			result += weight1 * matAResult;

			if (directPdfW)
				*directPdfW += weight1 * directPdfWMatA;
			if (reversePdfW)
				*reversePdfW += weight1 * reversePdfWMatA;
		}
	}

	BSDFEvent eventMatB = matB->GetEventTypes();
	if ((weight2 > 0.f) &&
			((!isTransmitEval && (eventMatB & REFLECT)) ||
			(isTransmitEval && (eventMatB & TRANSMIT)))) {
		HitPoint hitPointB(hitPoint);
		matB->Bump(&hitPointB);
		const Frame frameB(hitPointB.GetFrame());
		const Vector lightDirB = frameB.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirB = frameB.ToLocal(frame.ToWorld(localEyeDir));

		float directPdfWMatB, reversePdfWMatB;
		const Spectrum matBResult = matB->Evaluate(hitPointB, lightDirB, eyeDirB, &eventMatB, &directPdfWMatB, &reversePdfWMatB);
		if (!matBResult.Black()) {
			result += weight2 * matBResult;

			if (directPdfW)
				*directPdfW += weight2 * directPdfWMatB;
			if (reversePdfW)
				*reversePdfW += weight2 * reversePdfWMatB;
		}
	}

	*event = eventMatA | eventMatB;

	return result;
}

Spectrum MixMaterial::Sample(const HitPoint &hitPoint,
	const Vector &localFixedDir, Vector *localSampledDir,
	const float u0, const float u1, const float passThroughEvent,
	float *pdfW, BSDFEvent *event) const {
	const Frame frame(hitPoint.GetFrame());

	HitPoint hitPointA(hitPoint);
	matA->Bump(&hitPointA);
	const Frame frameA(hitPointA.GetFrame());
	const Vector fixedDirA = frameA.ToLocal(frame.ToWorld(localFixedDir));

	HitPoint hitPointB(hitPoint);
	matB->Bump(&hitPointB);
	const Frame frameB(hitPointB.GetFrame());
	const Vector fixedDirB = frameB.ToLocal(frame.ToWorld(localFixedDir));

	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	const bool sampleMatA = (passThroughEvent < weight1);

	const float weightFirst = sampleMatA ? weight1 : weight2;
	const float weightSecond = sampleMatA ? weight2 : weight1;

	const float passThroughEventFirst = sampleMatA ? (passThroughEvent / weight1) : (passThroughEvent - weight1) / weight2;

	// Sample the first material, evaluate the second
	const Material *matFirst = sampleMatA ? matA : matB;
	const Material *matSecond = sampleMatA ? matB : matA;

	HitPoint &hitPoint1 = sampleMatA ? hitPointA : hitPointB;
	HitPoint &hitPoint2 = sampleMatA ? hitPointB : hitPointA;
	const Frame &frame1 = sampleMatA ? frameA : frameB;
	const Frame &frame2 = sampleMatA ? frameB : frameA;
	const Vector &fixedDir1 = sampleMatA ? fixedDirA : fixedDirB;
	const Vector &fixedDir2 = sampleMatA ? fixedDirB : fixedDirA;

	// Sample the first material
	Spectrum result = matFirst->Sample(hitPoint1, fixedDir1, localSampledDir,
			u0, u1, passThroughEventFirst, pdfW, event);
	if (result.Black())
		return Spectrum();

	*localSampledDir = frame1.ToWorld(*localSampledDir);
	const Vector sampledDir2 = frame2.ToLocal(*localSampledDir);
	*localSampledDir = frame.ToLocal(*localSampledDir);

	*pdfW *= weightFirst;

	if ((*event) & SPECULAR)
		return result;
	
	result *= *pdfW;

	// Evaluate the second material
	const Vector &localLightDir = (hitPoint2.fromLight) ? fixedDir2 : sampledDir2;
	const Vector &localEyeDir = (hitPoint2.fromLight) ? sampledDir2 : fixedDir2;
	
	// This test is usually done by BSDF::Evaluate() and must be repeated in
	// material referencing other materials
	const float isTransmitEval = (Sgn(localLightDir.z) != Sgn(localEyeDir.z));

	const BSDFEvent eventSecondMat = matSecond->GetEventTypes();
	if ((!isTransmitEval && (eventSecondMat & REFLECT)) ||
			(isTransmitEval && (eventSecondMat & TRANSMIT))) {
		BSDFEvent eventSecond;
		float pdfWSecond;
		Spectrum evalSecond = matSecond->Evaluate(hitPoint2, localLightDir, localEyeDir, &eventSecond, &pdfWSecond);
		if (!evalSecond.Black()) {
			result += weightSecond * evalSecond;
			*pdfW += weightSecond * pdfWSecond;
		}
	}

	return result / *pdfW;
}

void MixMaterial::Pdf(const HitPoint &hitPoint,
		const Vector &localLightDir, const Vector &localEyeDir,
		float *directPdfW, float *reversePdfW) const {
	const Frame frame(hitPoint.GetFrame());
	const float weight2 = Clamp(mixFactor->GetFloatValue(hitPoint), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	float directPdfWMatA = 1.f;
	float reversePdfWMatA = 1.f;
	if (weight1 > 0.f) {
		HitPoint hitPointA(hitPoint);
		matA->Bump(&hitPointA);
		const Frame frameA(hitPointA.GetFrame());
		const Vector lightDirA = frameA.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirA = frameA.ToLocal(frame.ToWorld(localEyeDir));
		matA->Pdf(hitPointA, lightDirA, eyeDirA, &directPdfWMatA, &reversePdfWMatA);
	}

	float directPdfWMatB = 1.f;
	float reversePdfWMatB = 1.f;
	if (weight2 > 0.f) {
		HitPoint hitPointB(hitPoint);
		matB->Bump(&hitPointB);
		const Frame frameB(hitPointB.GetFrame());
		const Vector lightDirB = frameB.ToLocal(frame.ToWorld(localLightDir));
		const Vector eyeDirB = frameB.ToLocal(frame.ToWorld(localEyeDir));
		matB->Pdf(hitPointB, lightDirB, eyeDirB, &directPdfWMatB, &reversePdfWMatB);
	}

	if (directPdfW)
		*directPdfW = weight1 * directPdfWMatA + weight2 *directPdfWMatB;
	if (reversePdfW)
		*reversePdfW = weight1 * reversePdfWMatA + weight2 * reversePdfWMatB;
}

void MixMaterial::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	if (matA == oldMat)
		matA = newMat;

	if (matB == oldMat)
		matB = newMat;
	
	// Update volumes too
	Material::UpdateMaterialReferences(oldMat, newMat);
	
	Preprocess();
}

bool MixMaterial::IsReferencing(const Material *mat) const {
	return matA == mat || matA->IsReferencing(mat) ||
		matB == mat || matB->IsReferencing(mat);
}

void MixMaterial::AddReferencedMaterials(boost::unordered_set<const Material *> &referencedMats) const {
	Material::AddReferencedMaterials(referencedMats);

	referencedMats.insert(matA);
	matA->AddReferencedMaterials(referencedMats);

	referencedMats.insert(matB);
	matB->AddReferencedMaterials(referencedMats);
}

void MixMaterial::AddReferencedTextures(boost::unordered_set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	matA->AddReferencedTextures(referencedTexs);
	matB->AddReferencedTextures(referencedTexs);
	mixFactor->AddReferencedTextures(referencedTexs);
}

void MixMaterial::UpdateTextureReferences(const Texture *oldTex, const Texture *newTex) {
	if (mixFactor == oldTex)
		mixFactor = newTex;

	Material::UpdateTextureReferences(oldTex, newTex);

	Preprocess();
}

Properties MixMaterial::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const  {
	Properties props;

	const string name = GetName();
	props.Set(Property("scene.materials." + name + ".type")("mix"));
	props.Set(Property("scene.materials." + name + ".material1")(matA->GetName()));
	props.Set(Property("scene.materials." + name + ".material2")(matB->GetName()));
	props.Set(Property("scene.materials." + name + ".amount")(mixFactor->GetSDLValue()));
	props.Set(Material::ToProperties(imgMapCache, useRealFileName));

	return props;
}
