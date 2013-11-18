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
#include "luxrays/core/randomgen.h"
#include "luxrays/core/geometry/transform.h"
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/spectrum.h"
#include "slg/sdl/texture.h"
#include "slg/sdl/material.h"
#include "slg/sdl/mapping.h"

namespace slg {

// OpenCL data types
namespace ocl {
#include "slg/sdl/light_types.cl"
}

class Scene;

typedef enum {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE, TYPE_POINT,
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
	virtual void SetSceneIndex(const u_int index) { lightSceneIndex = index; }
	virtual u_int GetSceneIndex() const { return lightSceneIndex; }

	virtual bool IsEnvironmental() const { return false; }

	virtual u_int GetID() const = 0;
	virtual float GetPower(const Scene &scene) const = 0;
	virtual int GetSamples() const = 0;

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

private:
	u_int lightSceneIndex;
};

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

class EnvLightSource;
class TriangleLight;

class LightSourceDefinitions {
public:
	LightSourceDefinitions();
	~LightSourceDefinitions();

	// Update lightGroupCount, envLightSources, intersecableLightSources,
	// lightIndexByMeshIndex and lightsDistribution
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
	const std::vector<TriangleLight *> &GetIntersecableLightSources() const {
		return intersecableLightSources;
	}
	const std::vector<u_int> &GetLightIndexByMeshIndex() const { return lightIndexByMeshIndex; }
	const Distribution1D *GetLightsDistribution() const { return lightsDistribution; }

	LightSource *SampleAllLights(const float u, float *pdf) const;
	float SampleAllLightPdf(const LightSource *light) const;

private:
	std::vector<LightSource *> lights;
	boost::unordered_map<std::string, LightSource *> lightsByName;

	u_int lightGroupCount;
	vector<u_int> lightTypeCount;

	std::vector<u_int> lightIndexByMeshIndex;

	// Only intersecable light sources
	std::vector<TriangleLight *> intersecableLightSources;
	// Only env. lights sources (i.e. sky, sun and infinite light, etc.)
	std::vector<EnvLightSource *> envLightSources;

	// Used for power based light sampling strategy
	Distribution1D *lightsDistribution;

};

//------------------------------------------------------------------------------
// Intersecable LightSource interface
//------------------------------------------------------------------------------

class IntersecableLightSource : public LightSource {
public:
	IntersecableLightSource(const Material *mat) : lightMaterial(mat),
			area(0.f), invArea(0.f) { }
	virtual ~IntersecableLightSource() { }

	virtual float GetPower(const Scene &scene) const = 0;
	virtual u_int GetID() const { return lightMaterial->GetLightID(); }
	virtual int GetSamples() const { return lightMaterial->GetEmittedSamples(); }

	bool IsVisibleIndirectDiffuse() const { return lightMaterial->IsVisibleIndirectDiffuse(); }
	bool IsVisibleIndirectGlossy() const { return lightMaterial->IsVisibleIndirectGlossy(); }
	bool IsVisibleIndirectSpecular() const { return lightMaterial->IsVisibleIndirectSpecular(); }

	void SetMaterial(const Material *mat) { lightMaterial = mat; }
	const Material *GetMaterial() const { return lightMaterial; }

	float GetArea() const { return area; }

	virtual luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const = 0;

	void UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
		if (lightMaterial == oldMat)
			lightMaterial = newMat;
	}

protected:
	const Material *lightMaterial;
	float area, invArea;
};

//------------------------------------------------------------------------------
// Not interescable LightSource interface
//------------------------------------------------------------------------------

class NotIntersecableLightSource : public LightSource {
public:
	NotIntersecableLightSource(const luxrays::Transform &l2w) :
		id(0), lightToWorld(l2w), gain(1.f), samples(-1),
		isVisibleIndirectDiffuse(true), isVisibleIndirectGlossy(true),
		isVisibleIndirectSpecular(true) { }
	virtual ~NotIntersecableLightSource() { }

	virtual void SetID(const u_int lightID) { id = lightID; }
	virtual u_int GetID() const { return id; }
	void SetSamples(const int sampleCount) { samples = sampleCount; }
	virtual int GetSamples() const { return samples; }

	void SetIndirectDiffuseVisibility(const bool visible) { isVisibleIndirectDiffuse = visible; }
	bool IsVisibleIndirectDiffuse() const { return isVisibleIndirectDiffuse; }
	void SetIndirectGlossyVisibility(const bool visible) { isVisibleIndirectGlossy = visible; }
	bool IsVisibleIndirectGlossy() const { return isVisibleIndirectGlossy; }
	void SetIndirectSpecularVisibility(const bool visible) { isVisibleIndirectSpecular = visible; }
	bool IsVisibleIndirectSpecular() const { return isVisibleIndirectSpecular; }

	const luxrays::Transform &GetTransformation() const { return lightToWorld; }

	void SetGain(const luxrays::Spectrum &g) { gain = g; }
	luxrays::Spectrum GetGain() const { return gain; }

	virtual void AddReferencedImageMaps(boost::unordered_set<const ImageMap *> &referencedImgMaps) const {
	}

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;

protected:
	u_int id;

	const luxrays::Transform lightToWorld;
	luxrays::Spectrum gain;
	int samples;

	bool isVisibleIndirectDiffuse, isVisibleIndirectGlossy, isVisibleIndirectSpecular;
};

//------------------------------------------------------------------------------
// Env. LightSource interface
//------------------------------------------------------------------------------

