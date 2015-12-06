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

#include "slg/lights/sky2light.h"
#include "slg/lights/data/ArHosekSkyModelData.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// SkyLight2
//------------------------------------------------------------------------------

/*
This source is published under the following 3-clause BSD license.

Copyright (c) 2012, Lukas Hosek and Alexander Wilkie
All rights reserved.

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * None of the names of the contributors may be used to endorse or promote 
      products derived from this software without specific prior written 
      permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* ============================================================================

This file is part of a sample implementation of the analytical skylight model
presented in the SIGGRAPH 2012 paper


           "An Analytic Model for Full Spectral Sky-Dome Radiance"

                                    by 

                       Lukas Hosek and Alexander Wilkie
                Charles University in Prague, Czech Republic


                        Version: 1.1, July 4th, 2012
                        
Version history:

1.1  The coefficients of the spectral model are now scaled so that the output 
     is given in physical units: W / (m^-2 * sr * nm). Also, the output of the   
     XYZ model is now no longer scaled to the range [0...1]. Instead, it is
     the result of a simple conversion from spectral data via the CIE 2 degree
     standard observer matching functions. Therefore, after multiplication 
     with 683 lm / W, the Y channel now corresponds to luminance in lm.
     
1.0  Initial release (May 11th, 2012).


Please visit http://cgg.mff.cuni.cz/projects/SkylightModelling/ to check if
an updated version of this code has been published!

============================================================================ */

static float RiCosBetween(const Vector &w1, const Vector &w2) {
	return Clamp(Dot(w1, w2), -1.f, 1.f);
}

static float ComputeCoefficient(const float elevation[6],
	const float parameters[6]) {
	return elevation[0] * parameters[0] +
		5.f * elevation[1] * parameters[1] +
		10.f * elevation[2] * parameters[2] +
		10.f * elevation[3] * parameters[3] +
		5.f * elevation[4] * parameters[4] +
		elevation[5] * parameters[5];
}

static float ComputeInterpolatedCoefficient(u_int index, u_int component,
	float turbidity, float albedo, const float elevation[6]) {
	const u_int lowTurbidity = Floor2UInt(Clamp(turbidity - 1.f, 0.f, 9.f));
	const u_int highTurbidity = min(lowTurbidity + 1U, 9U);
	const float turbidityLerp = Clamp(turbidity - highTurbidity, 0.f, 1.f);

	return Lerp(Clamp(albedo, 0.f, 1.f),
		Lerp(turbidityLerp,
			ComputeCoefficient(elevation, datasetsRGB[component][0][lowTurbidity][index]),
			ComputeCoefficient(elevation, datasetsRGB[component][0][highTurbidity][index])),
		Lerp(turbidityLerp,
			ComputeCoefficient(elevation, datasetsRGB[component][1][lowTurbidity][index]),
			ComputeCoefficient(elevation, datasetsRGB[component][1][highTurbidity][index])));
}

static void ComputeModel(float turbidity, const Spectrum &albedo, float elevation,
	Spectrum skyModel[10]) {
	const float normalizedElevation = powf(Max(0.f, elevation) * 2.f * INV_PI, 1.f / 3.f);

	const float elevations[6] = {
		powf(1.f - normalizedElevation, 5.f),
		powf(1.f - normalizedElevation, 4.f) * normalizedElevation,
		powf(1.f - normalizedElevation, 3.f) * powf(normalizedElevation, 2.f),
		powf(1.f - normalizedElevation, 2.f) * powf(normalizedElevation, 3.f),
		(1.f - normalizedElevation) * powf(normalizedElevation, 4.f),
		powf(normalizedElevation, 5.f)
	};

	float values[3];
	for (u_int i = 0; i < 10; ++i) {
		for (u_int comp = 0; comp < 3; ++comp)
			values[comp] = ComputeInterpolatedCoefficient(i, comp, turbidity, albedo.c[comp], elevations);
		skyModel[i] = Spectrum(values);
	}
}

SkyLight2::SkyLight2() : localSunDir(0.f, 0.f, 1.f), turbidity(2.2f) {
}

SkyLight2::~SkyLight2() {
}

Spectrum SkyLight2::ComputeRadiance(const Vector &w) const {
	const float cosG = RiCosBetween(w, absoluteSunDir);
	const float cosG2 = cosG * cosG;
	const float gamma = acosf(cosG);
	const float cosT = max(0.f, CosTheta(w));

	const Spectrum expTerm(dTerm * Exp(eTerm * gamma));
	const Spectrum rayleighTerm(fTerm * cosG2);
	const Spectrum mieTerm(gTerm * (1.f + cosG2) /
		Pow(Spectrum(1.f) + iTerm * (iTerm - Spectrum(2.f * cosG)), 1.5f));
	const Spectrum zenithTerm(hTerm * sqrtf(cosT));

	// 683 is a scaling factor to convert W to lm
	return 683.f * (Spectrum(1.f) + aTerm * Exp(bTerm / (cosT + .01f))) *
		(cTerm + expTerm + rayleighTerm + mieTerm + zenithTerm) * radianceTerm;
}

