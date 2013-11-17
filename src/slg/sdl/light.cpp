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

#include <boost/format.hpp>

#include "luxrays/core/geometry/frame.h"
#include "slg/core/spd.h"
#include "slg/core/data/sun_spect.h"
#include "slg/sdl/light.h"
#include "slg/sdl/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// This is used to scale the world radius in sun/sky/infinite lights in order to
// avoid problems with objects that are near the borderline of the world bounding sphere
const float slg::LIGHT_WORLD_RADIUS_SCALE = 10.f;

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

LightSourceDefinitions::LightSourceDefinitions() : lightTypeCount(LIGHT_SOURCE_TYPE_COUNT, 0) {
	lightsDistribution = NULL;
	lightGroupCount = 1;
}

LightSourceDefinitions::~LightSourceDefinitions() {
	BOOST_FOREACH(LightSource *l, lights)
		delete l;
}

void LightSourceDefinitions::DefineLightSource(const std::string &name, LightSource *newLight) {
	if (IsLightSourceDefined(name)) {
		const LightSource *oldLight = GetLightSource(name);

		// Update name/LightSource definition
		const u_int index = GetLightSourceIndex(name);
		lights[index] = newLight;
		lightsByName.erase(name);
		--lightTypeCount[oldLight->GetType()];
		lightsByName.insert(std::make_pair(name, newLight));
		++lightTypeCount[newLight->GetType()];

		// Delete old LightSource
		delete oldLight;
	} else {
		// Add the new LightSource
		lights.push_back(newLight);
		lightsByName.insert(std::make_pair(name, newLight));
		++lightTypeCount[newLight->GetType()];
	}
}

const LightSource *LightSourceDefinitions::GetLightSource(const std::string &name) const {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw std::runtime_error("Reference to an undefined LightSource: " + name);
	else
		return it->second;
}

LightSource *LightSourceDefinitions::GetLightSource(const std::string &name) {
	// Check if the LightSource has been already defined
	boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.find(name);

	if (it == lightsByName.end())
		throw std::runtime_error("Reference to an undefined LightSource: " + name);
	else
		return it->second;
}

u_int LightSourceDefinitions::GetLightSourceIndex(const std::string &name) const {
	return GetLightSourceIndex(GetLightSource(name));
}

u_int LightSourceDefinitions::GetLightSourceIndex(const LightSource *m) const {
	for (u_int i = 0; i < lights.size(); ++i) {
		if (m == lights[i])
			return i;
	}

	throw std::runtime_error("Reference to an undefined LightSource: " + boost::lexical_cast<std::string>(m));
}

const LightSource *LightSourceDefinitions::GetLightByType(const LightSourceType type) const {
	BOOST_FOREACH(LightSource *l, lights) {
		if (l->GetType() == type)
			return l;
	}

	return NULL;
}

const TriangleLight *LightSourceDefinitions::GetLightSourceByMeshIndex(const u_int index) const {
	return (const TriangleLight *)lights[lightIndexByMeshIndex[index]];
}

std::vector<std::string> LightSourceDefinitions::GetLightSourceNames() const {
	std::vector<std::string> names;
	names.reserve(lights.size());
	for (boost::unordered_map<std::string, LightSource *>::const_iterator it = lightsByName.begin(); it != lightsByName.end(); ++it)
		names.push_back(it->first);

	return names;
}

void LightSourceDefinitions::UpdateMaterialReferences(const Material *oldMat, const Material *newMat) {
	// Replace old material direct references with new one
	BOOST_FOREACH(LightSource *l, lights) {
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl)
			tl->UpdateMaterialReferences(oldMat, newMat);
	}
}

void LightSourceDefinitions::DeleteLightSource(const std::string &name) {
	const u_int index = GetLightSourceIndex(name);
	--lightTypeCount[lights[index]->GetType()];
	lights.erase(lights.begin() + index);
	lightsByName.erase(name);
}

