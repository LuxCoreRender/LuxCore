/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <boost/lexical_cast.hpp>

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/material.h"

using namespace luxrays;
using namespace luxrays::sdl;

//------------------------------------------------------------------------------
// MaterialDefinitions
//------------------------------------------------------------------------------

MaterialDefinitions::MaterialDefinitions() { }

MaterialDefinitions::~MaterialDefinitions() {
	for (std::vector<Material *>::const_iterator it = mats.begin(); it != mats.end(); ++it)
		delete (*it);
}

void MaterialDefinitions::DefineMaterial(const std::string &name, Material *m) {
	if (IsMaterialDefined(name))
		throw std::runtime_error("Already defined material: " + name);

	mats.push_back(m);
	matsByName.insert(std::make_pair(name, m));
}

void MaterialDefinitions::UpdateMaterial(const std::string &name, Material *m) {
	if (!IsMaterialDefined(name))
		throw std::runtime_error("Can not update an undefined material: " + name);

	Material *oldMat = GetMaterial(name);

	// Update name/material definition
	const u_int index = GetMaterialIndex(name);
	mats[index] = m;
	matsByName.erase(name);
	matsByName.insert(std::make_pair(name, m));

	// Delete old material
	delete oldMat;

	// Update all possible reference to old material with the new one
	for (u_int i = 0; i < mats.size(); ++i)
		mats[i]->UpdateMaterialReference(oldMat, m);
}

Material *MaterialDefinitions::GetMaterial(const std::string &name) {
	// Check if the material has been already defined
	std::map<std::string, Material *>::const_iterator it = matsByName.find(name);

	if (it == matsByName.end())
		throw std::runtime_error("Reference to an undefined material: " + name);
	else
		return it->second;
}

u_int MaterialDefinitions::GetMaterialIndex(const std::string &name) {
	return GetMaterialIndex(GetMaterial(name));
}

u_int MaterialDefinitions::GetMaterialIndex(const Material *m) const {
	for (u_int i = 0; i < mats.size(); ++i) {
		if (m == mats[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined material: " + boost::lexical_cast<std::string>(m));
}

std::vector<std::string> MaterialDefinitions::GetMaterialNames() const {
	std::vector<std::string> names;
	names.reserve(mats.size());
	for (std::map<std::string, Material *>::const_iterator it = matsByName.begin(); it != matsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void MaterialDefinitions::DeleteMaterial(const std::string &name) {
	const u_int index = GetMaterialIndex(name);
	mats.erase(mats.begin() + index);
	matsByName.erase(name);
}

//------------------------------------------------------------------------------
// Matte material
//------------------------------------------------------------------------------

Spectrum MatteMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((fromLight ? eyeDir.z : lightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((fromLight ? lightDir.z : eyeDir.z) * INV_PI);

	*event = DIFFUSE | REFLECT;
	return Kd->GetColorValue(uv) * INV_PI;
}

Spectrum MatteMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	if (fabsf(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*sampledDir = Sgn(fixedDir.z) * CosineSampleHemisphere(u0, u1, pdfW);

	*cosSampledDir = fabsf(sampledDir->z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*event = DIFFUSE | REFLECT;
	return Kd->GetColorValue(uv) * INV_PI;
}

void MatteMaterial::Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((fromLight ? eyeDir.z : lightDir.z) * INV_PI);

	if (reversePdfW)
		*reversePdfW = fabsf((fromLight ? lightDir.z : eyeDir.z) * INV_PI);
}

void MatteMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kd);
}

//------------------------------------------------------------------------------
// Mirror material
//------------------------------------------------------------------------------

Spectrum MirrorMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum MirrorMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	*event = SPECULAR | REFLECT;

	*sampledDir = Vector(-fixedDir.x, -fixedDir.y, fixedDir.z);
	*pdfW = 1.f;

	*cosSampledDir = fabsf(sampledDir->z);
	// The cosSampledDir is used to compensate the other one used inside the integrator
	return Kr->GetColorValue(uv) / (*cosSampledDir);
}

void MirrorMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kr);
}

//------------------------------------------------------------------------------
// Glass material
//------------------------------------------------------------------------------

Spectrum GlassMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum GlassMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);
	const Vector N(0.f, 0.f, 1.f);

	const Vector rayDir = -fixedDir;
	const Vector reflDir = rayDir - (2.f * Dot(N, rayDir)) * Vector(N);

	const float nc = ousideIor->GetGreyValue(uv);
	const float nt = ior->GetGreyValue(uv);
	const float nnt = into ? (nc / nt) : (nt / nc);
	const float nnt2 = nnt * nnt;
	const float ddn = Dot(rayDir, shadeN);
	const float cos2t = 1.f - nnt2 * (1.f - ddn * ddn);

	// Total internal reflection
	if (cos2t < 0.f) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = 1.f;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kr->GetColorValue(uv) / (*cosSampledDir);
	}

	const float kk = (into ? 1.f : -1.f) * (ddn * nnt + sqrtf(cos2t));
	const Vector nkk = kk * Vector(N);
	const Vector transDir = Normalize(nnt * rayDir - nkk);

	const float c = 1.f - (into ? -ddn : Dot(transDir, N));
	const float c2 = c * c;
	const float a = nt - nc;
	const float b = nt + nc;
	const float R0 = a * a / (b * b);
	const float Re = R0 + (1.f - R0) * c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return Spectrum();
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabsf(sampledDir->z);
			*pdfW = 1.f;

			// The cosSampledDir is used to compensate the other one used inside the integrator
			return Kr->GetColorValue(uv) / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = 1.f;

		if (fromLight)
			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
		else
			return Kt->GetColorValue(uv) / (*cosSampledDir);
	} else if (u0 < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = P / Re;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kr->GetColorValue(uv) / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		if (fromLight)
			return Kt->GetColorValue(uv) * (nnt2 / (*cosSampledDir));
		else
			return Kt->GetColorValue(uv) / (*cosSampledDir);
	}
}

void GlassMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kr);
	referencedTexs.insert(Kt);
	referencedTexs.insert(ousideIor);
	referencedTexs.insert(ior);
}

//------------------------------------------------------------------------------
// Architectural glass material
//------------------------------------------------------------------------------

Spectrum ArchGlassMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum ArchGlassMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);
	const Vector N(0.f, 0.f, 1.f);

	const Vector rayDir = -fixedDir;
	const Vector reflDir = rayDir - (2.f * Dot(N, rayDir)) * Vector(N);

	const float ddn = Dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrtf(cos2t));
	const Vector nkk = kk * Vector(N);
	const Vector transDir = Normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : Dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f) {
		if (Re == 0.f)
			return Spectrum();
		else {
			*event = SPECULAR | REFLECT;
			*sampledDir = reflDir;
			*cosSampledDir = fabsf(sampledDir->z);
			*pdfW = 1.f;

			// The cosSampledDir is used to compensate the other one used inside the integrator
			return Kr->GetColorValue(uv) / (*cosSampledDir);
		}
	} else if (Re == 0.f) {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = 1.f;

		return Kt->GetColorValue(uv) / (*cosSampledDir);
	} else if (passThroughEvent < P) {
		*event = SPECULAR | REFLECT;
		*sampledDir = reflDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = P / Re;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kr->GetColorValue(uv) / (*cosSampledDir);
	} else {
		*event = SPECULAR | TRANSMIT;
		*sampledDir = transDir;
		*cosSampledDir = fabsf(sampledDir->z);
		*pdfW = (1.f - P) / Tr;

		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kt->GetColorValue(uv) / (*cosSampledDir);
	}
}