class EnvLightSource : public NotIntersecableLightSource {
public:
	EnvLightSource(const luxrays::Transform &l2w) :
		NotIntersecableLightSource(l2w) { }
	virtual ~EnvLightSource() { }

	virtual bool IsEnvironmental() const { return true; }

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const = 0;
};

//------------------------------------------------------------------------------
// PointLight implementation
//
// Note: the light source is always placed in the origin of local coord. space
//------------------------------------------------------------------------------

//class PointLight : public NotIntersecableLightSource {
//public:
//	PointLight(const luxrays::Transform &l2w);
//	virtual ~PointLight();
//
//	virtual void Preprocess() { }
//
//	virtual LightSourceType GetType() const { return TYPE_POINT; }
//	virtual float GetPower(const Scene &scene) const;
//
//	virtual luxrays::Spectrum Emit(const Scene &scene,
//		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
//		luxrays::Point *pos, luxrays::Vector *dir,
//		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;
//
//    virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
//		const float u0, const float u1, const float passThroughEvent,
//        luxrays::Vector *dir, float *distance, float *directPdfW,
//		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;
//
//	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
//			float *directPdfA = NULL, float *emissionPdfW = NULL) const;
//
//	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;
//};

//------------------------------------------------------------------------------
// InfiniteLight implementation
//------------------------------------------------------------------------------

class InfiniteLight : public EnvLightSource {
public:
	InfiniteLight(const luxrays::Transform &l2w, const ImageMap *imgMap);
	virtual ~InfiniteLight();

	virtual void Preprocess() { }

	virtual LightSourceType GetType() const { return TYPE_IL; }
	virtual float GetPower(const Scene &scene) const;

	const ImageMap *GetImageMap() const { return imageMap; }
	const UVMapping2D *GetUVMapping() const { return &mapping; }
	UVMapping2D *GetUVMapping() { return &mapping; }

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

	const Distribution2D *GetDistribution2D() const { return imageMapDistribution; }

private:
	const ImageMap *imageMap;
	UVMapping2D mapping;

	Distribution2D *imageMapDistribution;
};

//------------------------------------------------------------------------------
// Sky implementation
//------------------------------------------------------------------------------

class SkyLight : public EnvLightSource {
public:
	SkyLight(const luxrays::Transform &l2w, float turbidity, const luxrays::Vector &sundir);
	virtual ~SkyLight() { }

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_IL_SKY; }
	virtual float GetPower(const Scene &scene) const;

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	const luxrays::Vector GetSunDir() const { return Normalize(luxrays::Inverse(lightToWorld) * sunDir); }
	void SetSunDir(const luxrays::Vector &dir) { sunDir = Normalize(lightToWorld * dir); }

	void GetInitData(float *thetaSData, float *phiSData,
		float *zenith_YData, float *zenith_xData, float *zenith_yData,
		float *perez_YData, float *perez_xData, float *perez_yData) const {
		*thetaSData = thetaS;
		*phiSData = phiS;
		*zenith_YData = zenith_Y;
		*zenith_xData = zenith_x;
		*zenith_yData = zenith_y;
		for (size_t i = 0; i < 6; ++i) {
			perez_YData[i] = perez_Y[i];
			perez_xData[i] = perez_x[i];
			perez_yData[i] = perez_y[i];
		}
	}

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

private:
	void GetSkySpectralRadiance(const float theta, const float phi, luxrays::Spectrum * const spect) const;

	luxrays::Vector sunDir;
	float turbidity;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

//------------------------------------------------------------------------------
// Sun implementation
//------------------------------------------------------------------------------

class SunLight : public EnvLightSource {
public:
	SunLight(const luxrays::Transform &l2w, float turbidity, float relSize, const luxrays::Vector &sunDir);
	virtual ~SunLight() { }

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_SUN; }
	virtual float GetPower(const Scene &scene) const;

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetRelSize(const float s) { relSize = s; }
	float GetRelSize() const { return relSize; }

	const luxrays::Vector GetDir() const { return Normalize(luxrays::Inverse(lightToWorld) * sunDir); }
	void SetDir(const luxrays::Vector &dir) { sunDir = Normalize(lightToWorld * dir); }

	void GetInitData(luxrays::Vector *xData, luxrays::Vector *yData,
		float *thetaSData, float *phiSData, float *VData,
		float *cosThetaMaxData, float *sin2ThetaMaxData,
		luxrays::Spectrum *suncolorData) const {
		*xData = x;
		*yData = y;
		*thetaSData = thetaS;
		*phiSData = phiS;
		*VData = V;
		*cosThetaMaxData = cosThetaMax;
		*sin2ThetaMaxData = sin2ThetaMax;
		*suncolorData = sunColor;
	}

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

private:
	luxrays::Vector sunDir;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	luxrays::Vector x, y;
	float thetaS, phiS, V;
	float cosThetaMax, sin2ThetaMax;
	luxrays::Spectrum sunColor;
};

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public IntersecableLightSource {
public:
	TriangleLight(const Material *mat, const luxrays::ExtMesh *mesh,
		const u_int meshIndex, const u_int triangleIndex);
	virtual ~TriangleLight() { }

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_TRIANGLE; }

	virtual float GetPower(const Scene &scene) const;

	const luxrays::ExtMesh *GetMesh() const { return mesh; }
	u_int GetMeshIndex() const { return meshIndex; }
	u_int GetTriangleIndex() const { return triangleIndex; }

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

private:
	const luxrays::ExtMesh *mesh;
	u_int meshIndex, triangleIndex;
};

}

#endif	/* _SLG_LIGHT_H */