void LightSourceDefinitions::Preprocess(const Scene *scene) {
	// Update lightGroupCount, envLightSources, intersecableLightSources,
	// lightIndexByMeshIndex and lightsDistribution

	lightGroupCount = 0;
	intersecableLightSources.clear();
	envLightSources.clear();

	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene->dataSet->GetBSphere().rad * 1.01f;
	const float iWorldRadius2 = 1.f / (worldRadius * worldRadius);
	float totalPower = 0.f;
	vector<float> lightPower;
	lightPower.reserve(lights.size());

	lightIndexByMeshIndex.resize(scene->objDefs.GetSize(), NULL_INDEX);
	for (u_int i = 0; i < lights.size(); ++i) {
		LightSource *l = lights[i];
		// Initialize the light source index
		l->SetSceneIndex(i);

		lightGroupCount = Max(lightGroupCount, l->GetID() + 1);

		float power = l->GetPower(*scene);
		// In order to avoid over-sampling of distant lights
		if (l->IsEnvironmental()) {
			power *= iWorldRadius2;			
			envLightSources.push_back((EnvLightSource *)l);
		}
		lightPower.push_back(power);
		totalPower += power;

		// Build lightIndexByMeshIndex
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl) {
			lightIndexByMeshIndex[tl->GetMeshIndex()] = i;
			intersecableLightSources.push_back(tl);
		}
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	if (totalPower == 0.f) {
		// To handle a corner case
		lightsDistribution = NULL;
	} else
		lightsDistribution = new Distribution1D(&lightPower[0], lights.size());
}

LightSource *LightSourceDefinitions::SampleAllLights(const float u, float *pdf) const {
	if (lightsDistribution) {
		// Power based light strategy
		const u_int lightIndex = lightsDistribution->SampleDiscrete(u, pdf);
		assert ((lightIndex >= 0) && (lightIndex < GetSize()));

		return lights[lightIndex];
	} else {
		// All uniform light strategy
		const u_int lightIndex = Min<u_int>(Floor2UInt(lights.size() * u), lights.size() - 1);

		return lights[lightIndex];
	}
}

float LightSourceDefinitions::SampleAllLightPdf(const LightSource *light) const {
	if (lightsDistribution) {
		// Power based light strategy
		return lightsDistribution->Pdf(light->GetSceneIndex());
	} else {
		// All uniform light strategy
		return 1.f /  GetSize();
	}
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight(const Transform &l2w, const ImageMap *imgMap) :
	EnvLightSource(l2w), imageMap(imgMap), mapping(1.f, 1.f, 0.f, 0.f) {
	if (imageMap->GetChannelCount() == 1)
		imageMapDistribution = new Distribution2D(imageMap->GetPixels(), imageMap->GetWidth(), imageMap->GetHeight());
	else {
		const float *pixels = imageMap->GetPixels();
		float *data = new float[imageMap->GetWidth() * imageMap->GetHeight()];
		for (u_int y = 0; y < imageMap->GetHeight(); ++y) {
			for (u_int x = 0; x < imageMap->GetWidth(); ++x) {
				const u_int index = x + y * imageMap->GetWidth();
				const float *pixel = &pixels[index * imageMap->GetChannelCount()];
				
				data[index] = Spectrum(pixel).Y();
			}
		}

		imageMapDistribution = new Distribution2D(data, imageMap->GetWidth(), imageMap->GetHeight());
		delete data;
	}
}

InfiniteLight::~InfiniteLight() {
	delete imageMapDistribution;
}

float InfiniteLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * (4.f * M_PI * M_PI * worldRadius * worldRadius) *
		imageMap->GetSpectrumMeanY();
}

Spectrum InfiniteLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);
	const UV uv(SphericalPhi(localDir) * INV_TWOPI, SphericalTheta(localDir) * INV_PI);

	const float distPdf = imageMapDistribution->Pdf(uv.u, uv.v);
	if (directPdfA)
		*directPdfA = distPdf / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * imageMap->GetSpectrum(mapping.Map(uv));
}

