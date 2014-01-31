/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#include "slg/sdl/bsdf.h"
#include "slg/sdl/scene.h"

using namespace luxrays;
using namespace slg;

void BSDF::Init(const bool fixedFromLight, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
	hitPoint.fromLight = fixedFromLight;
	hitPoint.passThroughEvent = u0;

	hitPoint.p = ray(rayHit.t);
	hitPoint.fixedDir = -ray.d;

	// Get the triangle
	mesh = scene.objDefs.GetSceneObject(rayHit.meshIndex)->GetExtMesh();

	// Get the material
	material = scene.objDefs.GetSceneObject(rayHit.meshIndex)->GetMaterial();

	// Interpolate face normal
	hitPoint.geometryN = mesh->GetGeometryNormal(rayHit.triangleIndex);
	hitPoint.shadeN = mesh->InterpolateTriNormal(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Interpolate color
	hitPoint.color = mesh->InterpolateTriColor(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Interpolate alpha
	hitPoint.alpha = mesh->InterpolateTriAlpha(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Check if it is a light source
	if (material->IsLightSource())
		triangleLightSource = scene.lightDefs.GetLightSourceByMeshIndex(rayHit.meshIndex);
	else
		triangleLightSource = NULL;

	// Interpolate UV coordinates
	hitPoint.uv = mesh->InterpolateTriUV(rayHit.triangleIndex, rayHit.b1, rayHit.b2);

	// Check if I have to apply normal mapping
	if (material->HasNormalTex()) {
		// Apply normal mapping
		const Spectrum color = material->GetNormalTexValue(hitPoint);

		const float x = 2.f * color.c[0] - 1.f;
		const float y = 2.f * color.c[1] - 1.f;
		const float z = 2.f * color.c[2] - 1.f;

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
		const UV uv = material->GetBumpTexValue(hitPoint);

		Vector v1, v2;
		CoordinateSystem(Vector(hitPoint.shadeN), &v1, &v2);
		hitPoint.shadeN = Normalize(Normal(
				v1.x * uv.u + v2.x * uv.v + hitPoint.shadeN.x,
				v1.y * uv.u + v2.y * uv.v + hitPoint.shadeN.y,
				v1.z * uv.u + v2.z * uv.v + hitPoint.shadeN.z));
	}

    // Build the local reference system
    mesh->GetTriangleFrame(rayHit.triangleIndex, hitPoint.shadeN, frame);
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
	if (hitPoint.fromLight)
		return result * (absDotEyeDirNG / absDotLightDirNG);
	else
		return result;
}

Spectrum BSDF::Sample(Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent) const {
	Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);
	Vector localSampledDir;

	Spectrum result = material->Sample(hitPoint,
			localFixedDir, &localSampledDir, u0, u1, hitPoint.passThroughEvent,
			pdfW, absCosSampledDir, event, requestedEvent);
	if (result.Black())
		return result;

	*sampledDir = frame.ToWorld(localSampledDir);

	// Adjoint BSDF
	if (hitPoint.fromLight) {
		const float absDotFixedDirNG = AbsDot(hitPoint.fixedDir, hitPoint.geometryN);
		const float absDotSampledDirNG = AbsDot(*sampledDir, hitPoint.geometryN);
		return result * (absDotSampledDirNG / absDotFixedDirNG);
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

Spectrum BSDF::GetPassThroughTransparency() const {
	const Vector localFixedDir = frame.ToLocal(hitPoint.fixedDir);

	return material->GetPassThroughTransparency(hitPoint, localFixedDir, hitPoint.passThroughEvent);
}

Spectrum BSDF::GetEmittedRadiance(float *directPdfA, float *emissionPdfW) const {
	return triangleLightSource ? 
		triangleLightSource->GetRadiance(hitPoint, directPdfA, emissionPdfW) :
		Spectrum();
}