Spectrum ArchGlassMaterial::GetPassThroughTransparency(const UV &uv,
		const Vector &fixedDir, const float passThroughEvent) const {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);

	// TODO: remove
	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);
	const Vector N(0.f, 0.f, 1.f);

	const Vector rayDir = -fixedDir;

	const float ddn = Dot(rayDir, shadeN);
	const float cos2t = ddn * ddn;

	// Total internal reflection is not possible
	const float kk = (into ? 1.f : -1.f) * (ddn + sqrtf(cos2t));
	const Vector nkk = kk * Vector(N);
	const Vector transDir = Normalize(rayDir - nkk);

	const float c = 1.f - (into ? -ddn : Dot(transDir, N));
	const float c2 = c * c;
	const float Re = c2 * c2 * c;
	const float Tr = 1.f - Re;
	const float P = .25f + .5f * Re;

	if (Tr == 0.f)
		return Spectrum();
	else if (Re == 0.f)
		return Kt->GetColorValue(uv);
	else if (passThroughEvent < P)
		return Spectrum();
	else
		return Kt->GetColorValue(uv);
}

void ArchGlassMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kr);
	referencedTexs.insert(Kt);
}

//------------------------------------------------------------------------------
// Metal material
//------------------------------------------------------------------------------

Spectrum MetalMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Vector MetalMaterial::GlossyReflection(const Vector &fixedDir, const float exponent,
		const float u0, const float u1) {
	// Ray from outside going in ?
	const bool into = (fixedDir.z > 0.f);
	const Vector shadeN(0.f, 0.f, into ? 1.f : -1.f);

	const float phi = 2.f * M_PI * u0;
	const float cosTheta = powf(1.f - u1, exponent);
	const float sinTheta = sqrtf(Max(0.f, 1.f - cosTheta * cosTheta));
	const float x = cosf(phi) * sinTheta;
	const float y = sinf(phi) * sinTheta;
	const float z = cosTheta;

	const Vector dir = -fixedDir;
	const float dp = Dot(shadeN, dir);
	const Vector w = dir - (2.f * dp) * Vector(shadeN);

	const Vector u = Normalize(Cross(
			(fabsf(shadeN.x) > .1f) ? Vector(0.f, 1.f, 0.f) :  Vector(1.f, 0.f, 0.f),
			w));
	const Vector v = Cross(w, u);

	return x * u + y * v + z * w;
}

Spectrum MetalMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	const float e = 1.f / (exponent->GetGreyValue(uv) + 1.f);
	*sampledDir = GlossyReflection(fixedDir, e, u0, u1);

	if (sampledDir->z * fixedDir.z > 0.f) {
		*event = SPECULAR | REFLECT;
		*pdfW = 1.f;
		*cosSampledDir = fabsf(sampledDir->z);
		// The cosSampledDir is used to compensate the other one used inside the integrator
		return Kr->GetColorValue(uv) / (*cosSampledDir);
	} else
		return Spectrum();
}

void MetalMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kr);
	referencedTexs.insert(exponent);
}

//------------------------------------------------------------------------------
// Mix material
//------------------------------------------------------------------------------

