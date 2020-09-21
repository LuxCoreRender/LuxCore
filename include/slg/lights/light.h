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

#ifndef _SLG_LIGHT_H
#define	_SLG_LIGHT_H

#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/geometry/vector.h"
#include "luxrays/core/randomgen.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/color/color.h"
#include "luxrays/core/color/spds/regular.h"
#include "luxrays/core/color/spds/irregular.h"
#include "luxrays/core/namedobject.h"
#include "luxrays/utils/mcdistribution.h"
#include "slg/textures/texture.h"
#include "slg/textures/mapping/mapping.h"
#include "slg/materials/material.h"

namespace slg {

// OpenCL data types
namespace ocl {
using luxrays::ocl::Vector;
#include "slg/lights/light_types.cl"
}

class BSDF;
class Scene;

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	TYPE_SPOT, TYPE_PROJECTION, TYPE_IL_CONSTANT, TYPE_SHARPDISTANT, TYPE_DISTANT,
	TYPE_IL_SKY2, TYPE_LASER, TYPE_SPHERE, TYPE_MAPSPHERE,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

//------------------------------------------------------------------------------
// Generic LightSource interface
//------------------------------------------------------------------------------

class LightSource : public luxrays::NamedObject {
public:
	LightSource() : NamedObject("light"), lightSceneIndex(0) { }
	virtual ~LightSource() { }

	virtual void Preprocess() = 0;

	virtual LightSourceType GetType() const = 0;

	// If emits light on rays intersecting nothing
	virtual bool IsEnvironmental() const { return false; }
	// If the emitted power is based on scene radius
	virtual bool IsInfinite() const { return false; }
	// If it can be directly intersected by a ray
	virtual bool IsIntersectable() const { return false; }

	virtual float GetAvgPassThroughTransparency() const { return 1.f; }

	virtual u_int GetID() const = 0;
	virtual float GetPower(const Scene &scene) const = 0;
	virtual float GetImportance() const = 0;

	virtual bool IsDirectLightSamplingEnabled() const = 0;

	virtual bool IsVisibleIndirectDiffuse() const = 0;
	virtual bool IsVisibleIndirectGlossy() const = 0;
	virtual bool IsVisibleIndirectSpecular() const = 0;

	// Emits particle from the light
	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float time, const float u0, const float u1,
		const float u2, const float u3, const float passThroughEvent,
		luxrays::Ray &ray, float &emissionPdfW,
		float *directPdfA = NULL, float *cosThetaAtLight = NULL) const = 0;

	// Illuminates bsdf.hitPoint.p
    virtual luxrays::Spectrum Illuminate(const Scene &scene, const BSDF &bsdf,
		const float time, const float u0, const float u1, const float passThroughEvent,
        luxrays::Ray &shadowRay, float &directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const = 0;

	// This can be used at pre-processing stage to check if the point is always in
	// shadow (to avoid tracing the shadow ray). This method can be optionally
	// implemented by a light source. The return value can be just false if the
	// answer is unknown.
	virtual bool IsAlwaysInShadow(const Scene &scene,
			const luxrays::Point &p, const luxrays::Normal &n) const {
		return false;
	}
	
	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const { }

	static std::string LightSourceType2String(const LightSourceType type);

	u_int lightSceneIndex;
};

//------------------------------------------------------------------------------
// Intersectable LightSource interface
//------------------------------------------------------------------------------

class IntersectableLightSource : public LightSource {
public:
	IntersectableLightSource() : lightMaterial(NULL) { }
	virtual ~IntersectableLightSource() { }

	virtual bool IsIntersectable() const { return true; }

	virtual float GetAvgPassThroughTransparency() const { return lightMaterial->GetAvgPassThroughTransparency(); }
	virtual float GetPower(const Scene &scene) const = 0;
	virtual u_int GetID() const { return lightMaterial->GetLightID(); }
	virtual float GetImportance() const { return lightMaterial->GetEmittedImportance(); }

	virtual bool IsVisibleIndirectDiffuse() const { return lightMaterial->IsVisibleIndirectDiffuse(); }
	virtual bool IsVisibleIndirectGlossy() const { return lightMaterial->IsVisibleIndirectGlossy(); }
	virtual bool IsVisibleIndirectSpecular() const { return lightMaterial->IsVisibleIndirectSpecular(); }

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const = 0;

	const Material *lightMaterial;
};

//------------------------------------------------------------------------------
// Not intersectable LightSource interface
//------------------------------------------------------------------------------

class NotIntersectableLightSource : public LightSource {
public:
	NotIntersectableLightSource() :
		gain(1.f), temperature(-1.f), normalizeTemperature(false),
		id(0), importance(1.f), temperatureScale(1.f) { }
	virtual ~NotIntersectableLightSource() { }

	virtual void Preprocess();

	virtual bool IsDirectLightSamplingEnabled() const { return true; }

	virtual bool IsVisibleIndirectDiffuse() const { return false; }
	virtual bool IsVisibleIndirectGlossy() const { return false; }
	virtual bool IsVisibleIndirectSpecular() const { return false; }
	
	void SetID(const u_int lightID) { id = lightID; }
	u_int GetID() const { return id; }
	float GetImportance() const { return importance; }
	void SetImportance(const float imp) { importance = imp; }
	
	const luxrays::Spectrum &GetTemperatureScale() const { return temperatureScale; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	luxrays::Transform lightToWorld;
	luxrays::Spectrum gain;

	float temperature;
	bool normalizeTemperature;

protected:
	u_int id;
	float importance;

	luxrays::Spectrum temperatureScale;
};

//------------------------------------------------------------------------------
// Infinite LightSource interface
//------------------------------------------------------------------------------

class InfiniteLightSource : public NotIntersectableLightSource {
public:
	InfiniteLightSource() : isVisibleIndirectDiffuse(true),
		isVisibleIndirectGlossy(true), isVisibleIndirectSpecular(true) { }
	virtual ~InfiniteLightSource() { }

	virtual bool IsInfinite() const { return true; }

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }

	virtual bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	virtual bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	virtual bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const;

	static float GetEnvRadius(const Scene &scene);

protected:
	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular;
};

//------------------------------------------------------------------------------
// Env. LightSource interface
//------------------------------------------------------------------------------

class EnvLightSource : public InfiniteLightSource {
public:
	EnvLightSource() { }
	virtual ~EnvLightSource() { }

	virtual bool IsEnvironmental() const { return true; }

	virtual luxrays::UV GetEnvUV(const luxrays::Vector &dir) const {
		throw std::runtime_error("Internal error: called EnvLightSource::GetEnvUV()");
	}
	virtual void UpdateVisibilityMap(const Scene *scene, const bool useRTMode) { }

	// Note: bsdf parameter can be NULL if it is a camera ray
	virtual luxrays::Spectrum GetRadiance(const Scene &scene,
			const BSDF *bsdf, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const = 0;

	static void ToLatLongMapping(const Vector &w, float *s, float *t, float *pdf = NULL);
	static void FromLatLongMapping(const float s, const float t, Vector *w, float *pdf = NULL);
};

}

#endif	/* _SLG_LIGHT_H */
