/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#ifndef _SLG_LIGHT_H
#define	_SLG_LIGHT_H

#include "luxrays/luxrays.h"
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
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE
} LightSourceType;

//------------------------------------------------------------------------------
// LightSource implementation
//------------------------------------------------------------------------------

class LightSource {
public:
	LightSource() { }
	virtual ~LightSource() { }

	virtual LightSourceType GetType() const = 0;

	virtual bool IsEnvironmental() const { return false; }

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
};

//------------------------------------------------------------------------------
// InfiniteLightBase implementation
//------------------------------------------------------------------------------

class InfiniteLightBase : public LightSource {
public:
	InfiniteLightBase(const luxrays::Transform &l2w) : lightToWorld(l2w), gain(1.f, 1.f, 1.f) { }
	virtual ~InfiniteLightBase() { }

	virtual void Preprocess() { }

	virtual bool IsEnvironmental() const { return true; }

	const luxrays::Transform &GetTransformation() const { return lightToWorld; }

	void SetGain(const luxrays::Spectrum &g) {
		gain = g;
	}
	luxrays::Spectrum GetGain() const {
		return gain;
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
			float *directPdfA = NULL, float *emissionPdfW = NULL) const = 0;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const = 0;

protected:
	const luxrays::Transform lightToWorld;
	luxrays::Spectrum gain;
};

//------------------------------------------------------------------------------
// InfiniteLight implementation
//------------------------------------------------------------------------------

class InfiniteLight : public InfiniteLightBase {
public:
	InfiniteLight(const luxrays::Transform &l2w, const ImageMap *imgMap);
	virtual ~InfiniteLight() { }

	virtual LightSourceType GetType() const { return TYPE_IL; }

	const ImageMap *GetImageMap() const { return imageMap; }
	UVMapping2D *GetUVMapping() { return &mapping; }

	virtual luxrays::Spectrum GetRadiance(const Scene &scene, const luxrays::Vector &dir,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

	virtual luxrays::Properties ToProperties(const ImageMapCache &imgMapCache) const;

private:
	const ImageMap *imageMap;
	UVMapping2D mapping;
};

//------------------------------------------------------------------------------
// Sky implementation
//------------------------------------------------------------------------------

class SkyLight : public InfiniteLightBase {
public:
	SkyLight(const luxrays::Transform &l2w, float turbidity, const luxrays::Vector &sundir);
	virtual ~SkyLight() { }

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_IL_SKY; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	const luxrays::Vector GetSunDir() const { return Normalize(luxrays::Inverse(lightToWorld) * sunDir); }
	void SetSunDir(const luxrays::Vector &dir) { sunDir = Normalize(lightToWorld * dir); }

	void GetInitData(float *thetaSData, float *phiSData,
		float *zenith_YData, float *zenith_xData, float *zenith_yData,
		float *perez_YData, float *perez_xData, float *perez_yData) {
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

class SunLight : public LightSource {
public:
	SunLight(const luxrays::Transform &l2w, float turbidity, float relSize, const luxrays::Vector &sunDir);
	virtual ~SunLight() { }

	virtual void Preprocess();

	virtual LightSourceType GetType() const { return TYPE_SUN; }

	const luxrays::Transform &GetTransformation() const { return lightToWorld; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetRelSize(const float s) { relSize = s; }
	float GetRelSize() const { return relSize; }

	const luxrays::Vector GetDir() const { return Normalize(luxrays::Inverse(lightToWorld) * sunDir); }
	void SetDir(const luxrays::Vector &dir) { sunDir = Normalize(lightToWorld * dir); }

	void SetGain(const luxrays::Spectrum &g);
	const luxrays::Spectrum GetGain() const { return gain; }

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

	luxrays::Properties ToProperties() const;

private:
	const luxrays::Transform lightToWorld;

	luxrays::Vector sunDir;
	luxrays::Spectrum gain;
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

class TriangleLight : public LightSource {
public:
	TriangleLight() { }
	TriangleLight(const Material *mat, const u_int triangleGlobalIndex, const luxrays::ExtMesh *mesh,
		const unsigned int triangleIndex);
	virtual ~TriangleLight() { }

	virtual LightSourceType GetType() const { return TYPE_TRIANGLE; }

	void SetMaterial(const Material *mat) { lightMaterial = mat; }
	const Material *GetMaterial() const { return lightMaterial; }

	void Init();
	const luxrays::ExtMesh *GetMesh() const { return mesh; }
	unsigned int GetTriIndex() const { return triIndex; }
	float GetArea() const { return area; }

	virtual luxrays::Spectrum Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		luxrays::Point *pos, luxrays::Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

	virtual luxrays::Spectrum Illuminate(const Scene &scene, const luxrays::Point &p,
		const float u0, const float u1, const float passThroughEvent,
        luxrays::Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	luxrays::Spectrum GetRadiance(const HitPoint &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

private:
	const Material *lightMaterial;
	const luxrays::ExtMesh *mesh;
	u_int triGlobalIndex, triIndex;
	float area, invArea;
};

}

#endif	/* _SLG_LIGHT_H */
