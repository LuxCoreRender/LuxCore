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

#include "luxrays/utils/sdl/bsdf.h"
#include "luxrays/utils/sdl/scene.h"

namespace luxrays { namespace sdl {

void BSDF::Init(const bool fromL, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
	assert (!rayHit.Miss());

	fromLight = fromL;
	passThroughEvent = u0;

	hitPoint = ray(rayHit.t);
	hitPointB1 = rayHit.b1;
	hitPointB2 = rayHit.b2;
	fixedDir = -ray.d;

	const u_int currentTriangleIndex = rayHit.index;
	const u_int currentMeshIndex = scene.dataSet->GetMeshID(currentTriangleIndex);

	// Get the triangle
	mesh = scene.meshDefs.GetExtMesh(currentMeshIndex);
	triIndex = scene.dataSet->GetMeshTriangleID(currentTriangleIndex);

	// Get the material
	material = scene.objectMaterials[currentMeshIndex];

	// Interpolate face normal
	geometryN = mesh->GetGeometryNormal(triIndex);
	shadeN = mesh->InterpolateTriNormal(triIndex, rayHit.b1, rayHit.b2);

	// Check if it is a light source
	if (material->IsLightSource())
		triangleLightSource = scene.triangleLights[currentTriangleIndex];
	else
		triangleLightSource = NULL;

	// Interpolate UV coordinates
	hitPointUV = mesh->InterpolateTriUV(triIndex, rayHit.b1, rayHit.b2);

	// Check if I have to apply bump mapping
	if (material->HasNormalTex()) {
		// Apply normal mapping
		const Texture *nm = material->GetNormalTexture();
		const Spectrum color = nm->GetColorValue(hitPointUV);

		const float x = 2.f * (color.r - 0.5f);
		const float y = 2.f * (color.g - 0.5f);
		const float z = 2.f * (color.b - 0.5f);

		Vector v1, v2;
		CoordinateSystem(Vector(shadeN), &v1, &v2);
		shadeN = Normalize(Normal(
				v1.x * x + v2.x * y + shadeN.x * z,
				v1.y * x + v2.y * y + shadeN.y * z,
				v1.z * x + v2.z * y + shadeN.z * z));
	}

	// Check if I have to apply normal mapping
	if (material->HasBumpTex()) {
		// Apply normal mapping
		const Texture *bm = material->GetBumpTexture();
		const UV &dudv = bm->GetDuDv();

		const float b0 = bm->GetGreyValue(hitPointUV);

		const UV uvdu(hitPointUV.u + dudv.u, hitPointUV.v);
		const float bu = bm->GetGreyValue(uvdu);

		const UV uvdv(hitPointUV.u, hitPointUV.v + dudv.v);
		const float bv = bm->GetGreyValue(uvdv);

		const Vector bump(bu - b0, bv - b0, 1.f);

		Vector v1, v2;
		CoordinateSystem(Vector(shadeN), &v1, &v2);
		shadeN = Normalize(Normal(
				v1.x * bump.x + v2.x * bump.y + shadeN.x * bump.z,
				v1.y * bump.x + v2.y * bump.y + shadeN.y * bump.z,
				v1.z * bump.x + v2.z * bump.y + shadeN.z * bump.z));
	}

	frame.SetFromZ(shadeN);
}

Spectrum BSDF::Evaluate(const Vector &generatedDir,
		BSDFEvent *event, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = fromLight ? generatedDir : fixedDir;
	const Vector &lightDir = fromLight ? fixedDir : generatedDir;

	const float dotLightDirNG = Dot(lightDir, geometryN);
	const float absDotLightDirNG = fabsf(dotLightDirNG);
	const float dotEyeDirNG = Dot(eyeDir, geometryN);
	const float absDotEyeDirNG = fabsf(dotEyeDirNG);

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	const float sideTest = dotEyeDirNG * dotLightDirNG;
	if (((sideTest > 0.f) && !(material->GetEventTypes() & REFLECT)) ||
			((sideTest < 0.f) && !(material->GetEventTypes() & TRANSMIT)) ||
			(sideTest == 0.f))
		return Spectrum();

	Vector localLightDir = frame.ToLocal(lightDir);
	Vector localEyeDir = frame.ToLocal(eyeDir);
	Spectrum result = material->Evaluate(fromLight, hitPointUV, localLightDir, localEyeDir,
			event, directPdfW, reversePdfW);

	// Adjoint BSDF
	if (fromLight) {
		const float absDotLightDirNS = AbsDot(lightDir, shadeN);
		const float absDotEyeDirNS = AbsDot(eyeDir, shadeN);
		return result * ((absDotLightDirNS * absDotEyeDirNG) / (absDotEyeDirNS * absDotLightDirNG));
	} else
		return result;
}

Spectrum BSDF::Sample(Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *cosSampledDir, BSDFEvent *event) const {
	Vector localFixedDir = frame.ToLocal(fixedDir);
	Vector localSampledDir;

	Spectrum result = material->Sample(fromLight, hitPointUV,
			localFixedDir, &localSampledDir, u0, u1, passThroughEvent,
			pdfW, cosSampledDir, event);
	if (result.Black())
		return result;

	*sampledDir = frame.ToWorld(localSampledDir);

	// Adjoint BSDF
	if (fromLight) {
		const float absDotFixedDirNS = fabsf(localFixedDir.z);
		const float absDotSampledDirNS = fabsf(localSampledDir.z);
		const float absDotFixedDirNG = AbsDot(fixedDir, geometryN);
		const float absDotSampledDirNG = AbsDot(*sampledDir, geometryN);
		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
	} else
		return result;
}

void BSDF::Pdf(const Vector &sampledDir, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = fromLight ? sampledDir : fixedDir;
	const Vector &lightDir = fromLight ? fixedDir : sampledDir;
	Vector localLightDir = frame.ToLocal(lightDir);
	Vector localEyeDir = frame.ToLocal(eyeDir);

	material->Pdf(fromLight, hitPointUV, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

Spectrum BSDF::GetEmittedRadiance(const Scene *scene,
			float *directPdfA,
			float *emissionPdfW) const {
	return triangleLightSource ? 
		triangleLightSource->GetRadiance(scene, fixedDir, hitPointB1, hitPointB2, directPdfA, emissionPdfW) :
		Spectrum();
}

} }
