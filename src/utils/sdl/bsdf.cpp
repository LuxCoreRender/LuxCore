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
	const RayHit &rayHit, const float passThroughEvent) {
	assert (!rayHit.Miss());

	Init(fixedFromLight, scene, ray(rayHit.t), ray.d, rayHit.index,
			rayHit.b1, rayHit.b2, passThroughEvent);
}

void BSDF::Init(const bool fixedFromLight, const Scene &scene, const Point &p, const Vector &incomingDir,
		const u_int triGlobalIndex, const float triangleBaryCoord1, const float triangleBaryCoord2,
		const float u0) {
	fromLight = fixedFromLight;
	passThroughEvent = u0;

	hitPoint = p;
	hitPointB1 = triangleBaryCoord1;
	hitPointB2 = triangleBaryCoord2;
	fixedDir = -incomingDir;

	const u_int meshIndex = scene.dataSet->GetMeshID(triGlobalIndex);

	// Get the triangle
	mesh = scene.meshDefs.GetExtMesh(meshIndex);
	triIndex = scene.dataSet->GetMeshTriangleID(triGlobalIndex);

	// Get the material
	material = scene.objectMaterials[meshIndex];

	// Interpolate face normal
	geometryN = mesh->GetGeometryNormal(triIndex);
	shadeN = mesh->InterpolateTriNormal(triIndex, hitPointB1, hitPointB2);

	// Check if it is a light source
	if (material->IsLightSource())
		triangleLightSource = scene.triangleLights[triGlobalIndex];
	else
		triangleLightSource = NULL;

	// Interpolate UV coordinates
	hitPointUV = mesh->InterpolateTriUV(triIndex, hitPointB1, hitPointB2);

	// Check if I have to apply normal mapping
	if (material->HasNormalTex()) {
		// Apply normal mapping
		const Texture *nm = material->GetNormalTexture();
		const Spectrum color = nm->GetColorValue(*this);

		const float x = 2.f * color.r - 1.f;
		const float y = 2.f * color.g - 1.f;
		const float z = 2.f * color.b - 1.f;

		Vector v1, v2;
		CoordinateSystem(Vector(shadeN), &v1, &v2);
		shadeN = Normalize(Normal(
				v1.x * x + v2.x * y + shadeN.x * z,
				v1.y * x + v2.y * y + shadeN.y * z,
				v1.z * x + v2.z * y + shadeN.z * z));
	}

	// Check if I have to apply bump mapping
	if (material->HasBumpTex()) {
		// Apply bump mapping
		const Texture *bm = material->GetBumpTexture();
		const UV &dudv = bm->GetDuDv();

		const float b0 = bm->GetGreyValue(*this);

		// This is a simple trick. The correct code would require differential information.
		BSDF tmpBsdf(*this);
		tmpBsdf.hitPointUV.u = hitPointUV.u + dudv.u;
		tmpBsdf.hitPointUV.v = hitPointUV.v;
		const float bu = bm->GetGreyValue(tmpBsdf);

		// This is a simple trick. The correct code would require differential information.
		tmpBsdf.hitPointUV.u = hitPointUV.u;
		tmpBsdf.hitPointUV.v = hitPointUV.v + dudv.u;
		const float bv = bm->GetGreyValue(tmpBsdf);

		// bumpScale is a fixed scale factor to try to more closely match
		// LuxRender bump mapping
		const float bumpScale = 50.f;
		const Vector bump(bumpScale * (bu - b0), bumpScale * (bv - b0), 1.f);

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
			((sideTest < 0.f) && !(material->GetEventTypes() & TRANSMIT)))
		return Spectrum();

	const Vector localLightDir = frame.ToLocal(lightDir);
	const Vector localEyeDir = frame.ToLocal(eyeDir);
	const Spectrum result = material->Evaluate(*this, localLightDir, localEyeDir,
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
		float *pdfW, float *absCosSampledDir, BSDFEvent *event) const {
	Vector localFixedDir = frame.ToLocal(fixedDir);
	Vector localSampledDir;

	Spectrum result = material->Sample(*this,
			localFixedDir, &localSampledDir, u0, u1, passThroughEvent,
			pdfW, absCosSampledDir, event);
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

	material->Pdf(*this, localLightDir, localEyeDir, directPdfW, reversePdfW);
}

Spectrum BSDF::GetEmittedRadiance(float *directPdfA, float *emissionPdfW) const {
	return triangleLightSource ? 
		triangleLightSource->GetRadiance(*this, directPdfA, emissionPdfW) :
		Spectrum();
}

Spectrum BSDF::GetPassThroughTransparency() const {
	const Vector localFixedDir = frame.ToLocal(fixedDir);

	return material->GetPassThroughTransparency(*this, localFixedDir, passThroughEvent);
}

} }
