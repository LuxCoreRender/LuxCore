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

class Scene;

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	TYPE_SPOT, TYPE_PROJECTION, TYPE_IL_CONSTANT, TYPE_SHARPDISTANT, TYPE_DISTANT,
	TYPE_IL_SKY2, TYPE_LASER,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

//------------------------------------------------------------------------------
// Generic LightSource interface
//------------------------------------------------------------------------------

class LightSource {
public:
	LightSource() : lightSceneIndex(0) { }
	virtual ~LightSource() { }

	std::string GetName() const { return "light-" + boost::lexical_cast<std::string>(this); }

	virtual void Preprocess() = 0;

	virtual LightSourceType GetType() const = 0;

	// If emits light on rays intersecting nothing
	virtual bool IsEnvironmental() const { return false; }
	// If the emitted power is based on scene radius
	virtual bool IsInfinite() const { return false; }
	// If it can be directly intersected by a ray
	virtual bool IsIntersectable() const { return false; }

	virtual u_int GetID() const = 0;
	virtual float GetPower(const Scene &scene) const = 0;
	virtual int GetSamples() const = 0;
	virtual float GetImportance() const = 0;

	virtual bool IsVisibleIndirectDiffuse() const = 0;
	virtual bool IsVisibleIndirectGlossy() const = 0;
	virtual bool IsVisibleIndirectSpecular() const = 0;

	// Emits particle from the light
	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const = 0;

	// Illuminates a luxrays::Point in the scene
    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const = 0;

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const { }

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

	virtual float GetPower(const Scene &scene) const = 0;
	virtual u_int GetID() const { return lightMaterial->GetLightID(); }
	virtual int GetSamples() const { return lightMaterial->GetEmittedSamples(); }
	virtual float GetImportance() const { return lightMaterial->GetEmittedImportance(); }

	virtual bool IsVisibleIndirectDiffuse() const { return lightMaterial->IsVisibleIndirectDiffuse(); }
	virtual bool IsVisibleIndirectGlossy() const { return lightMaterial->IsVisibleIndirectGlossy(); }
	virtual bool IsVisibleIndirectSpecular() const { return lightMaterial->IsVisibleIndirectSpecular(); }

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const = 0;

	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
		if (lightMaterial == oldMat)
			lightMaterial = newMat;
}

	const Material *lightMaterial;
};

//------------------------------------------------------------------------------
// Not intersectable LightSource interface
//------------------------------------------------------------------------------

class NotIntersectableLightSource : public LightSource {
public:
	NotIntersectableLightSource() :
		gain(1.f), id(0), samples(-1), importance(1.f) { }
	virtual ~NotIntersectableLightSource() { }

	virtual bool IsVisibleIndirectDiffuse() const { return false; }
	virtual bool IsVisibleIndirectGlossy() const { return false; }
	virtual bool IsVisibleIndirectSpecular() const { return false; }
	
	virtual void SetID(const u_int lightID) { id = lightID; }
	virtual u_int GetID() const { return id; }
	void SetSamples(const int sampleCount) { samples = sampleCount; }
	virtual int GetSamples() const { return samples; }
	virtual float GetImportance() const { return importance; }
	virtual void SetImportance(const float imp) { importance = imp; }

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Transform lightToWorld;
	luxrays::Spectrum gain;

protected:
	u_int id;
	int samples;
	float importance;
};

//------------------------------------------------------------------------------
// Infinite LightSource interface
//------------------------------------------------------------------------------

// This is used to scale the world radius in sun/sky/infinite lights in order to
// avoid problems with objects that are near the borderline of the world bounding sphere
extern const float LIGHT_WORLD_RADIUS_SCALE;

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

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

protected:
	static float GetEnvRadius(const Scene &scene);

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

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const = 0;
};

}

#endif	/* _SLG_LIGHT_H */