Spectrum InfiniteLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	// Choose p2 on scene bounding sphere according importance sampling
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);

	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	Point p1 = worldCenter + worldRadius * SphericalDirection(sinf(theta), cosf(theta), phi);

	// Choose p2 on scene bounding sphere
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(lightToWorld * (p2 - p1));

	// Compute InfiniteAreaLight ray weight
	*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = distPdf / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum InfiniteLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	float uv[2];
	float distPdf;
	imageMapDistribution->SampleContinuous(u0, u1, uv, &distPdf);

	const float phi = uv[0] * 2.f * M_PI;
	const float theta = uv[1] * M_PI;
	*dir = Normalize(lightToWorld * SphericalDirection(sinf(theta), cosf(theta), phi));

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	const Point emisPoint(p + (*distance) * (*dir));
	const Normal emisNormal(Normalize(worldCenter - emisPoint));

	const float cosAtLight = Dot(emisNormal, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();
	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = distPdf / (4.f * M_PI);

	if (emissionPdfW)
		*emissionPdfW = distPdf / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return gain * imageMap->GetSpectrum(mapping.Map(UV(uv[0], uv[1])));
}

Properties InfiniteLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene." + GetName();
	Properties props;

	props.Set(Property(prefix + ".file")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imageMap)).str() + ".exr"));
	props.Set(Property(prefix + ".gain")(gain));
	props.Set(Property(prefix + ".shift")(mapping.uDelta, mapping.vDelta));
	props.Set(Property(prefix + ".transformation")(lightToWorld.m));
	props.Set(Property(prefix + ".samples")(samples));
	props.Set(Property(prefix + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property(prefix + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property(prefix + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

//------------------------------------------------------------------------------
// SkyLight
//------------------------------------------------------------------------------

static float PerezBase(const float lam[6], const float theta, const float gamma) {
	return (1.f + lam[1] * expf(lam[2] / cosf(theta))) *
		(1.f + lam[3] * expf(lam[4] * gamma)  + lam[5] * cosf(gamma) * cosf(gamma));
}

static float RiAngleBetween(const float thetav, const float phiv,
		const float theta, const float phi) {
	const float cospsi = sinf(thetav) * sinf(theta) * cosf(phi - phiv) + cosf(thetav) * cosf(theta);
	if (cospsi >= 1.f)
		return 0.f;
	if (cospsi <= -1.f)
		return M_PI;
	return acosf(cospsi);
}

static void ChromaticityToSpectrum(float Y, float x, float y, Spectrum *s) {
	float X, Z;
	
	if (y != 0.f)
		X = (x / y) * Y;
	else
		X = 0.f;
	
	if (y != 0.f && Y != 0.f)
		Z = (1.f - x - y) / y * Y;
	else
		Z = 0.f;

	// Assuming sRGB (D65 illuminant)
	s->r =  3.2410f * X - 1.5374f * Y - 0.4986f * Z;
	s->g = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
	s->b =  0.0556f * X - 0.2040f * Y + 1.0570f * Z;
}

SkyLight::SkyLight(const luxrays::Transform &l2w, float turb,
		const Vector &sd) : EnvLightSource(l2w) {
	turbidity = turb;
	sunDir = Normalize(lightToWorld * sd);
}

float SkyLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
	
	const u_int steps = 100;
	const float deltaStep = 2.f / steps;
	float phi = 0.f, power = 0.f;
	for (u_int i = 0; i < steps; ++i) {
		float cosTheta = -1.f;
		for (u_int j = 0; j < steps; ++j) {
			float theta = acosf(cosTheta);
			float gamma = RiAngleBetween(theta, phi, thetaS, phiS);
			theta = Min<float>(theta, M_PI * .5f - .001f);
			power += zenith_Y * PerezBase(perez_Y, theta, gamma);
			cosTheta += deltaStep;
		}

		phi += deltaStep * M_PI;
	}
	power /= steps * steps;

	return power * (4.f * M_PI * worldRadius * worldRadius) * 2.f * M_PI;
}

void SkyLight::Preprocess() {
	thetaS = SphericalTheta(sunDir);
	phiS = SphericalPhi(sunDir);

	float aconst = 1.0f;
	float bconst = 1.0f;
	float cconst = 1.0f;
	float dconst = 1.0f;
	float econst = 1.0f;

	float theta2 = thetaS*thetaS;
	float theta3 = theta2*thetaS;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * thetaS);
	zenith_Y = (4.0453f * T - 4.9710f) * tan(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 0.06f;

	zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * thetaS) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * thetaS + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * thetaS + 0.25886f);

	zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * thetaS) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * thetaS  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * thetaS  + 0.26688f);

	perez_Y[1] = (0.1787f * T  - 1.4630f) * aconst;
	perez_Y[2] = (-0.3554f * T  + 0.4275f) * bconst;
	perez_Y[3] = (-0.0227f * T  + 5.3251f) * cconst;
	perez_Y[4] = (0.1206f * T  - 2.5771f) * dconst;
	perez_Y[5] = (-0.0670f * T  + 0.3703f) * econst;

	perez_x[1] = (-0.0193f * T  - 0.2592f) * aconst;
	perez_x[2] = (-0.0665f * T  + 0.0008f) * bconst;
	perez_x[3] = (-0.0004f * T  + 0.2125f) * cconst;
	perez_x[4] = (-0.0641f * T  - 0.8989f) * dconst;
	perez_x[5] = (-0.0033f * T  + 0.0452f) * econst;

	perez_y[1] = (-0.0167f * T  - 0.2608f) * aconst;
	perez_y[2] = (-0.0950f * T  + 0.0092f) * bconst;
	perez_y[3] = (-0.0079f * T  + 0.2102f) * cconst;
	perez_y[4] = (-0.0441f * T  - 1.6537f) * dconst;
	perez_y[5] = (-0.0109f * T  + 0.0529f) * econst;

	zenith_Y /= PerezBase(perez_Y, 0, thetaS);
	zenith_x /= PerezBase(perez_x, 0, thetaS);
	zenith_y /= PerezBase(perez_y, 0, thetaS);
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * .5f) - .001f);
	const float gamma = RiAngleBetween(theta, phi, thetaS, phiS);

	// Compute xyY values
	const float x = zenith_x * PerezBase(perez_x, theta_fin, gamma);
	const float y = zenith_y * PerezBase(perez_y, theta_fin, gamma);
	const float Y = zenith_Y * PerezBase(perez_Y, theta_fin, gamma);

	ChromaticityToSpectrum(Y, x, y, spect);
}

