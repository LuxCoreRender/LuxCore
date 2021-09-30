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

#include "slg/lights/light.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// LightSource
//------------------------------------------------------------------------------

Properties LightSource::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();

	Properties props;
	if (volume)
		props.Set(Property(prefix + ".volume")(volume->GetName()));

	return props;
}

void LightSource::UpdateVolumeReferences(const Volume *oldVol, const Volume *newVol) {
	if (volume == oldVol)
		volume = (const Volume *)newVol;
}

string LightSource::LightSourceType2String(const LightSourceType type) {
	switch (type) {
		case TYPE_IL: return "INFINITE";
		case TYPE_IL_SKY: return "SKY";
		case TYPE_SUN: return "SUN";
		case TYPE_TRIANGLE: return "TRIANGLELIGHT";
		case TYPE_POINT: return "POINT";
		case TYPE_MAPPOINT: return "MAPPOINT";
		case TYPE_SPOT: return "SPOTLIGHT";
		case TYPE_PROJECTION: return "PROJECTION";
		case TYPE_IL_CONSTANT: return "CONSTANTINFINITE";
		case TYPE_SHARPDISTANT: return "SHARPDISTANT";
		case TYPE_DISTANT: return "DISTANT";
		case TYPE_IL_SKY2: return "SKY2";
		case TYPE_LASER: return "LASER";
		case TYPE_SPHERE: return "SPHERE";
		case TYPE_MAPSPHERE: return "MAPSPHERE";
		default:
			throw runtime_error("Unknown light source type in LightSource::LightSourceType2String(): " + ToString(type));
	}
}

//------------------------------------------------------------------------------
// NotIntersectableLightSource
//------------------------------------------------------------------------------

void NotIntersectableLightSource::Preprocess() {
	temperatureScale = (temperature >= 0.f) ? TemperatureToWhitePoint(temperature, normalizeTemperature) : Spectrum(1.f);
}

Properties NotIntersectableLightSource::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();

	return LightSource::ToProperties(imgMapCache, useRealFileName) <<
			Property(prefix + ".gain")(gain) <<
			Property(prefix + ".transformation")(lightToWorld.m) <<
			Property(prefix + ".id")(id) <<
			Property(prefix + ".temperature")(temperature) <<
			Property(prefix + ".temperature.normalize")(normalizeTemperature);
}

//------------------------------------------------------------------------------
// InfiniteLightSource
//------------------------------------------------------------------------------

float InfiniteLightSource::GetEnvRadius(const Scene &scene) {
	return LIGHT_WORLD_RADIUS_SCALE * scene.sceneBSphere.rad;
}

Properties InfiniteLightSource::ToProperties(const ImageMapCache &imgMapCache, const bool useRealFileName) const {
	const string prefix = "scene.lights." + GetName();

	return NotIntersectableLightSource::ToProperties(imgMapCache, useRealFileName) <<
			Property(prefix + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse) <<
			Property(prefix + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy) <<
			Property(prefix + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular);
}

//------------------------------------------------------------------------------
// InfiniteLightSource
//------------------------------------------------------------------------------

void EnvLightSource::ToLatLongMapping(const Vector &w, float *s, float *t, float *pdf) {
	const float theta = SphericalTheta(w);

	*s = SphericalPhi(w) * INV_TWOPI;
	*t = theta * INV_PI;

	if (pdf) {
		const float sinTheta = sinf(theta);
		*pdf = (sinTheta > 0.f) ? (INV_TWOPI * INV_PI / sinTheta) : 0.f;
		assert (!isnan(*pdf) && !isinf(*pdf) && (*pdf >= 0.f));
	}
}

void EnvLightSource::FromLatLongMapping(const float s, const float t, Vector *w, float *pdf) {
	const float phi = s * 2.f * M_PI;
	const float theta = t * M_PI;
	const float sinTheta = sinf(theta);

	*w = SphericalDirection(sinTheta, cosf(theta), phi);

	if (pdf) {
		*pdf = (sinTheta > 0.f) ? (INV_TWOPI * INV_PI / sinTheta) : 0.f;
		assert (!isnan(*pdf) && !isinf(*pdf) && (*pdf >= 0.f));
	}
}
