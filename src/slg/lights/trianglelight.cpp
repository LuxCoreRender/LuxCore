/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include "slg/bsdf/bsdf.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/lights/trianglelight.h"
#include "slg/scene/sceneobject.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight() : sceneObject(NULL), 
		meshIndex(NULL_INDEX), triangleIndex(NULL_INDEX),
		triangleArea(0.f), invTriangleArea(0.f),
		meshArea(0.f), invMeshArea(0.f) {
}

TriangleLight::~TriangleLight() {
}

bool TriangleLight::IsDirectLightSamplingEnabled() const {
	switch (lightMaterial->GetDirectLightSamplingType()) {
		case DLS_AUTO: {
			// Check the number of triangles and disable direct light sampling for mesh
			// with too many elements
			return (sceneObject->GetExtMesh()->GetTotalTriangleCount() > 256) ? false : true;
		}
		case DLS_ENABLED:
			return true;
		case DLS_DISABLED:
			return false;
		default:
			throw runtime_error("Unknown material emission direct sampling type: " + ToString(lightMaterial->GetDirectLightSamplingType()));
	}
}

float TriangleLight::GetPower(const Scene &scene) const {
	const float emittedRadianceY = lightMaterial->GetEmittedRadianceY(invMeshArea);

	if (lightMaterial->GetEmittedTheta() == 0.f)
		return triangleArea * emittedRadianceY;
	else if (lightMaterial->GetEmittedTheta() < 90.f)
		return triangleArea * (2.f * M_PI) * (1.f - lightMaterial->GetEmittedCosThetaMax()) * emittedRadianceY;
	else
		return triangleArea * M_PI * emittedRadianceY;
}

void TriangleLight::Preprocess() {
	const ExtMesh *mesh = sceneObject->GetExtMesh();

	Transform localToWorld;
	mesh->GetLocal2World(0.f, localToWorld);

	triangleArea = mesh->GetTriangleArea(localToWorld, triangleIndex);
	invTriangleArea = 1.f / triangleArea;

	meshArea = mesh->GetMeshArea(localToWorld);
	invMeshArea = 1.f / meshArea;
}

Spectrum TriangleLight::Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		Ray &ray, float &emissionPdfW,
		float *directPdfA, float *cosThetaAtLight) const {
	// A safety check to avoid NaN/Inf
	if ((triangleArea == 0.f) || (meshArea == 0.f))
		return Spectrum();

	Spectrum emissionColor(1.f);
	Vector localDirOut;
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();
	if (emissionFunc) {
		emissionFunc->Sample(u2, u3, &localDirOut, &emissionPdfW);
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localDirOut) / emissionFunc->Average();
	} else {
		if (lightMaterial->GetEmittedTheta() == 0.f) {
			localDirOut = Vector(0.f, 0.f, 1.f);
			emissionPdfW = 1.f;
		} else if (lightMaterial->GetEmittedTheta() < 90.f) {
			const float cosThetaMax = lightMaterial->GetEmittedCosThetaMax();
			localDirOut = UniformSampleCone(u2, u3, cosThetaMax);
			emissionPdfW = UniformConePdf(cosThetaMax);
		} else
			localDirOut = CosineSampleHemisphere(u2, u3, &emissionPdfW);

		// Cannot really not emit the particle, so just bias it to the correct angle
		localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);
	}

	if (emissionPdfW == 0.f)
		return Spectrum();
	emissionPdfW *= invTriangleArea;

	const ExtMesh *mesh = sceneObject->GetExtMesh();

	// Build a temporary HitPoint
	HitPoint tmpHitPoint;
	mesh->GetLocal2World(time, tmpHitPoint.localToWorld);

	// Origin
	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(tmpHitPoint.localToWorld, triangleIndex, u0, u1, &samplePoint, &b0, &b1, &b2);

	// Initialize the temporary HitPoint
	tmpHitPoint.Init(true, false,
			scene, meshIndex, triangleIndex,
			samplePoint, Vector(mesh->GetGeometryNormal(tmpHitPoint.localToWorld, triangleIndex)),
			b1, b2,
			passThroughEvent);
	// Add bump?
	// lightMaterial->Bump(&hitPoint, 1.f);

	const Frame frame(tmpHitPoint.GetFrame());

	// Ray direction
	const Vector rayDir = frame.ToWorld(localDirOut);

	// Ray origin
	const Point rayOrig = tmpHitPoint.p + Vector(tmpHitPoint.geometryN * MachineEpsilon::E(tmpHitPoint.p)) *
			// With an IES/map I can emit from the backface too so I have to
			// use rayDir here instead of tmpHitPoint.intoObject
			((Dot(rayDir, tmpHitPoint.geometryN) > 0.f) ? 1.f : -1.f);

	if (directPdfA)
		*directPdfA = invTriangleArea;

	if (cosThetaAtLight)
		*cosThetaAtLight = fabsf(localDirOut.z);

	ray.Update(rayOrig, rayDir, time);

	return lightMaterial->GetEmittedRadiance(tmpHitPoint, invMeshArea) * emissionColor * fabsf(localDirOut.z);
}

