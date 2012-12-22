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

#ifndef _LUXRAYS_SDL_LIGHT_H
#define	_LUXRAYS_SDL_LIGHT_H

#include "luxrays/luxrays.h"
#include "luxrays/utils/core/spectrum.h"
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/sdl/texture.h"
#include "luxrays/utils/sdl/material.h"

namespace luxrays { namespace sdl {

class Scene;

enum LightSourceType {
	TYPE_IL, TYPE_IL_SKY, TYPE_SUN, TYPE_TRIANGLE
};

//------------------------------------------------------------------------------
// LightSource implementation
//------------------------------------------------------------------------------

class LightSource {
public:
	LightSource() { }
	virtual ~LightSource() { }

	virtual LightSourceType GetType() const = 0;

	virtual bool IsAreaLight() const { return false; }
	virtual bool IsEnvironmental() const { return false; }

	// Emits particle from the light
	virtual Spectrum Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *pos, Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const {
		throw std::runtime_error("Internal error, called LightSource::Emit()");
	}

	// Illuminates a point in the scene
    virtual Spectrum Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const {
		throw std::runtime_error("Internal error, called LightSource::Illuminate()");
	}

	// Returns radiance for ray hitting the light source
	virtual Spectrum GetRadiance(const Scene *scene,
			const Vector &dir, const Point &hitPoint,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const {
		throw std::runtime_error("Internal error, called LightSource::GetRadiance()");
	}
};

//------------------------------------------------------------------------------
// InfiniteLightBase implementation
//------------------------------------------------------------------------------

class InfiniteLightBase : public LightSource {
public:
	InfiniteLightBase() : gain(1.f, 1.f, 1.f) { }
	~InfiniteLightBase() { }

	virtual void Preprocess() { }

	bool IsEnvironmental() const { return true; }

	void SetGain(const Spectrum &g) {
		gain = g;
	}

	Spectrum GetGain() const {
		return gain;
	}

	Spectrum Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *pos, Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

    Spectrum Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	virtual Spectrum GetRadiance(const Scene *scene,
			const Vector &dir, const Point &hitPoint,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const = 0;

protected:
	Spectrum gain;
};

//------------------------------------------------------------------------------
// InfiniteLight implementation
//------------------------------------------------------------------------------

class InfiniteLight : public InfiniteLightBase {
public:
	InfiniteLight(ImageMapInstance *tx);
	~InfiniteLight() { }

	LightSourceType GetType() const { return TYPE_IL; }

	void SetShift(const float su, const float sv) {
		shiftU = su;
		shiftV = sv;
	}

	float GetShiftU() const { return shiftU; }
	float GetShiftV() const { return shiftV; }

	const ImageMapInstance *GetTexture() const { return tex; }

	Spectrum GetRadiance(const Scene *scene,
			const Vector &dir, const Point &hitPoint,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

private:
	ImageMapInstance *tex;
	float shiftU, shiftV;
};

//------------------------------------------------------------------------------
// Sky implementation
//------------------------------------------------------------------------------

class SkyLight : public InfiniteLightBase {
public:
	SkyLight(float turbidity, const Vector &sundir);
	virtual ~SkyLight() { }

	void Preprocess();

	LightSourceType GetType() const { return TYPE_IL_SKY; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetSunDir(const Vector &dir) { sundir = dir; }
	const Vector &GetSunDir() const { return sundir; }

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

	Spectrum GetRadiance(const Scene *scene,
			const Vector &dir, const Point &hitPoint,
			float *directPdfA = NULL, float *emissionPdfW = NULL) const;

private:
	void GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const;

	Vector sundir;
	float turbidity;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

class SunLight : public LightSource {
public:
	SunLight(float turbidity, float relSize, const Vector &sunDir);
	virtual ~SunLight() { }

	void Preprocess();

	LightSourceType GetType() const { return TYPE_SUN; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetRelSize(const float s) { relSize = s; }
	float GetRelSize() const { return relSize; }

	const Vector &GetDir() const { return sunDir; }
	void SetDir(const Vector &dir) { sunDir = Normalize(dir); }

	void SetGain(const Spectrum &g);
	const Spectrum GetGain() const { return gain; }

	void GetInitData(Vector *xData, Vector *yData,
		float *thetaSData, float *phiSData, float *VData,
		float *cosThetaMaxData, float *sin2ThetaMaxData,
		Spectrum *suncolorData) const {
		*xData = x;
		*yData = y;
		*thetaSData = thetaS;
		*phiSData = phiS;
		*VData = V;
		*cosThetaMaxData = cosThetaMax;
		*sin2ThetaMaxData = sin2ThetaMax;
		*suncolorData = sunColor;
	}

	Spectrum Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *pos, Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

	Spectrum Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	Spectrum GetRadiance(const Scene *scene,
			const Vector &dir,
			const Point &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

private:
	Vector sunDir;
	Spectrum gain;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float thetaS, phiS, V;
	float cosThetaMax, sin2ThetaMax;
	Spectrum sunColor;
};

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public LightSource {
public:
	TriangleLight() { }
	TriangleLight(const Material *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const std::vector<ExtMesh *> &objs);

	bool IsAreaLight() const { return true; }

	LightSourceType GetType() const { return TYPE_TRIANGLE; }

	void SetMaterial(const Material *mat) { lightMaterial = mat; }
	const Material *GetMaterial() const { return lightMaterial; }

	void Init(const std::vector<ExtMesh *> &objs);
	unsigned int GetMeshIndex() const { return meshIndex; }
	unsigned int GetTriIndex() const { return triIndex; }
	float GetArea() const { return area; }

	Spectrum Emit(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		Point *pos, Vector *dir,
		float *emissionPdfW, float *directPdfA = NULL, float *cosThetaAtLight = NULL) const;

	Spectrum Illuminate(const Scene *scene, const Point &p,
		const float u0, const float u1, const float u2,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW = NULL, float *cosThetaAtLight = NULL) const;

	Spectrum GetRadiance(const Scene *scene,
			const Vector &dir,
			const Point &hitPoint,
			float *directPdfA = NULL,
			float *emissionPdfW = NULL) const;

private:
	const Material *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area, invArea;
};

} }

#endif	/* _LUXRAYS_SDL_LIGHT_H */