void SkyLight2::Preprocess() {
	absoluteSunDir = Normalize(lightToWorld * localSunDir);
	absoluteUpDir = Normalize(lightToWorld * Vector(0.f, 0.f, 1.f));

	ComputeModel(turbidity, groundAlbedo, M_PI * .5f - SphericalTheta(absoluteSunDir), model);

	aTerm = model[0];
	bTerm = model[1];
	cTerm = model[2];
	dTerm = model[3];
	eTerm = model[4];
	fTerm = model[5];
	gTerm = model[6];
	hTerm = model[7];
	iTerm = model[8];
	radianceTerm = model[9];
}

void SkyLight2::GetPreprocessedData(float *absoluteSunDirData, float *absoluteUpDirData,
		float *aTermData, float *bTermData, float *cTermData, float *dTermData,
		float *eTermData, float *fTermData, float *gTermData, float *hTermData,
		float *iTermData, float *radianceTermData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
	}

	if (absoluteUpDirData) {
		absoluteUpDirData[0] = absoluteUpDir.x;
		absoluteUpDirData[1] = absoluteUpDir.y;
		absoluteUpDirData[2] = absoluteUpDir.z;
	}

	if (aTermData) {
		aTermData[0] = aTerm.c[0];
		aTermData[1] = aTerm.c[1];
		aTermData[2] = aTerm.c[2];
	}

	if (bTermData) {
		bTermData[0] = bTerm.c[0];
		bTermData[1] = bTerm.c[1];
		bTermData[2] = bTerm.c[2];
	}

	if (cTermData) {
		cTermData[0] = cTerm.c[0];
		cTermData[1] = cTerm.c[1];
		cTermData[2] = cTerm.c[2];
	}

	if (dTermData) {
		dTermData[0] = dTerm.c[0];
		dTermData[1] = dTerm.c[1];
		dTermData[2] = dTerm.c[2];
	}

	if (eTermData) {
		eTermData[0] = eTerm.c[0];
		eTermData[1] = eTerm.c[1];
		eTermData[2] = eTerm.c[2];
	}
	
	if (fTermData) {
		fTermData[0] = fTerm.c[0];
		fTermData[1] = fTerm.c[1];
		fTermData[2] = fTerm.c[2];
	}
	
	if (gTermData) {
		gTermData[0] = gTerm.c[0];
		gTermData[1] = gTerm.c[1];
		gTermData[2] = gTerm.c[2];
	}

	if (hTermData) {
		hTermData[0] = hTerm.c[0];
		hTermData[1] = hTerm.c[1];
		hTermData[2] = hTerm.c[2];
	}

	if (iTermData) {
		iTermData[0] = iTerm.c[0];
		iTermData[1] = iTerm.c[1];
		iTermData[2] = iTerm.c[2];
	}

	if (radianceTermData) {
		radianceTermData[0] = radianceTerm.c[0];
		radianceTermData[1] = radianceTerm.c[1];
		radianceTermData[2] = radianceTerm.c[2];
	}
}

float SkyLight2::GetPower(const Scene &scene) const {
	const float envRadius = GetEnvRadius(scene);
	
	const u_int steps = 100;
	const float deltaStep = 1.f / steps;
	float power = 0.f;
	for (u_int i = 0; i < steps; ++i) {
		for (u_int j = 0; j < steps; ++j)
			power += ComputeRadiance(UniformSampleSphere(i * deltaStep + deltaStep / 2.f,
					j * deltaStep + deltaStep / 2.f)).Y();
	}
	power /= steps * steps;

	return gain.Y() * power * (4.f * M_PI * envRadius * envRadius) * 2.f * M_PI;
}

Spectrum SkyLight2::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Choose two points p1 and p2 on scene bounding sphere
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	Point p1 = worldCenter + envRadius * UniformSampleSphere(u0, u1);
	Point p2 = worldCenter + envRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(lightToWorld * (p2 - p1));

	// Compute SkyLight2 ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * envRadius * envRadius);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum SkyLight2::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float envRadius = GetEnvRadius(scene);

	*dir = Normalize(lightToWorld * UniformSampleSphere(u0, u1));

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, envRadius * envRadius -
		centerDistance + approach * approach));

	const Point emisPoint(p + (*distance) * (*dir));
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = 1.f / (4.f * M_PI);

	if (emissionPdfW)
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * envRadius * envRadius);

	return GetRadiance(scene, -(*dir));
}

Spectrum SkyLight2::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float envRadius = GetEnvRadius(scene);
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * envRadius * envRadius);
	}

	const Vector w = -dir;
	if (hasGround && (Dot(w, absoluteUpDir) < 0.f)) {
		// Higher hemisphere
		return groundColor;
	} else {
		// Lower hemisphere
		return gain * ComputeRadiance(w);
	}
}

Properties SkyLight2::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sky2"));
	props.Set(Property(prefix + ".dir")(localSunDir));
	props.Set(Property(prefix + ".turbidity")(turbidity));
	props.Set(Property(prefix + ".groundalbedo")(groundAlbedo));
	props.Set(Property(prefix + ".ground.enable")(hasGround));
	props.Set(Property(prefix + ".ground.color")(groundColor));

	return props;
}