Spectrum TriangleLight::Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        Ray &shadowRay, float &directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	// A safety check to avoid NaN/Inf
	if ((triangleArea == 0.f) || (meshArea == 0.f))
		return Spectrum();

	//--------------------------------------------------------------------------
	// Compute the sample point and direction
	//--------------------------------------------------------------------------

	const ExtMesh *mesh = sceneObject->GetExtMesh();

	HitPoint tmpHitPoint;
	mesh->GetLocal2World(time, tmpHitPoint.localToWorld);

	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(tmpHitPoint.localToWorld, triangleIndex, u0, u1, &samplePoint, &b0, &b1, &b2);

	Vector sampleDir = samplePoint - bsdf.hitPoint.p;
	const float distanceSquared = sampleDir.LengthSquared();
	const float distance = sqrtf(distanceSquared);
	sampleDir /= distance;
	
	// Initialize the temporary HitPoint
	tmpHitPoint.Init(true, false,
			scene, meshIndex, triangleIndex,
			samplePoint, -sampleDir,
			b1, b2,
			passThroughEvent);
	// Add bump?
	// lightMaterial->Bump(&hitPoint, 1.f);

	const float cosAtLight = Dot(tmpHitPoint.geometryN, -sampleDir);
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();

	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if (!emissionFunc && (cosAtLight < lightMaterial->GetEmittedCosThetaMax() + DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = fabsf(cosAtLight);

	//--------------------------------------------------------------------------
	// Initialize the shadow ray
	//--------------------------------------------------------------------------
	
	// Move shadow ray origin along the geometry normal by an epsilon to avoid self-shadow problems
	const Point shadowRayOrig = bsdf.GetRayOrigin(sampleDir);

	// Compute shadow ray direction with displaced start and end point to avoid self-shadow problems
	Vector shadowRayDir = tmpHitPoint.p + Vector(tmpHitPoint.geometryN * MachineEpsilon::E(tmpHitPoint.p)) *
			(tmpHitPoint.intoObject ? 1.f : -1.f) - shadowRayOrig;
	const float shadowRayDistance = shadowRayDir.Length();
	shadowRayDir /= shadowRayDistance;

	//--------------------------------------------------------------------------
	// Compute the PDF and emission color
	//--------------------------------------------------------------------------

	Spectrum emissionColor(1.f);
	if (emissionFunc) {
		// Add bump?
		// lightMaterial->Bump(&hitPoint, 1.f);

		const Frame frame(tmpHitPoint.GetFrame());

		const Vector localFromLight = Normalize(frame.ToLocal(-sampleDir));
		
		if (emissionPdfW) {
			const float emissionFuncPdf = emissionFunc->Pdf(localFromLight);
			if (emissionFuncPdf == 0.f)
				return Spectrum();
			*emissionPdfW = emissionFuncPdf * invTriangleArea;
		}
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localFromLight) / emissionFunc->Average();
		
		directPdfW = invTriangleArea * distanceSquared;
	} else {
		if (emissionPdfW) {
			if (lightMaterial->GetEmittedTheta() == 0.f)
				*emissionPdfW = invTriangleArea;
			else if (lightMaterial->GetEmittedTheta() < 90.f)
				*emissionPdfW = invTriangleArea * UniformConePdf(lightMaterial->GetEmittedCosThetaMax());
			else
				*emissionPdfW = invTriangleArea * fabsf(cosAtLight) * INV_PI;
		}

		directPdfW = invTriangleArea * distanceSquared / fabsf(cosAtLight);
	}

	assert (!isnan(directPdfW) && !isinf(directPdfW));
	
	shadowRay = Ray(shadowRayOrig, shadowRayDir, 0.f, shadowRayDistance, time);

	return lightMaterial->GetEmittedRadiance(tmpHitPoint, invMeshArea) * emissionColor;
}