Spectrum SkyLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Choose two points p1 and p2 on scene bounding sphere
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	Point p1 = worldCenter + worldRadius * UniformSampleSphere(u0, u1);
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize(lightToWorld * (p2 - p1));

	// Compute InfiniteAreaLight ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum SkyLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	*dir = Normalize(lightToWorld * UniformSampleSphere(u0, u1));

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
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
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return GetRadiance(scene, -(*dir));
}

Spectrum SkyLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const Vector localDir = Normalize(Inverse(lightToWorld) * -dir);
	const float theta = SphericalTheta(localDir);
	const float phi = SphericalPhi(localDir);
	Spectrum s;
	GetSkySpectralRadiance(theta, phi, &s);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * s;
}

Properties SkyLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene." + GetName();
	Properties props;

	props.Set(Property(prefix + ".dir")(GetSunDir()));
	props.Set(Property(prefix + ".gain")(gain));
	props.Set(Property(prefix + ".turbidity")(turbidity));
	props.Set(Property(prefix + ".transformation")(lightToWorld.m));
	props.Set(Property(prefix + ".samples")(samples));
	props.Set(Property(prefix + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property(prefix + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property(prefix + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

SunLight::SunLight(const luxrays::Transform &l2w,
		float turb, float size,	const Vector &sd) :
		EnvLightSource(l2w) {
	turbidity = turb;
	sunDir = Normalize(lightToWorld * sd);
	relSize = size;
}

float SunLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return sunColor.Y() * (M_PI * worldRadius * worldRadius) * 2.f * M_PI * sin2ThetaMax / (relSize * relSize);
}

void SunLight::Preprocess() {
	CoordinateSystem(sunDir, &x, &y);

	// Values from NASA Solar System Exploration page
	// http://solarsystem.nasa.gov/planets/profile.cfm?Object=Sun&Display=Facts&System=Metric
	const float sunRadius = 695500.f;
	const float sunMeanDistance = 149600000.f;

	const float sunSize = relSize * sunRadius;
	if (sunSize <= sunMeanDistance) {
		sin2ThetaMax = sunSize / sunMeanDistance;
		sin2ThetaMax *= sin2ThetaMax;
		cosThetaMax = sqrtf(1.f - sin2ThetaMax);
	} else {
		cosThetaMax = 0.f;
		sin2ThetaMax = 1.f;
	}

	Vector wh = Normalize(sunDir);
	phiS = SphericalPhi(wh);
	thetaS = SphericalTheta(wh);

	// NOTE - lordcrc - sun_k_oWavelengths contains 64 elements, while sun_k_oAmplitudes contains 65?!?
	IrregularSPD k_oCurve(sun_k_oWavelengths, sun_k_oAmplitudes, 64);
	IrregularSPD k_gCurve(sun_k_gWavelengths, sun_k_gAmplitudes, 4);
	IrregularSPD k_waCurve(sun_k_waWavelengths, sun_k_waAmplitudes, 13);

	RegularSPD solCurve(sun_solAmplitudes, 380, 750, 38);  // every 5 nm

	float beta = 0.04608365822050f * turbidity - 0.04586025928522f;
	float tauR, tauA, tauO, tauG, tauWA;

	float m = 1.f / (cosf(thetaS) + 0.00094f * powf(1.6386f - thetaS, 
		-1.253f));  // Relative Optical Mass

	int i;
	float lambda;
	// NOTE - lordcrc - SPD stores data internally, no need for Ldata to stick around
	float Ldata[91];
	for(i = 0, lambda = 350.f; i < 91; ++i, lambda += 5.f) {
			// Rayleigh Scattering
		tauR = expf( -m * 0.008735f * powf(lambda / 1000.f, -4.08f));
			// Aerosol (water + dust) attenuation
			// beta - amount of aerosols present
			// alpha - ratio of small to large particle sizes. (0:4,usually 1.3)
		const float alpha = 1.3f;
		tauA = expf(-m * beta * powf(lambda / 1000.f, -alpha));  // lambda should be in um
			// Attenuation due to ozone absorption
			// lOzone - amount of ozone in cm(NTP)
		const float lOzone = .35f;
		tauO = expf(-m * k_oCurve.sample(lambda) * lOzone);
			// Attenuation due to mixed gases absorption
		tauG = expf(-1.41f * k_gCurve.sample(lambda) * m / powf(1.f +
			118.93f * k_gCurve.sample(lambda) * m, 0.45f));
			// Attenuation due to water vapor absorbtion
			// w - precipitable water vapor in centimeters (standard = 2)
		const float w = 2.0f;
		tauWA = expf(-0.2385f * k_waCurve.sample(lambda) * w * m /
		powf(1.f + 20.07f * k_waCurve.sample(lambda) * w * m, 0.45f));

		Ldata[i] = (solCurve.sample(lambda) *
			tauR * tauA * tauO * tauG * tauWA);
	}

	RegularSPD LSPD(Ldata, 350, 800, 91);
	// Note: (1000000000.0f / (M_PI * 100.f * 100.f)) is for compatibility with past scene
	sunColor = gain * LSPD.ToRGB() / (1000000000.0f / (M_PI * 100.f * 100.f));
}

Spectrum SunLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad;

	// Set ray origin and direction for infinite light ray
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = worldCenter + worldRadius * (sunDir + d1 * x + d2 * y);
	*dir = -UniformSampleCone(u2, u3, cosThetaMax, x, y, sunDir);
	*emissionPdfW = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(sunDir, -(*dir));

	return sunColor;
}

Spectrum SunLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = UniformSampleCone(u0, u1, cosThetaMax, x, y, sunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = Dot(sunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return Spectrum();

	*distance = numeric_limits<float>::infinity();
	*directPdfW = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad;
		*emissionPdfW =  UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);
	}
	
	return sunColor;
}

Spectrum SunLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	if ((cosThetaMax < 1.f) && (Dot(-dir, sunDir) > cosThetaMax)) {
		if (directPdfA)
			*directPdfA = UniformConePdf(cosThetaMax);

		if (emissionPdfW) {
			const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad;
			*emissionPdfW = UniformConePdf(cosThetaMax) / (M_PI * worldRadius * worldRadius);
		}

		return sunColor;
	}

	return Spectrum();
}

