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
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/material.h"
#include "slg/sdl/mapping.h"

namespace slg {

// OpenCL data types
namespace ocl {
using luxrays::ocl::Vector;
#include "slg/sdl/light_types.cl"
}

class Scene;

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT, TYPE_MAPPOINT,
	TYPE_SPOT, TYPE_PROJECTION, TYPE_IL_CONSTANT, TYPE_SHARPDISTANT, TYPE_DISTANT,
	TYPE_IL_SKY2, TYPE_LASER,
	LIGHT_SOURCE_TYPE_COUNT
} LightSourceType;

extern const float LIGHT_WORLD_RADIUS_SCALE;

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
	IntersectableLightSource() : lightMaterial(NULL), area(0.f), invArea(0.f) { }
	virtual ~IntersectableLightSource() { }

	virtual bool IsIntersectable() const { return true; }

	virtual float GetPower(const Scene &scene) const = 0;
	virtual u_int GetID() const { return lightMaterial->GetLightID(); }
	virtual int GetSamples() const { return lightMaterial->GetEmittedSamples(); }
	virtual float GetImportance() const { return lightMaterial->GetEmittedImportance(); }

	virtual bool IsVisibleIndirectDiffuse() const { return lightMaterial->IsVisibleIndirectDiffuse(); }
	virtual bool IsVisibleIndirectGlossy() const { return lightMaterial->IsVisibleIndirectGlossy(); }
	virtual bool IsVisibleIndirectSpecular() const { return lightMaterial->IsVisibleIndirectSpecular(); }

	float GetArea() const { return area; }

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const = 0;

	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
		if (lightMaterial == oldMat)
			lightMaterial = newMat;
	}

	const Material *lightMaterial;
	
protected:
	float area, invArea;
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

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

typedef enum {
	TYPE_UNIFORM, TYPE_POWER, TYPE_LOG_POWER,
	LIGHT_STRATEGY_TYPE_COUNT
} LightStrategyType;

class LightStrategy {
public:
	virtual ~LightStrategy() { delete lightsDistribution; }

	LightStrategyType GetType() const { return type; }

	virtual void Preprocess(const Scene *scn) { scene = scn; }

	LightSource *SampleLights(const float u, float *pdf) const;
	float SampleLightPdf(const LightSource *light) const;
	
	const luxrays::Distribution1D *GetLightsDistribution() const { return lightsDistribution; }

protected:
	LightStrategy(const LightStrategyType t) : scene(NULL), lightsDistribution(NULL), type(t) { }

	const Scene *scene;
	luxrays::Distribution1D *lightsDistribution;

private:
	const LightStrategyType type;
};

class LightStrategyUniform : public LightStrategy {
public:
	LightStrategyUniform() : LightStrategy(TYPE_UNIFORM) { }

	virtual void Preprocess(const Scene *scene);
};

class LightStrategyPower : public LightStrategy {
public:
	LightStrategyPower() : LightStrategy(TYPE_POWER) { }

	virtual void Preprocess(const Scene *scene);
};

class LightStrategyLogPower : public LightStrategy {
public:
	LightStrategyLogPower() : LightStrategy(TYPE_LOG_POWER) { }

	virtual void Preprocess(const Scene *scene);
};

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

class TriangleLight;

class LightSourceDefinitions {
public:
	LightSourceDefinitions();
	~LightSourceDefinitions();

	void SetLightStrategy(const LightStrategyType type);

	// Update lightGroupCount, infiniteLightSources, intersectableLightSources,
	// lightIndexByMeshIndex and lightStrategyType
	void Preprocess(const Scene *scene);

	bool IsLightSourceDefined(const std::string &name) const {
		return (lightsByName.count(name) > 0);
	}
	void DefineLightSource(const std::string &name, LightSource *l);

	const LightSource *GetLightSource(const std::string &name) const;
	LightSource *GetLightSource(const std::string &name);
	const LightSource *GetLightSource(const u_int index) const { return lights[index]; }
	LightSource *GetLightSource(const u_int index) { return lights[index]; }
	u_int GetLightSourceIndex(const std::string &name) const;
	u_int GetLightSourceIndex(const LightSource *l) const;
	const LightSource *GetLightByType(const LightSourceType type) const;
	const TriangleLight *GetLightSourceByMeshIndex(const u_int index) const;

	u_int GetSize() const { return static_cast<u_int>(lights.size()); }
	std::vector<std::string> GetLightSourceNames() const;