bool TriangleLight::IsAlwaysInShadow(const Scene &scene,
			const luxrays::Point &p, const luxrays::Normal &n) const {
	const ExtMesh *mesh = sceneObject->GetExtMesh();

	// This would be the correct code but BlendLuxCore is currently always
	// exporting the normals so I resort to the following trick
	/*if (mesh->HasNormals())
		return false;
	else {
		const float cosTheta = Dot(n, mesh->GetShadeNormal(0.f, triangleIndex, 0));

		return (cosTheta >= lightMaterial->GetEmittedCosThetaMax() + DEFAULT_COS_EPSILON_STATIC);
	}*/

	//	It is to hard to say if motion blur is enabled
	if ((mesh->GetType() == TYPE_TRIANGLE_MOTION) || (mesh->GetType() == TYPE_EXT_TRIANGLE_MOTION))
		return false;

	Transform localToWorld;
	mesh->GetLocal2World(0.f, localToWorld);

	// I use the shading normal of the first vertex for this test (see above)
	const Normal triNormal = mesh->HasNormals() ?
		mesh->GetShadeNormal(localToWorld, triangleIndex, 0) : mesh->GetGeometryNormal(localToWorld, triangleIndex);
	const float cosTheta = Dot(n, triNormal);

	return (cosTheta >= lightMaterial->GetEmittedCosThetaMax() + DEFAULT_COS_EPSILON_STATIC);
}

Spectrum TriangleLight::GetRadiance(const HitPoint &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	// A safety check to avoid NaN/Inf
	if ((triangleArea == 0.f) || (meshArea == 0.f))
		return Spectrum();

	const float cosOutLight = Dot(hitPoint.shadeN, hitPoint.fixedDir);
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();
	// emissionFunc can emit light even backward, this is for compatibility with classic Lux
	if (!emissionFunc && (cosOutLight < lightMaterial->GetEmittedCosThetaMax() + DEFAULT_COS_EPSILON_STATIC))
		return Spectrum();

	if (directPdfA)
		*directPdfA = invTriangleArea;

	Spectrum emissionColor(1.f);
	if (emissionFunc) {
		const Vector localFromLight = Normalize(hitPoint.GetFrame().ToLocal(hitPoint.fixedDir));
		
		if (emissionPdfW) {
			const float emissionFuncPdf = emissionFunc->Pdf(localFromLight);
			if (emissionFuncPdf == 0.f)
				return Spectrum();
			*emissionPdfW = emissionFuncPdf * invTriangleArea;
		}
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localFromLight) / emissionFunc->Average();
	} else {
		if (emissionPdfW) {
			if (lightMaterial->GetEmittedTheta() == 0.f)
				*emissionPdfW = 1.f;
			else if (lightMaterial->GetEmittedTheta() < 90.f)
				*emissionPdfW = UniformConePdf(lightMaterial->GetEmittedCosThetaMax());
			else
				*emissionPdfW = invTriangleArea * fabsf(cosOutLight) * INV_PI;
		}
	}

	return lightMaterial->GetEmittedRadiance(hitPoint, invMeshArea) * emissionColor;
}
