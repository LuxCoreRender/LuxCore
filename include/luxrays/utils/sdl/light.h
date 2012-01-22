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
#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/sdl/texmap.h"
#include "luxrays/utils/sdl/material.h"

namespace luxrays { namespace sdl {

class Scene;

enum LightSourceType {
	TYPE_IL_BF, TYPE_IL_PORTAL, TYPE_IL_IS, TYPE_IL_SKY,
	TYPE_SUN, TYPE_TRIANGLE
};

class LightSource {
public:
	virtual ~LightSource() { }

	virtual LightSourceType GetType() const = 0;

	virtual bool IsAreaLight() const { return false; }

	virtual Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const = 0;

	virtual Spectrum Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const = 0;
};

//------------------------------------------------------------------------------
// InfiniteLight implementations
//------------------------------------------------------------------------------

class InfiniteLight : public LightSource {
public:
	InfiniteLight(TexMapInstance *tx);
	virtual ~InfiniteLight() { }

	virtual void SetGain(const Spectrum &g) {
		gain = g;
	}

	virtual Spectrum GetGain() const {
		return gain;
	}

	void SetShift(const float su, const float sv) {
		shiftU = su;
		shiftV = sv;
	}

	float GetShiftU() const { return shiftU; }
	float GetShiftV() const { return shiftV; }

	const TexMapInstance *GetTexture() const { return tex; }

	virtual void Preprocess() { }

	virtual Spectrum Le(const Vector &dir) const;

	virtual Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;
	virtual Spectrum Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const;

protected:
	TexMapInstance *tex;
	float shiftU, shiftV;
	Spectrum gain;
};

class InfiniteLightBF : public InfiniteLight {
public:
	InfiniteLightBF(TexMapInstance *tx) : InfiniteLight(tx) { }

	LightSourceType GetType() const { return TYPE_IL_BF; }

	Spectrum Sample_L(const Scene *scene, const Point &p, const Normal &N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const {
		*pdf = 0;
		return Spectrum();
	}
};

class InfiniteLightPortal : public InfiniteLight {
public:
	InfiniteLightPortal(TexMapInstance *tx, const std::string &portalFileName);
	~InfiniteLightPortal();

	LightSourceType GetType() const { return TYPE_IL_PORTAL; }

	Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;
	Spectrum Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const;

private:
	ExtTriangleMesh *portals;
	std::vector<float> portalAreas;
};

class InfiniteLightIS : public InfiniteLight {
public:
	InfiniteLightIS(TexMapInstance *tx);
	~InfiniteLightIS() { delete uvDistrib; }

	LightSourceType GetType() const { return TYPE_IL_IS; }

	void Preprocess();

	Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;
	Spectrum Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3, const float u4, float *pdf, Ray *ray) const;

private:
	Distribution2D *uvDistrib;
};

//------------------------------------------------------------------------------
// Sunsky implementation
//------------------------------------------------------------------------------

class SkyLight : public InfiniteLight {
public:
	SkyLight(float turbidity, const Vector &sundir);
	virtual ~SkyLight() { }

	void Init();

	LightSourceType GetType() const { return TYPE_IL_SKY; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetSunDir(const Vector &dir) { sundir = dir; }
	const Vector &GetSunDir() const { return sundir; }

	virtual Spectrum Le(const Vector &dir) const;
	void GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const;

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

protected:
	Vector sundir;
	float turbidity;
	float thetaS;
	float phiS;
	float zenith_Y, zenith_x, zenith_y;
	float perez_Y[6], perez_x[6], perez_y[6];
};

class SunLight : public LightSource {
public:
	SunLight(float turbidity, float relSize, const Vector &sundir);
	virtual ~SunLight() { }

	void Init();

	LightSourceType GetType() const { return TYPE_SUN; }

	void SetTurbidity(const float t) { turbidity = t; }
	float GetTubidity() const { return turbidity; }

	void SetRelSize(const float s) { relSize = s; }
	float GetRelSize() const { return relSize; }

	const Vector &GetDir() const { return sundir; }
	void SetDir(const Vector &dir) { sundir = Normalize(dir); }

	void SetGain(const Spectrum &g);
	const Spectrum GetGain() const { return gain; }

	Spectrum Le(const Vector &dir) const;

	Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;
	Spectrum Sample_L(const Scene *scene, const float u0, const float u1,
		const float u2, const float u3,	const float u4, float *pdf, Ray *ray) const;

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
		*suncolorData = suncolor;
	}

protected:
	Vector sundir;
	Spectrum gain;
	float turbidity;
	float relSize;
	// XY Vectors for cone sampling
	Vector x, y;
	float thetaS, phiS, V;
	float cosThetaMax, sin2ThetaMax;
	Spectrum suncolor;
};

//------------------------------------------------------------------------------
// TriangleLight implementation
//------------------------------------------------------------------------------

class TriangleLight : public LightSource {
public:
	TriangleLight() { }
	TriangleLight(const AreaLightMaterial *mat, const unsigned int mshIndex,
		const unsigned int triangleIndex, const std::vector<ExtMesh *> &objs);

	bool IsAreaLight() const { return true; }

	LightSourceType GetType() const { return TYPE_TRIANGLE; }

	void SetMaterial(const AreaLightMaterial *mat) { lightMaterial = mat; }
	const Material *GetMaterial() const { return lightMaterial; }

	Spectrum Sample_L(const Scene *scene, const Point &p, const Normal *N,
		const float u0, const float u1, const float u2, float *pdf, Ray *shadowRay) const;
	Spectrum Sample_L(const Scene *scene,
		const float u0, const float u1, const float u2, const float u3,
		const float u4, float *pdf, Ray *ray) const;

	void Init(const std::vector<ExtMesh *> &objs);
	unsigned int GetMeshIndex() const { return meshIndex; }
	unsigned int GetTriIndex() const { return triIndex; }
	float GetArea() const { return area; }

private:
	const AreaLightMaterial *lightMaterial;
	unsigned int meshIndex, triIndex;
	float area;
};

} }

#endif	/* _LUXRAYS_SDL_LIGHT_H */
