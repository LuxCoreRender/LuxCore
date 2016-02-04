/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/bsdf/bsdf_types.cl"
}

class Scene;
class SceneObject;

class BSDF {
public:
	// An empty BSDF
	BSDF() : material(NULL) { };

	// A BSDF initialized from a ray hit
	BSDF(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent,
		const PathVolumeInfo *volInfo) {
		assert (!rayHit.Miss());
		Init(fixedFromLight, scene, ray, rayHit, passThroughEvent, volInfo);
	}
	// Used when hitting a surface
	void Init(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const luxrays::RayHit &rayHit, const float passThroughEvent,
		const PathVolumeInfo *volInfo);
	// Used when hitting a volume scatter point
	void Init(const bool fixedFromLight, const Scene &scene, const luxrays::Ray &ray,
		const Volume &volume, const float t, const float passThroughEvent);

	bool IsEmpty() const { return (material == NULL); }
	bool IsLightSource() const { return material->IsLightSource(); }
	bool IsDelta() const { return material->IsDelta(); }
	bool IsVisibleIndirectDiffuse() const { return material->IsVisibleIndirectDiffuse(); }
	bool IsVisibleIndirectGlossy() const { return material->IsVisibleIndirectGlossy(); }
	bool IsVisibleIndirectSpecular() const { return material->IsVisibleIndirectSpecular(); }
	bool IsShadowCatcher() const { return material->IsShadowCatcher(); }
	bool IsVolume() const { return dynamic_cast<const Volume *>(material) != NULL; }
	int GetSamples() const { return material->GetSamples(); }
	u_int GetObjectID() const;
	u_int GetMaterialID() const { return material->GetID(); }
	u_int GetLightID() const { return material->GetLightID(); }
	const Volume *GetMaterialInteriorVolume() const { return material->GetInteriorVolume(hitPoint, hitPoint.passThroughEvent); }
	const Volume *GetMaterialExteriorVolume() const { return material->GetExteriorVolume(hitPoint, hitPoint.passThroughEvent); }

	BSDFEvent GetEventTypes() const { return material->GetEventTypes(); }
	MaterialType GetMaterialType() const { return material->GetType(); }

	luxrays::Spectrum GetPassThroughTransparency() const;
	const luxrays::Frame &GetFrame() const { return frame; }

	luxrays::Spectrum Evaluate(const luxrays::Vector &generatedDir,
		BSDFEvent *event, float *directPdfW = NULL, float *reversePdfW = NULL) const;
	luxrays::Spectrum Sample(luxrays::Vector *sampledDir,
		const float u0, const float u1,
		float *pdfW, float *absCosSampledDir, BSDFEvent *event,
		const BSDFEvent requestedEvent = ALL) const;
	void Pdf(const luxrays::Vector &sampledDir, float *directPdfW, float *reversePdfW) const;

	luxrays::Spectrum GetEmittedRadiance(float *directPdfA = NULL, float *emissionPdfW = NULL) const ;

	const LightSource *GetLightSource() const { return triangleLightSource; }

	HitPoint hitPoint;

private:
	const SceneObject *sceneObject;
	const luxrays::ExtMesh *mesh;
	const Material *material;
	const TriangleLight *triangleLightSource; // != NULL only if it is an area light
	luxrays::Frame frame;
};
	
}

#endif	/* _SLG_BSDF_H */