	// Update any reference to oldMat with newMat
	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat);

	void DeleteLightSource(const std::string &name);
  
	u_int GetLightGroupCount() const { return lightGroupCount; }
	const u_int GetLightTypeCount(const LightSourceType type) const { return lightTypeCount[type]; }
	const vector<u_int> &GetLightTypeCounts() const { return lightTypeCount; }

	const std::vector<LightSource *> &GetLightSources() const {
		return lights;
	}
	const std::vector<EnvLightSource *> &GetEnvLightSources() const {
		return envLightSources;
	}
	const std::vector<TriangleLight *> &GetIntersectableLightSources() const {
		return intersectableLightSources;
	}
	const std::vector<u_int> &GetLightIndexByMeshIndex() const { return lightIndexByMeshIndex; }
	const LightStrategy *GetLightStrategy() const { return lightStrategy; }

private:
	std::vector<LightSource *> lights;
	boost::unordered_map<std::string, LightSource *> lightsByName;

	u_int lightGroupCount;
	vector<u_int> lightTypeCount;

	std::vector<u_int> lightIndexByMeshIndex;

	// Only intersectable light sources
	std::vector<TriangleLight *> intersectableLightSources;
	// Only env. light sources (i.e. sky, sun and infinite light, etc.)
	std::vector<EnvLightSource *> envLightSources;

	LightStrategy *lightStrategy;
};

//------------------------------------------------------------------------------
// PointLight implementation
//------------------------------------------------------------------------------

class PointLight : public NotIntersectableLightSource {
public:
	PointLight();
	virtual ~PointLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *localPos, float *absolutePos, float *emittedFactor) const;

	virtual LightSourceType GetType() const { return TYPE_POINT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Point localPos;
	luxrays::Spectrum color;
	float power, efficency;

protected:
	luxrays::Spectrum emittedFactor;
	luxrays::Point absolutePos;
};

//------------------------------------------------------------------------------
// MapPointLight implementation
//------------------------------------------------------------------------------

class MapPointLight : public PointLight {
public:
	MapPointLight();
	virtual ~MapPointLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *localPos, float *absolutePos, float *emittedFactor,
		const SampleableSphericalFunction **funcData) const;

	virtual LightSourceType GetType() const { return TYPE_MAPPOINT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	const ImageMap *imageMap;

private:
	SampleableSphericalFunction *func;
};

//------------------------------------------------------------------------------
// SpotLight implementation
//------------------------------------------------------------------------------

class SpotLight : public NotIntersectableLightSource {
public:
	SpotLight();
	virtual ~SpotLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *emittedFactor, float *absolutePos,
		float *cosTotalWidth, float *cosFalloffStart,
		const luxrays::Transform **alignedLight2World) const;

	virtual LightSourceType GetType() const { return TYPE_SPOT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	float power, efficency;

	luxrays::Point localPos, localTarget;
	float coneAngle, coneDeltaAngle;

protected:
	luxrays::Spectrum emittedFactor;
	luxrays::Point absolutePos;
	float cosTotalWidth, cosFalloffStart;
	luxrays::Transform alignedLight2World;
};

//------------------------------------------------------------------------------
// ProjectionLight implementation
//------------------------------------------------------------------------------

class ProjectionLight : public NotIntersectableLightSource {
public:
	ProjectionLight();
	virtual ~ProjectionLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *emittedFactor, float *absolutePos, float *lightNormal,
		float *screenX0, float *screenX1, float *screenY0, float *screenY1,
		const luxrays::Transform **alignedLight2World, const luxrays::Transform **lightProjection) const;

	virtual LightSourceType GetType() const { return TYPE_PROJECTION; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	float power, efficency;

	luxrays::Point localPos, localTarget;

	float fov;
	const ImageMap *imageMap;

protected:
	luxrays::Spectrum emittedFactor;
	luxrays::Point absolutePos;
	luxrays::Normal lightNormal;
	float screenX0, screenX1, screenY0, screenY1, area;
	float cosTotalWidth;
	luxrays::Transform alignedLight2World, lightProjection;
};

//------------------------------------------------------------------------------
// LaserLight implementation
//------------------------------------------------------------------------------

class LaserLight : public NotIntersectableLightSource {
public:
	LaserLight();
	virtual ~LaserLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *emittedFactor, float *absolutePos,
		float *absolutDir, float *xData, float *yData) const;

	virtual LightSourceType GetType() const { return TYPE_LASER; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	float power, efficency;

	luxrays::Point localPos, localTarget;
	float radius;

protected:
	luxrays::Spectrum emittedFactor;
	luxrays::Point absoluteLightPos;
	luxrays::Vector absoluteLightDir;
	luxrays::Vector x, y;
};

//------------------------------------------------------------------------------
// SharpDistantLight implementation
//------------------------------------------------------------------------------