Spectrum MixMaterial::GetPassThroughTransparency(const UV &uv, const Vector &fixedDir,
		const float passThroughEvent) const {
	const float weight2 = Clamp(mixFactor->GetGreyValue(uv), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (passThroughEvent < weight1)
		return matA->GetPassThroughTransparency(uv, fixedDir, passThroughEvent / weight1);
	else
		return matB->GetPassThroughTransparency(uv, fixedDir, (passThroughEvent - weight2) / weight2);
}

Spectrum MixMaterial::GetEmittedRadiance(const UV &uv) const {
	Spectrum result;

	const float weight2 = Clamp(mixFactor->GetGreyValue(uv), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (matA->IsLightSource() && (weight1 > 0.f))
		result += weight1 * matA->GetEmittedRadiance(uv);
	if (matB->IsLightSource() && (weight2 > 0.f))
		result += weight2 * matB->GetEmittedRadiance(uv);

	return result;
}

Spectrum MixMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	Spectrum result;

	const float weight2 = Clamp(mixFactor->GetGreyValue(uv), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	BSDFEvent eventMatA = NONE;
	float directPdfWMatA = 1.f;
	float reversePdfWMatA = 1.f;
	if (weight1 > 0.f)
		result += weight1 * matA->Evaluate(fromLight, uv, lightDir, eyeDir, &eventMatA, &directPdfWMatA, &reversePdfWMatA);

	BSDFEvent eventMatB = NONE;
	float directPdfWMatB = 1.f;
	float reversePdfWMatB = 1.f;
	if (weight2 > 0.f)
		result += weight2 * matB->Evaluate(fromLight, uv, lightDir, eyeDir, &eventMatB, &directPdfWMatB, &reversePdfWMatB);

	*event = eventMatA | eventMatB;

	if (directPdfW)
		*directPdfW = weight1 * directPdfWMatA + weight2 *directPdfWMatB;
	if (reversePdfW)
		*reversePdfW = weight1 * reversePdfWMatA + weight2 * reversePdfWMatB;

	return result;
}

Spectrum MixMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	const float weight2 = Clamp(mixFactor->GetGreyValue(uv), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	if (passThroughEvent < weight1) {
		// Sample the first material
		Spectrum result = matA->Sample(fromLight, uv, fixedDir, sampledDir,
				u0, u1, passThroughEvent / weight1, pdfW, cosSampledDir, event);
		if (result.Black())
			return Spectrum();
		*pdfW *= weight1;

		// Evaluate the second material
		BSDFEvent eventMatA;
		if (fromLight) {
			float pdfWMatB;
			Spectrum f = matB->Evaluate(fromLight, uv, fixedDir, *sampledDir, &eventMatA, &pdfWMatB);
			if (!f.Black()) {
				result += weight2 * f;
				*pdfW += weight2 * pdfWMatB;
			}
		} else {
			float pdfWMatB;
			Spectrum f = matB->Evaluate(fromLight, uv, *sampledDir, fixedDir, &eventMatA, &pdfWMatB);
			if (!f.Black()) {
				result += weight2 * f;
				*pdfW += weight2 * pdfWMatB;
			}
		}
		
		return result;
	} else {
		// Sample the second material
		Spectrum result = matB->Sample(fromLight, uv, fixedDir, sampledDir,
				u0, u1, (passThroughEvent - weight1) / weight2, pdfW, cosSampledDir, event);
		if (result.Black())
			return Spectrum();
		*pdfW *= weight2;

		// Evaluate the first material
		BSDFEvent eventMatA;
		if (fromLight) {
			float pdfWMatA;
			Spectrum f = matA->Evaluate(fromLight, uv, fixedDir, *sampledDir, &eventMatA, &pdfWMatA);
			if (!f.Black()) {
				result += weight1 * f;
				*pdfW += weight1 * pdfWMatA;
			}
		} else {
			float pdfWMatA;
			Spectrum f = matA->Evaluate(fromLight, uv, *sampledDir, fixedDir, &eventMatA, &pdfWMatA);
			if (!f.Black()) {
				result += weight1 * f;
				*pdfW += weight1 * pdfWMatA;
			}
		}

		return result;
	}
}

void MixMaterial::Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
	const float weight2 = Clamp(mixFactor->GetGreyValue(uv), 0.f, 1.f);
	const float weight1 = 1.f - weight2;

	float directPdfWMatA = 1.f;
	float reversePdfWMatA = 1.f;
	if (weight1 > 0.f)
		matA->Pdf(fromLight, uv, lightDir, eyeDir, &directPdfWMatA, &reversePdfWMatA);

	float directPdfWMatB = 1.f;
	float reversePdfWMatB = 1.f;
	if (weight2 > 0.f)
		matB->Pdf(fromLight, uv, lightDir, eyeDir, &directPdfWMatB, &reversePdfWMatB);

	if (directPdfW)
		*directPdfW = weight1 * directPdfWMatA + weight2 *directPdfWMatB;
	if (reversePdfW)
		*reversePdfW = weight1 * reversePdfWMatA + weight2 * reversePdfWMatB;
}

void MixMaterial::UpdateMaterialReference(const Material *oldMat,  const Material *newMat) {
	if (matA == oldMat)
		matA = newMat;

	if (matB == oldMat)
		matB = newMat;
}

bool MixMaterial::IsReferencing(const Material *mat) const {
	if (matA == mat)
		return true;
	if (matB == mat)
		return true;

	const MixMaterial *mixA = dynamic_cast<const MixMaterial *>(matA);
	if (mixA && mixA->IsReferencing(mat))
		return true;
	const MixMaterial *mixB = dynamic_cast<const MixMaterial *>(matB);
	if (mixB && mixB->IsReferencing(mat))
		return true;

	return false;
}

void MixMaterial::AddReferencedMaterials(std::set<const Material *> &referencedMats) const {
	Material::AddReferencedMaterials(referencedMats);

	referencedMats.insert(matA);
	matA->AddReferencedMaterials(referencedMats);

	referencedMats.insert(matB);
	matB->AddReferencedMaterials(referencedMats);
}

void MixMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	matA->AddReferencedTextures(referencedTexs);
	matB->AddReferencedTextures(referencedTexs);
	referencedTexs.insert(mixFactor);
}