Properties SunLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene." + GetName();
	Properties props;

	props.Set(Property(prefix + ".dir")(GetDir()));
	props.Set(Property(prefix + ".gain")(gain));
	props.Set(Property(prefix + ".turbidity")(turbidity));
	props.Set(Property(prefix + ".relsize")(relSize));
	props.Set(Property(prefix + ".transformation")(lightToWorld.m));
	props.Set(Property(prefix + ".samples")(samples));
	props.Set(Property(prefix + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property(prefix + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property(prefix + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight(const Material *mat, const ExtMesh *m,
		const u_int mIndex, const unsigned int tIndex) : IntersecableLightSource(mat) {
	mesh = m;
	meshIndex = mIndex;
	triangleIndex = tIndex;
}

float TriangleLight::GetPower(const Scene &scene) const {
	return area * M_PI * lightMaterial->GetEmittedRadianceY();
}

void TriangleLight::Preprocess() {
	area = mesh->GetTriangleArea(triangleIndex);
	invArea = 1.f / area;
}

Spectrum TriangleLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Origin
	float b0, b1, b2;
	mesh->Sample(triangleIndex, u0, u1, orig, &b0, &b1, &b2);

	// Build the local frame
	const Normal N = mesh->GetGeometryNormal(triangleIndex); // Light sources are supposed to be flat
	Frame frame(N);

	Vector localDirOut = CosineSampleHemisphere(u2, u3, emissionPdfW);
	if (*emissionPdfW == 0.f)
		return Spectrum();
	*emissionPdfW *= invArea;

	// Cannot really not emit the particle, so just bias it to the correct angle
	localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);

	// Direction
	*dir = frame.ToWorld(localDirOut);

	if (directPdfA)
		*directPdfA = invArea;

	if (cosThetaAtLight)
		*cosThetaAtLight = localDirOut.z;

	const UV triUV = mesh->InterpolateTriUV(triangleIndex, b1, b2);
	const HitPoint hitPoint = { Vector(-N), *orig, triUV, N, N, passThroughEvent, false };

	return lightMaterial->GetEmittedRadiance(hitPoint, invArea) * localDirOut.z;
}

Spectrum TriangleLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(triangleIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetGeometryNormal(triangleIndex); // Light sources are supposed to be flat

	*dir = samplePoint - p;
	const float distanceSquared = dir->LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir /= (*distance);

	const float cosAtLight = Dot(sampleN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	*directPdfW = invArea * distanceSquared / cosAtLight;

	if (emissionPdfW)
		*emissionPdfW = invArea * cosAtLight * INV_PI;

	const UV triUV = mesh->InterpolateTriUV(triangleIndex, b1, b2);
	const HitPoint hitPoint = { Vector(-sampleN), samplePoint, triUV, sampleN, sampleN, passThroughEvent, false };

	return lightMaterial->GetEmittedRadiance(hitPoint, invArea);
}

Spectrum TriangleLight::GetRadiance(const HitPoint &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	const float cosOutLight = Dot(hitPoint.geometryN, hitPoint.fixedDir);
	if (cosOutLight <= 0.f)
		return Spectrum();

	if (directPdfA)
		*directPdfA = invArea;

	if (emissionPdfW)
		*emissionPdfW = cosOutLight * INV_PI * invArea;

	return lightMaterial->GetEmittedRadiance(hitPoint, invArea);
}