class SharpDistantLight : public InfiniteLightSource {
public:
	SharpDistantLight();
	virtual ~SharpDistantLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData) const;

	virtual LightSourceType GetType() const { return TYPE_SHARPDISTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;
	
	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	luxrays::Vector localLightDir;

protected:
	luxrays::Vector absoluteLightDir;
	luxrays::Vector x, y;
};

//------------------------------------------------------------------------------
// DistantLight implementation
//------------------------------------------------------------------------------

class DistantLight : public InfiniteLightSource {
public:
	DistantLight();
	virtual ~DistantLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData,
		float *sin2ThetaMaxData, float *cosThetaMaxData) const;

	virtual LightSourceType GetType() const { return TYPE_DISTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	luxrays::Vector localLightDir;
	float theta;

protected:
	luxrays::Vector absoluteLightDir;
	luxrays::Vector x, y;
	float sin2ThetaMax, cosThetaMax;
};

//------------------------------------------------------------------------------
// ConstantInfiniteLight implementation
//------------------------------------------------------------------------------

class ConstantInfiniteLight : public EnvLightSource {
public:
	ConstantInfiniteLight();
	virtual ~ConstantInfiniteLight();

	virtual void Preprocess() { }

	virtual LightSourceType GetType() const { return TYPE_IL_CONSTANT; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
};

//------------------------------------------------------------------------------
// InfiniteLight implementation
//------------------------------------------------------------------------------

class InfiniteLight : public EnvLightSource {
public:
	InfiniteLight();
	virtual ~InfiniteLight();

	virtual void Preprocess();
	void GetPreprocessedData(const luxrays::Distribution2D **imageMapDistribution) const;

	virtual LightSourceType GetType() const { return TYPE_IL; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
		referencedImgMaps.insert(imageMap);
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	const ImageMap *imageMap;
	UVMapping2D mapping;

private:	
	luxrays::Distribution2D *imageMapDistribution;
};

//------------------------------------------------------------------------------
// Sky implementation
//------------------------------------------------------------------------------

class SkyLight : public EnvLightSource {
public:
	SkyLight();
	virtual ~SkyLight();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteSunDirData,
		float *absoluteThetaData, float *absolutePhiData,
		float *zenith_YData, float *zenith_xData, float *zenith_yData,
		float *perez_YData, float *perez_xData, float *perez_yData) const;

	virtual LightSourceType GetType() const { return TYPE_IL_SKY; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Vector localSunDir;
	float turbidity;

private:
	void GetSkySpectralRadiance(const float theta, const float phi, luxrays::Spectrum * const spect) const;

	luxrays::Vector absoluteSunDir;
	float absoluteTheta, absolutePhi;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

//------------------------------------------------------------------------------
// Sky2 implementation
//------------------------------------------------------------------------------

class SkyLight2 : public EnvLightSource {
public:
	SkyLight2();
	virtual ~SkyLight2();

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteDirData,
		float *aTermData, float *bTermData, float *cTermData, float *dTermData,
		float *eTermData, float *fTermData, float *gTermData, float *hTermData,
		float *iTermData,float *radianceTermData) const;

	virtual LightSourceType GetType() const { return TYPE_IL_SKY2; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Vector localSunDir;
	float turbidity;

private:
	luxrays::Spectrum ComputeRadiance(const luxrays::Vector &w) const;
	float ComputeY(const luxrays::Vector &w) const;

	luxrays::Vector absoluteSunDir;
	luxrays::Spectrum model[10];
	luxrays::Spectrum aTerm, bTerm, cTerm, dTerm, eTerm, fTerm,
		gTerm, hTerm, iTerm, radianceTerm;
};

//------------------------------------------------------------------------------
// Sun implementation
//------------------------------------------------------------------------------

class SunLight : public EnvLightSource {
public:
	SunLight();
	virtual ~SunLight() { }

	virtual void Preprocess();
	void GetPreprocessedData(float *absoluteSunDirData,
		float *xData, float *yData,
		float *absoluteThetaData, float *absolutePhiData,
		float *VData, float *cosThetaMaxData, float *sin2ThetaMaxData) const;

	virtual LightSourceType GetType() const { return TYPE_SUN; }
	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

	luxrays::Spectrum color;
	luxrays::Vector localSunDir;
	float turbidity, relSize;

private:
	luxrays::Vector absoluteSunDir;
	// XY Vectors for cone sampling
	luxrays::Vector x, y;
	float absoluteTheta, absolutePhi;
	float V, cosThetaMax, sin2ThetaMax;
};

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public IntersectableLightSource {
public:
	TriangleLight();
	virtual ~TriangleLight();

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_TRIANGLE; }

	virtual float GetPower(const Scene &scene) const;

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

	const luxrays::ExtMesh *mesh;
	u_int meshIndex, triangleIndex;
};

}

#endif	/* _SLG_LIGHT_H */
