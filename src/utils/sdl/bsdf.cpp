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

#include "luxrays/core/geometry/frame.h"
#include "luxrays/utils/sdl/bsdf.h"

namespace luxrays { namespace sdl {

void BSDF::Init(const bool fromL, const Scene &scene, const Ray &ray,
		const RayHit &rayHit, const float u0) {
	fromLight = fromL;
	isPassThrough = false;
	isLightSource = false;

	hitPoint = ray(rayHit.t);

	const unsigned int currentTriangleIndex = rayHit.index;
	const unsigned int currentMeshIndex = scene.dataSet->GetMeshID(currentTriangleIndex);

	// Get the triangle
	mesh = scene.objects[currentMeshIndex];
	triIndex = scene.dataSet->GetMeshTriangleID(currentTriangleIndex);

	// Get the material
	material = scene.objectMaterials[currentMeshIndex];

	// Interpolate face normal
	geometryN = mesh->GetGeometryNormal(triIndex);
	shadeN = mesh->InterpolateTriNormal(triIndex, rayHit.b1, rayHit.b2);

	// Check if it is a light source
	if (material->IsLightSource()) {
		isLightSource = true;
		lightSource = scene.triangleLightSource[currentTriangleIndex];
		// SLG light sources are like black bodies
		return;
	}

	surfMat = (const SurfaceMaterial *)material;

	if (mesh->HasColors())
		surfaceColor = mesh->InterpolateTriColor(triIndex, rayHit.b1, rayHit.b2);
	else
		surfaceColor = Spectrum(1.f, 1.f, 1.f);

	// Check if I have to apply texture mapping or normal mapping
	TexMapInstance *tm = scene.objectTexMaps[currentMeshIndex];
	BumpMapInstance *bm = scene.objectBumpMaps[currentMeshIndex];
	NormalMapInstance *nm = scene.objectNormalMaps[currentMeshIndex];
	if (tm || bm || nm) {
		// Interpolate UV coordinates if required
		const UV triUV = mesh->InterpolateTriUV(triIndex, rayHit.b1, rayHit.b2);

		// Check if there is an assigned texture map
		if (tm) {
			const TextureMap *map = tm->GetTexMap();

			// Apply texture mapping
			surfaceColor *= map->GetColor(triUV);

			// Check if the texture map has an alpha channel
			if (map->HasAlpha()) {
				const float alpha = map->GetAlpha(triUV);

				if ((alpha == 0.0f) || ((alpha < 1.f) && (u0 > alpha))) {
					// It is a pass-through material
					isPassThrough = true;
					return;
				}
			}
		}

		// Check if there is an assigned bump/normal map
		if (bm || nm) {
			if (nm) {
				// Apply normal mapping
				const Spectrum color = nm->GetTexMap()->GetColor(triUV);

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

			if (bm) {
				// Apply bump mapping
				const TextureMap *map = bm->GetTexMap();
				const UV &dudv = map->GetDuDv();

				const float b0 = map->GetColor(triUV).Filter();

				const UV uvdu(triUV.u + dudv.u, triUV.v);
				const float bu = map->GetColor(uvdu).Filter();

				const UV uvdv(triUV.u, triUV.v + dudv.v);
				const float bv = map->GetColor(uvdv).Filter();

				const float scale = bm->GetScale();
				const Vector bump(scale * (bu - b0), scale * (bv - b0), 1.f);

				Vector v1, v2;
				CoordinateSystem(Vector(shadeN), &v1, &v2);
				shadeN = Normalize(Normal(
						v1.x * bump.x + v2.x * bump.y + shadeN.x * bump.z,
						v1.y * bump.x + v2.y * bump.y + shadeN.y * bump.z,
						v1.z * bump.x + v2.z * bump.y + shadeN.z * bump.z));
			}
		}
	}

	frame.SetFromZ(shadeN);
}

Spectrum BSDF::Evaluate(const Vector &lightDir, const Vector &eyeDir,
		BSDFEvent *event, float *directPdfW, float *reversePdfW) const {
	const float dotLightDirNG = Dot(lightDir, geometryN);
	const float dotEyeDirNG = Dot(eyeDir, geometryN);

	const float sideTest = (fabsf(dotLightDirNG) < DEFAULT_EPSILON_STATIC) ? 0.f : (dotEyeDirNG * dotLightDirNG);
	if (sideTest > 0.f)
		*event = REFLECT;
	else if (sideTest < 0.f)
		*event = TRANSMIT;
	else {
		*event = NONE;
		return Spectrum();
	}

	Vector localLightDir = frame.ToLocal(lightDir);
	Vector localEyeDir = frame.ToLocal(eyeDir);
	Spectrum result = surfMat->Evaluate(localLightDir, localEyeDir, event,
			directPdfW, reversePdfW);

	// Adjoint BSDF
	if (fromLight) {
		const float dotLightDirNS = Dot(lightDir, shadeN);
		return surfaceColor * result * (fabsf(dotLightDirNS) / fabsf(dotLightDirNG));
	} else
		return surfaceColor * result;
}

Spectrum BSDF::Sample(const Vector &fixedDir, Vector *sampledDir,
		const float u0, const float u1,  const float u2,
		float *pdfW, float *cosSampleDir, BSDFEvent *event) const {
	Vector localFixedDir = frame.ToLocal(fixedDir);
	Vector localSampledDir;
	Spectrum result = surfMat->Sample(localFixedDir, &localSampledDir, u0, u1, u2,
			pdfW, cosSampleDir, event);
	*sampledDir = frame.ToWorld(localSampledDir);

	// Adjoint BSDF
	if (fromLight) {
		const float dotLightDirNG = localFixedDir.z;
		const float dotLightDirNS = Dot(fixedDir, shadeN);
		return surfaceColor * result * (fabsf(dotLightDirNS) / fabsf(dotLightDirNG));
	} else
		return surfaceColor * result;
}

Spectrum BSDF::GetEmittedRadiance(const Scene *scene,
			const Vector &dir,
			float *directPdfA,
			float *emissionPdfW) const {
	return isLightSource ? 
		lightSource->GetRadiance(scene, dir, hitPoint, directPdfA, emissionPdfW) :
		Spectrum(0.f);
}

} }
