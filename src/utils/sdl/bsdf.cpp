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

void BSDF::Init(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
	hitPoint.fromLight = fixedFromLight;
	hitPoint.passThroughEvent = u0;

	hitPoint.p = ray(rayHit.t);
	hitPoint.fixedDir = -ray.d;

	const u_int meshIndex = scene.dataSet->GetMeshID(rayHit.index);

	// Get the triangle
	mesh = scene.meshDefs.GetExtMesh(meshIndex);
	triIndex = scene.dataSet->GetMeshTriangleID(rayHit.index);

	// Get the material
	material = scene.objectMaterials[meshIndex];

	// Interpolate face normal
	hitPoint.geometryN = mesh->GetGeometryNormal(triIndex);
	hitPoint.shadeN = mesh->InterpolateTriNormal(triIndex, rayHit.b1, rayHit.b2);

	// Check if it is a light source
	if (material->IsLightSource())
		triangleLightSource = scene.triangleLights[rayHit.index];
	else
		triangleLightSource = NULL;

	// Interpolate UV coordinates
	hitPoint.uv = mesh->InterpolateTriUV(triIndex, rayHit.b1, rayHit.b2);

	// Check if I have to apply normal mapping
	if (material->HasNormalTex()) {
		// Apply normal mapping
		const Texture *nm = material->GetNormalTexture();
		const Spectrum color = nm->GetSpectrumValue(hitPoint);

		const float x = 2.f * color.r - 1.f;
		const float y = 2.f * color.g - 1.f;
		const float z = 2.f * color.b - 1.f;

		Vector v1, v2;
		CoordinateSystem(Vector(hitPoint.shadeN), &v1, &v2);
		hitPoint.shadeN = Normalize(Normal(
				v1.x * x + v2.x * y + hitPoint.shadeN.x * z,
				v1.y * x + v2.y * y + hitPoint.shadeN.y * z,
				v1.z * x + v2.z * y + hitPoint.shadeN.z * z));
	}

	// Check if I have to apply bump mapping
	if (material->HasBumpTex()) {
		// Apply bump mapping
		const Texture *bm = material->GetBumpTexture();
		const UV &dudv = bm->GetDuDv();

		const float b0 = bm->GetFloatValue(hitPoint);

		float dbdu;
		if (dudv.u > 0.f) {
			// This is a simple trick. The correct code would require true differential information.
			HitPoint tmpHitPoint = hitPoint;
			tmpHitPoint.p.x += dudv.u;
			tmpHitPoint.uv.u += dudv.u;
			const float bu = bm->GetFloatValue(tmpHitPoint);

			dbdu = (bu - b0) / dudv.u;
		} else
			dbdu = 0.f;

		float dbdv;
		if (dudv.v > 0.f) {
			// This is a simple trick. The correct code would require true differential information.
			HitPoint tmpHitPoint = hitPoint;
			tmpHitPoint.p.y += dudv.v;
			tmpHitPoint.uv.v += dudv.v;
			const float bv = bm->GetFloatValue(tmpHitPoint);

			dbdv = (bv - b0) / dudv.v;
		} else
			dbdv = 0.f;

		const Vector bump(dbdu, dbdv, 1.f);

		Vector v1, v2;
		CoordinateSystem(Vector(hitPoint.shadeN), &v1, &v2);
		hitPoint.shadeN = Normalize(Normal(
				v1.x * bump.x + v2.x * bump.y + hitPoint.shadeN.x * bump.z,
				v1.y * bump.x + v2.y * bump.y + hitPoint.shadeN.y * bump.z,
				v1.z * bump.x + v2.z * bump.y + hitPoint.shadeN.z * bump.z));
	}

	frame.SetFromZ(hitPoint.shadeN);
}

Spectrum BSDF::Evaluate(const Vector &generatedDir,
		BSDFEvent *event, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = hitPoint.fromLight ? generatedDir : hitPoint.fixedDir;
	const Vector &lightDir = hitPoint.fromLight ? hitPoint.fixedDir : generatedDir;

	const float dotLightDirNG = Dot(lightDir, hitPoint.geometryN);
	const float absDotLightDirNG = fabsf(dotLightDirNG);
	const float dotEyeDirNG = Dot(eyeDir, hitPoint.geometryN);
	const float absDotEyeDirNG = fabsf(dotEyeDirNG);

	if ((absDotLightDirNG < DEFAULT_COS_EPSILON_STATIC) ||
			(absDotEyeDirNG < DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	const float sideTest = dotEyeDirNG * dotLightDirNG;
	if (((sideTest > 0.f) && !(material->GetEventTypes() & REFLECT)) ||
			((sideTest < 0.f) && !(material->GetEventTypes() & TRANSMIT)))
		return Spectrum();

	const Vector localLightDir = frame.ToLocal(lightDir);
	const Vector localEyeDir = frame.ToLocal(eyeDir);
	const Spectrum result = material->Evaluate(hitPoint, localLightDir, localEyeDir,
			event, directPdfW, reversePdfW);

	// Adjoint BSDF
	if (hitPoint.fromLight) {
		const float absDotLightDirNS = AbsDot(lightDir, hitPoint.shadeN);
		const float absDotEyeDirNS = AbsDot(eyeDir, hitPoint.shadeN);
		return result * ((absDotLightDirNS * absDotEyeDirNG) / (absDotEyeDirNS * absDotLightDirNG));
	} else
		return result;
}

Spectrum BSDF::Sample(Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event) const {
	Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);
	Vector localSampledDir;

	Spectrum result = material->Sample(hitPoint,
			localFixedDir, &localSampledDir, u0, u1, hitPoint.passThroughEvent,
			pdfW, absCosSampledDir, event);
	if (result.Black())
		return result;

	*sampledDir = frame.ToWorld(localSampledDir);

	// Adjoint BSDF
	if (hitPoint.fromLight) {
		const float absDotFixedDirNS = fabsf(localFixedDir.z);
		const float absDotSampledDirNS = fabsf(localSampledDir.z);
		const float absDotFixedDirNG = AbsDot(hitPoint.fixedDir, hitPoint.geometryN);
		const float absDotSampledDirNG = AbsDot(*sampledDir, hitPoint.geometryN);
		return result * ((absDotFixedDirNS * absDotSampledDirNG) / (absDotSampledDirNS * absDotFixedDirNG));
	} else
		return result;
}

void BSDF::Pdf(const Vector &sampledDir, float *directPdfW, float *reversePdfW) const {
	const Vector &eyeDir = hitPoint.fromLight ? sampledDir : hitPoint.fixedDir;
	const Vector &lightDir = hitPoint.fromLight ? hitPoint.fixedDir : sampledDir;
	Vector localLightDir = frame.ToLocal(lightDir);
	Vector localEyeDir = frame.ToLocal(eyeDir);

	material->Pdf(hitPoint, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

Spectrum BSDF::GetEmittedRadiance(float *directPdfA, float *emissionPdfW) const {
	return triangleLightSource ? 
		triangleLightSource->GetRadiance(hitPoint, directPdfA, emissionPdfW) :
		Spectrum();
}

Spectrum BSDF::GetPassThroughTransparency() const {
	const Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);

	return material->GetPassThroughTransparency(hitPoint, localFixedDir, hitPoint.passThroughEvent);
}

} }
