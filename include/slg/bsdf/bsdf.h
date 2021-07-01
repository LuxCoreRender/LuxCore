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

#ifndef _SLG_BSDF_H
#define	_SLG_BSDF_H

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/frame.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/color/color.h"
#include "slg/lights/light.h"
#include "slg/lights/trianglelight.h"
#include "slg/materials/material.h"
#include "slg/volumes/volume.h"
#include "slg/bsdf/bsdfevents.h"
#include "slg/bsdf/hitpoint.h"
#include "slg/scene/sceneobject.h"
#include "slg/utils/pathvolumeinfo.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/bsdf/bsdf_types.cl"
}

typedef enum {
	NO_REFLECT_TRANSMIT, ONLY_REFLECT, ONLY_TRANSMIT, REFLECT_TRANSMIT
} AlbedoSpecularSetting;

extern AlbedoSpecularSetting String2AlbedoSpecularSetting(const std::string &type);
extern const std::string AlbedoSpecularSetting2String(const AlbedoSpecularSetting type);

class Scene;

class BSDF {
public:
	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool fixedFromLight, const bool throughShadowTransparency,
		const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent,
		const PathVolumeInfo *volInfo) {
		assert (!rayHit.Miss());
		Init(fixedFromLight, throughShadowTransparency, scene,
				ray, rayHit, passThroughEvent, volInfo);
	}
	// A BSDF initialized with a point on a surface
	BSDF(const Scene &scene,
		const u_int meshIndex, const u_int triangleIndex,
		const luxrays::Point &surfacePoint,
		const float surfacePointBary1, const float surfacePointBary2, 
		const float time,
		const float passThroughEvent, const PathVolumeInfo *volInfo) {
		Init(scene, meshIndex, triangleIndex,
				surfacePoint, surfacePointBary1, surfacePointBary2,
				time, passThroughEvent, volInfo);
	}
	// A BSDF initialized with a volume scattering point
	BSDF(const bool fixedFromLight, const bool throughShadowTransparency,
		const Scene &scene, const luxrays::Ray &ray,
		const Volume &volume, const float t, const float passThroughEvent) {
		Init(fixedFromLight, throughShadowTransparency,
				scene, ray, volume, t, passThroughEvent);
	}

	// Used when hitting a surface
	void Init(const bool fixedFromLight, const bool throughShadowTransparency,
		const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent,
		const PathVolumeInfo *volInfo);
	// Used when have a point of a surface
	void Init(const Scene &scene,
		const u_int meshIndex, const u_int triangleIndex,
		const luxrays::Point &surfacePoint,
		const float surfacePointBary1, const float surfacePointBary2, 
		const float time,
		const float passThroughEvent, const PathVolumeInfo *volInfo);
	// Used when hitting a volume scatter point
	void Init(const bool fixedFromLight, const bool throughShadowTransparency,
		const Scene &scene, const luxrays::Ray &ray,
		const Volume &volume, const float t, const float passThroughEvent);

	void MoveHitPoint(const luxrays::Point &p, const luxrays::Normal &n);
	
	bool IsEmpty() const { return (material == NULL); }
	bool IsLightSource() const { return material->IsLightSource(); }
	bool IsDelta() const { return material->IsDelta(); }
	bool IsVisibleIndirectDiffuse() const { return material->IsVisibleIndirectDiffuse(); }
	bool IsVisibleIndirectGlossy() const { return material->IsVisibleIndirectGlossy(); }
	bool IsVisibleIndirectSpecular() const { return material->IsVisibleIndirectSpecular(); }
	bool IsShadowCatcher() const { return material->IsShadowCatcher(); }
	bool IsShadowCatcherOnlyInfiniteLights() const { return material->IsShadowCatcherOnlyInfiniteLights(); }
	bool IsCameraInvisible() const;
	bool IsVolume() const { return dynamic_cast<const Volume *>(material) != NULL; }
	bool IsPhotonGIEnabled() const { return material->IsPhotonGIEnabled(); }
	bool IsHoldout() const { return material->IsHoldout(); }
	bool IsAlbedoEndPoint(const AlbedoSpecularSetting albedoSpecularSetting,
		const float albedoSpecularGlossinessThreshold) const;
	u_int GetObjectID() const;
	const std::string &GetMaterialName() const;
	u_int GetMaterialID() const { return material->GetID(); }
	u_int GetLightID() const { return material->GetLightID(); }
	const Volume *GetMaterialInteriorVolume() const { return material->GetInteriorVolume(hitPoint, hitPoint.passThroughEvent); }
	const Volume *GetMaterialExteriorVolume() const { return material->GetExteriorVolume(hitPoint, hitPoint.passThroughEvent); }
	float GetGlossiness() const { return material->GetGlossiness(); }
	const SceneObject *GetSceneObject() const { return sceneObject; }

	BSDFEvent GetEventTypes() const { return material->GetEventTypes(); }
	MaterialType GetMaterialType() const { return material->GetType(); }

	luxrays::Spectrum GetPassThroughTransparency(const bool backTracing) const;
	const luxrays::Spectrum &GetPassThroughShadowTransparency() const { return material->GetPassThroughShadowTransparency(); }
	const luxrays::Frame &GetFrame() const { return frame; }

	luxrays::Spectrum Albedo() const;
	luxrays::Spectrum EvaluateTotal() const;
	luxrays::Spectrum Evaluate(const luxrays::Vector &generatedDir,
		BSDFEvent *event, float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(luxrays::Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir,
		BSDFEvent *event) const;
	luxrays::Spectrum ShadowCatcherSample(Vector *sampledDir,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event) const;
	void Pdf(const luxrays::Vector &sampledDir, float *directPdfW, float *reversePdfW) const;

	luxrays::Spectrum GetEmittedRadiance(float *directPdfA = NULL, float *emissionPdfW = NULL) const ;

	const LightSource *GetLightSource() const { return triangleLightSource; }
	
	luxrays::Point GetRayOrigin(const luxrays::Vector &sampleDir) const {
		if (IsVolume())
			return hitPoint.p;
		else {
			// Rise the ray origin along the geometry normal to avoid self intersection
			const float riseDirection = (Dot(sampleDir, hitPoint.geometryN) > 0.f) ? 1.f : -1.f;

			return hitPoint.p + riseDirection * luxrays::Vector(hitPoint.geometryN * luxrays::MachineEpsilon::E(hitPoint.p));
		}
	}

	bool HasBakeMap(const BakeMapType type) const;
	luxrays::Spectrum GetBakeMapValue() const;

	HitPoint hitPoint;

private:
	const SceneObject *sceneObject;
	const Material *material;
	const TriangleLight *triangleLightSource; // != NULL only if it is an area light
	luxrays::Frame frame;
};
	
}

#endif	/* _SLG_BSDF_H */