//------------------------------------------------------------------------------
// Null material
//------------------------------------------------------------------------------

Spectrum NullMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	return Spectrum();
}

Spectrum NullMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	//throw std::runtime_error("Internal error, called NullMaterial::Sample()");

	*sampledDir = -fixedDir;
	*cosSampledDir = 1.f;

	*pdfW = 1.f;
	*event = SPECULAR | TRANSMIT;
	return Spectrum(1.f);
}

//------------------------------------------------------------------------------
// MatteTranslucent material
//------------------------------------------------------------------------------

Spectrum MatteTranslucentMaterial::Evaluate(const bool fromLight, const UV &uv,
	const Vector &lightDir, const Vector &eyeDir, BSDFEvent *event,
	float *directPdfW, float *reversePdfW) const {
	const float cosSampledDir = Dot(lightDir, eyeDir);
	if (fabsf(cosSampledDir) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	const Spectrum r = Kr->GetColorValue(uv);
	const Spectrum t = Kt->GetColorValue(uv) * 
		// Energy conservation
		(Spectrum(1.f) - r);

	if (directPdfW)
		*directPdfW = .5f * fabsf((fromLight ? eyeDir.z : lightDir.z) * (.5f * INV_PI));

	if (reversePdfW)
		*reversePdfW = .5f * fabsf((fromLight ? lightDir.z : eyeDir.z) * (.5f * INV_PI));

	if (cosSampledDir > 0.f) {
		*event = DIFFUSE | REFLECT;
		return r * INV_PI;
	} else {
		*event = DIFFUSE | TRANSMIT;
		return t * INV_PI;
	}
}

Spectrum MatteTranslucentMaterial::Sample(const bool fromLight, const UV &uv,
	const Vector &fixedDir, Vector *sampledDir,
	const float u0, const float u1,  const float passThroughEvent,
	float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	if (fabsf(fixedDir.z) < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*sampledDir = CosineSampleHemisphere(u0, u1, pdfW);
	*cosSampledDir = fabsf(sampledDir->z);
	if (*cosSampledDir < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	*pdfW *= .5f;

	const Spectrum r = Kr->GetColorValue(uv);
	const Spectrum t = Kt->GetColorValue(uv) * 
		// Energy conservation
		(Spectrum(1.f) - r);

	if (passThroughEvent < .5f) {
		*sampledDir *= Sgn(fixedDir.z);
		*event = DIFFUSE | REFLECT;
		return r * INV_PI;
	} else {
		*sampledDir *= -Sgn(fixedDir.z);
		*event = DIFFUSE | TRANSMIT;
		return t * INV_PI;
	}
}

void MatteTranslucentMaterial::Pdf(const bool fromLight, const UV &uv,
		const Vector &lightDir, const Vector &eyeDir,
		float *directPdfW, float *reversePdfW) const {
	if (directPdfW)
		*directPdfW = fabsf((fromLight ? eyeDir.z : lightDir.z) * (.5f * INV_PI));

	if (reversePdfW)
		*reversePdfW = fabsf((fromLight ? lightDir.z : eyeDir.z) * (.5f * INV_PI));
}

void MatteTranslucentMaterial::AddReferencedTextures(std::set<const Texture *> &referencedTexs) const {
	Material::AddReferencedTextures(referencedTexs);

	referencedTexs.insert(Kr);
	referencedTexs.insert(Kt);
}
