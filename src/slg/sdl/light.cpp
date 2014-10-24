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

#include <algorithm>
#include <boost/format.hpp>

#include "luxrays/core/geometry/frame.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/sdl/light.h"
#include "slg/sdl/scene.h"
#include "slg/sdl/lightdata/ArHosekSkyModelData.h"

#include "slg/core/data/sun_spect.h"

using namespace std;
using namespace luxrays;
using namespace slg;

// This is used to scale the world radius in sun/sky/infinite lights in order to
// avoid problems with objects that are near the borderline of the world bounding sphere
const float slg::LIGHT_WORLD_RADIUS_SCALE = 10.f;

//------------------------------------------------------------------------------
// LightStrategy
//------------------------------------------------------------------------------

LightSource *LightStrategy::SampleLights(const float u, float *pdf) const {
		const u_int lightIndex = lightsDistribution->SampleDiscrete(u, pdf);
		assert ((lightIndex >= 0) && (lightIndex < scene->lightDefs.GetSize()));

		return  scene->lightDefs.GetLightSources()[lightIndex];
	}

float LightStrategy::SampleLightPdf(const LightSource *light) const {
	return lightsDistribution->Pdf(light->lightSceneIndex);
}

//------------------------------------------------------------------------------
// LightStrategyUniform
//------------------------------------------------------------------------------

void LightStrategyUniform::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);
	
	const u_int lightCount = scene->lightDefs.GetSize();
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		lightPower.push_back(l->GetImportance());
	}

	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

//------------------------------------------------------------------------------
// LightStrategyPower
//------------------------------------------------------------------------------

void LightStrategyPower::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);

	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene->dataSet->GetBSphere().rad * 1.01f;
	const float iWorldRadius2 = 1.f / (worldRadius * worldRadius);

	const u_int lightCount = scene->lightDefs.GetSize();
	float totalPower = 0.f;
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		float power = l->GetPower(*scene);
		// In order to avoid over-sampling of distant lights
		if (l->IsInfinite())
			power *= iWorldRadius2;			
		lightPower.push_back(power * l->GetImportance());
		totalPower += power;
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

//------------------------------------------------------------------------------
// LightStrategyLogPower
//------------------------------------------------------------------------------

void LightStrategyLogPower::Preprocess(const Scene *scn) {
	LightStrategy::Preprocess(scn);

	const u_int lightCount = scene->lightDefs.GetSize();
	float totalPower = 0.f;
	vector<float> lightPower;
	lightPower.reserve(lightCount);

	for (u_int i = 0; i < lightCount; ++i) {
		const LightSource *l = scene->lightDefs.GetLightSource(i);

		float power = logf(1.f + l->GetPower(*scene));

		lightPower.push_back(power * l->GetImportance());
		totalPower += power;
	}

	// Build the data to power based light sampling
	delete lightsDistribution;
	lightsDistribution = new Distribution1D(&lightPower[0], lightCount);
}

//------------------------------------------------------------------------------
// LightSourceDefinitions
//------------------------------------------------------------------------------

LightSourceDefinitions::LightSourceDefinitions() : lightTypeCount(LIGHT_SOURCE_TYPE_COUNT, 0) {
	lightStrategy = new LightStrategyPower();
	lightGroupCount = 1;
}

LightSourceDefinitions::~LightSourceDefinitions() {
	delete lightStrategy;
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
	// Replace old material direct references with new ones
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

void LightSourceDefinitions::SetLightStrategy(const LightStrategyType type) {
	if (lightStrategy && (lightStrategy->GetType() == type))
		return;

	delete lightStrategy;
	lightStrategy = NULL;

	switch (type) {
		case TYPE_UNIFORM:
			lightStrategy = new LightStrategyUniform();
			break;
		case TYPE_POWER:
			lightStrategy = new LightStrategyPower();
			break;
		case TYPE_LOG_POWER:
			lightStrategy = new LightStrategyLogPower();
			break;
		default:
			throw runtime_error("Unknown LightStrategyType in LightSourceDefinitions::SetLightStrategy(): " + ToString(type));
	}
}

void LightSourceDefinitions::Preprocess(const Scene *scene) {
	// Update lightGroupCount, envLightSources, intersectableLightSources,
	// lightIndexByMeshIndex and lightsDistribution

	lightGroupCount = 0;
	intersectableLightSources.clear();
	envLightSources.clear();

	lightIndexByMeshIndex.resize(scene->objDefs.GetSize(), NULL_INDEX);
	for (u_int i = 0; i < lights.size(); ++i) {
		LightSource *l = lights[i];
		// Initialize the light source index
		l->lightSceneIndex = i;

		lightGroupCount = Max(lightGroupCount, l->GetID() + 1);

		// Update the list of env. lights
		if (l->IsEnvironmental())
			envLightSources.push_back((EnvLightSource *)l);

		// Build lightIndexByMeshIndex
		TriangleLight *tl = dynamic_cast<TriangleLight *>(l);
		if (tl) {
			lightIndexByMeshIndex[tl->meshIndex] = i;
			intersectableLightSources.push_back(tl);
		}
	}

	// Build the light strategy
	lightStrategy->Preprocess(scene);
}

//------------------------------------------------------------------------------
// NotIntersectableLightSource
//------------------------------------------------------------------------------

Properties NotIntersectableLightSource::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props;

	props.Set(Property(prefix + ".gain")(gain));
	props.Set(Property(prefix + ".transformation")(lightToWorld.m));
	props.Set(Property(prefix + ".samples")(samples));
	props.Set(Property(prefix + ".id")(id));

	return props;
}

//------------------------------------------------------------------------------
// InfiniteLightSource
//------------------------------------------------------------------------------

Properties InfiniteLightSource::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".visibility.indirect.diffuse.enable")(isVisibleIndirectDiffuse));
	props.Set(Property(prefix + ".visibility.indirect.glossy.enable")(isVisibleIndirectGlossy));
	props.Set(Property(prefix + ".visibility.indirect.specular.enable")(isVisibleIndirectSpecular));

	return props;
}

//------------------------------------------------------------------------------
// PointLight
//------------------------------------------------------------------------------

PointLight::PointLight() : localPos(0.f), color(1.f), power(0.f), efficency(0.f) {
}

PointLight::~PointLight() {
}

void PointLight::Preprocess() {
	emittedFactor = gain * color * (power * efficency / (4.f * M_PI * color.Y()));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	absolutePos = lightToWorld * localPos;
}

void PointLight::GetPreprocessedData(float *localPosData, float *absolutePosData,
		float *emittedFactorData) const {
	if (localPosData) {
		localPosData[0] = localPos.x;
		localPosData[1] = localPos.y;
		localPosData[2] = localPos.z;
	}

	if (absolutePosData) {
		absolutePosData[0] = absolutePos.x;
		absolutePosData[1] = absolutePos.y;
		absolutePosData[2] = absolutePos.z;
	}

	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}
}

float PointLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() * 4.f * M_PI;
}

Spectrum PointLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*orig = absolutePos;
	*dir = UniformSampleSphere(u0, u1);
	*emissionPdfW = 1.f / (4.f * M_PI);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	return emittedFactor;
}

Spectrum PointLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector toLight(absolutePos - p);
	const float distanceSquared = toLight.LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir = toLight / *distance;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	*directPdfW = distanceSquared;

	if (emissionPdfW)
		*emissionPdfW = 1.f / (4.f * M_PI);

	return emittedFactor;
}

Properties PointLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("point"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));

	return props;
}

//------------------------------------------------------------------------------
// MapPointLight
//------------------------------------------------------------------------------

MapPointLight::MapPointLight() : imageMap(NULL), func(NULL) {
}

MapPointLight::~MapPointLight() {
	delete func;
}

void MapPointLight::Preprocess() {
	PointLight::Preprocess();

	delete func;
	func = new SampleableSphericalFunction(new ImageMapSphericalFunction(imageMap));
}

void MapPointLight::GetPreprocessedData(float *localPosData, float *absolutePosData,
		float *emittedFactorData, const SampleableSphericalFunction **funcData) const {
	PointLight::GetPreprocessedData(localPosData, absolutePosData, emittedFactorData);

	if (funcData)
		*funcData = func;
}

float MapPointLight::GetPower(const Scene &scene) const {
	return imageMap->GetSpectrumMeanY() * PointLight::GetPower(scene);
}

Spectrum MapPointLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*orig = absolutePos;

	Vector localFromLight;
	func->Sample(u0, u1, &localFromLight, emissionPdfW);
	if (*emissionPdfW == 0.f)
		return Spectrum();

	*dir = Normalize(lightToWorld * localFromLight);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	return emittedFactor * ((SphericalFunction *)func)->Evaluate(localFromLight) / func->Average();
}

Spectrum MapPointLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector localFromLight = Normalize(Inverse(lightToWorld) * p - localPos);
	const float funcPdf = func->Pdf(localFromLight);
	if (funcPdf == 0.f)
		return Spectrum();

	const Vector toLight(absolutePos - p);
	const float distanceSquared = toLight.LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir = toLight / *distance;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	*directPdfW = distanceSquared;

	if (emissionPdfW)
		*emissionPdfW = funcPdf / (4.f * M_PI);

	return emittedFactor * ((SphericalFunction *)func)->Evaluate(localFromLight) / func->Average();
}

Properties MapPointLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = PointLight::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("mappoint"));
	props.Set(Property(prefix + ".mapfile")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imageMap)).str() + ".exr"));

	return props;
}

//------------------------------------------------------------------------------
// SpotLight
//------------------------------------------------------------------------------

SpotLight::SpotLight() :
	color(1.f), power(0.f), efficency(0.f),
	localPos(Point()), localTarget(Point(0.f, 0.f, 1.f)),
	coneAngle(30.f), coneDeltaAngle(5.f) {
}

SpotLight::~SpotLight() {
}

void SpotLight::Preprocess() {
	cosTotalWidth = cosf(Radians(coneAngle));
	cosFalloffStart = cosf(Radians(coneAngle - coneDeltaAngle));

	emittedFactor = gain * color * (power * efficency /
			(2.f * M_PI * color.Y() * (1.f - .5f * (cosFalloffStart + cosTotalWidth))));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	absolutePos = lightToWorld * localPos;

	const Vector dir = Normalize(localTarget - localPos);
	Vector du, dv;
	CoordinateSystem(dir, &du, &dv);
	const Transform dirToZ(Matrix4x4(
			du.x, du.y, du.z, 0.f,
			dv.x, dv.y, dv.z, 0.f,
			dir.x, dir.y, dir.z, 0.f,
			0.f, 0.f, 0.f, 1.f));
	alignedLight2World = lightToWorld *
			Translate(Vector(localPos.x, localPos.y, localPos.z)) /
			dirToZ;
}

void SpotLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData,
		float *cosTotalWidthData, float *cosFalloffStartData,
		const luxrays::Transform **alignedLight2WorldData) const {
	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}

	if (absolutePosData) {
		absolutePosData[0] = absolutePos.x;
		absolutePosData[1] = absolutePos.y;
		absolutePosData[2] = absolutePos.z;
	}

	if (cosTotalWidthData)
		*cosTotalWidthData = cosTotalWidth;
	if (cosFalloffStartData)
		*cosFalloffStartData = cosFalloffStart;

	if (alignedLight2WorldData)
		*alignedLight2WorldData = &alignedLight2World;
}

float SpotLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() * 2.f * M_PI * (1.f - .5f * (cosFalloffStart + cosTotalWidth));
}

static float LocalFalloff(const Vector &w, const float cosTotalWidth, const float cosFalloffStart) {
	if (CosTheta(w) < cosTotalWidth)
		return 0.f;
 	if (CosTheta(w) > cosFalloffStart)
		return 1.f;

	// Compute falloff inside spotlight cone
	const float delta = (CosTheta(w) - cosTotalWidth) / (cosFalloffStart - cosTotalWidth);
	return powf(delta, 4);
}

Spectrum SpotLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*orig = absolutePos;
	const Vector localFromLight = UniformSampleCone(u0, u1, cosTotalWidth);
	*dir = Normalize(alignedLight2World * localFromLight);
	*emissionPdfW = UniformConePdf(cosTotalWidth);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = CosTheta(localFromLight);

	return emittedFactor * (LocalFalloff(localFromLight, cosTotalWidth, cosFalloffStart) / fabsf(CosTheta(localFromLight)));
}

Spectrum SpotLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector toLight(absolutePos - p);
	const float distanceSquared = toLight.LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir = toLight / *distance;

	const Vector localFromLight = Normalize(Inverse(alignedLight2World) * (-(*dir)));
	const float falloff = LocalFalloff(localFromLight, cosTotalWidth, cosFalloffStart);
	if (falloff == 0.f)
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = CosTheta(localFromLight);

	*directPdfW = distanceSquared;

	if (emissionPdfW)
		*emissionPdfW = UniformConePdf(cosTotalWidth);

	return emittedFactor * (falloff / fabsf(CosTheta(localFromLight)));
}

Properties SpotLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("spot"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".coneangle")(coneAngle));
	props.Set(Property(prefix + ".conedeltaangle")(coneDeltaAngle));

	return props;
}

//------------------------------------------------------------------------------
// ProjectionLight
//------------------------------------------------------------------------------

ProjectionLight::ProjectionLight() : color(1.f), power(0.f), efficency(0.f),
		localPos(Point()), localTarget(Point(0.f, 0.f, 1.f)),
		fov(45.f), imageMap(NULL) {
}

ProjectionLight::~ProjectionLight() {
}

void ProjectionLight::Preprocess() {
	const Vector dir = Normalize(localTarget - localPos);
	Vector du, dv;
	CoordinateSystem(dir, &du, &dv);
	const Transform dirToZ(Matrix4x4(
			du.x, du.y, du.z, 0.f,
			dv.x, dv.y, dv.z, 0.f,
			dir.x, dir.y, dir.z, 0.f,
			0.f, 0.f, 0.f, 1.f));
	alignedLight2World = lightToWorld *
			Translate(Vector(localPos.x, localPos.y, localPos.z)) /
			dirToZ;

	absolutePos = alignedLight2World * Point();
	lightNormal = Normalize(alignedLight2World * Normal(0.f, 0.f, 1.f));

	emittedFactor = gain * color * (power * efficency /
			(2.f * M_PI * (imageMap ? imageMap->GetSpectrumMeanY() : 1.f) *
				(1.f - .5f * (1.f - cosTotalWidth))));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	const float aspect = imageMap ? (imageMap->GetWidth() / (float)imageMap->GetHeight()) : 1.f;
	if (aspect > 1.f)  {
		screenX0 = -aspect;
		screenX1 = aspect;
		screenY0 = -1.f;
		screenY1 = 1.f;
	} else {
		screenX0 = -1.f;
		screenX1 = 1.f;
		screenY0 = -1.f / aspect;
		screenY1 = 1.f / aspect;
	}

	const float hither = DEFAULT_EPSILON_STATIC;
	const float yon = 1e30f;
	lightProjection = Perspective(fov, hither, yon);

	// Compute cosine of cone surrounding projection directions
	const float opposite = tanf(Radians(fov) / 2.f);
	const float tanDiag = opposite * sqrtf(1.f + 1.f / (aspect * aspect));
	cosTotalWidth = cosf(atanf(tanDiag));
	if (aspect <= 1.f)
		area = 4.f * opposite * opposite / aspect;
	else
		area = 4.f * opposite * opposite * aspect;
}

void ProjectionLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData, float *lightNormalData,
		float *screenX0Data, float *screenX1Data, float *screenY0Data, float *screenY1Data,
		const luxrays::Transform **alignedLight2WorldData, const luxrays::Transform **lightProjectionData) const {
	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}

	if (absolutePosData) {
		absolutePosData[0] = absolutePos.x;
		absolutePosData[1] = absolutePos.y;
		absolutePosData[2] = absolutePos.z;
	}

	if (lightNormalData) {
		lightNormalData[0] = lightNormal.x;
		lightNormalData[1] = lightNormal.y;
		lightNormalData[2] = lightNormal.z;
	}

	if (screenX0Data)
		*screenX0Data = screenX0;
	if (screenX1Data)
		*screenX1Data = screenX1;
	if (screenY0Data)
		*screenY0Data = screenY0;
	if (screenY1Data)
		*screenY1Data = screenY1;

	if (alignedLight2WorldData)
		*alignedLight2WorldData = &alignedLight2World;
	if (lightProjectionData)
		*lightProjectionData = &lightProjection;
}

float ProjectionLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() *
			(imageMap ? imageMap->GetSpectrumMeanY() : 1.f) *
			2.f * M_PI * (1.f - cosTotalWidth);
}

Spectrum ProjectionLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*orig = absolutePos;
	const Point ps = Inverse(lightProjection) *
		Point(u0 * (screenX1 - screenX0) + screenX0, u1 * (screenY1 - screenY0) + screenY0, 0.f);
	*dir = Normalize(alignedLight2World * Vector(ps.x, ps.y, ps.z));
	const float cos = Dot(*dir, lightNormal);
	const float cos2 = cos * cos;
	*emissionPdfW = 1.f / (area * cos2 * cos);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	Spectrum c = emittedFactor;
	if (imageMap)
		c *= imageMap->GetSpectrum(UV(u0, u1));

	return c;
}

Spectrum ProjectionLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const Vector toLight(absolutePos - p);
	const float distanceSquared = toLight.LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir = toLight / *distance;

	// Check the side
	if (Dot(-(*dir), lightNormal) < 0.f)
		return Spectrum();

	// Check if the point is inside the image plane
	const Vector localFromLight = Normalize(Inverse(alignedLight2World) * (-(*dir)));
	const Point p0 = lightProjection * Point(localFromLight.x, localFromLight.y, localFromLight.z);
	if ((p0.x < screenX0) || (p0.x >= screenX1) || (p0.y < screenY0) || (p0.y >= screenY1))
		return Spectrum();

	*directPdfW = distanceSquared;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 0.f;

	Spectrum c = emittedFactor;
	if (imageMap) {
		const float u = (p0.x - screenX0) / (screenX1 - screenX0);
		const float v = (p0.y - screenY0) / (screenY1 - screenY0);
		c *= imageMap->GetSpectrum(UV(u, v));
	}

	return c;
}

Properties ProjectionLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("projection"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".fov")(fov));
	props.Set(Property(prefix + ".mapfile")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imageMap)).str() + ".exr"));

	return props;
}

//------------------------------------------------------------------------------
// LaserLight
//------------------------------------------------------------------------------

LaserLight::LaserLight() :
	color(1.f), power(0.f), efficency(0.f),
	localPos(Point()), localTarget(Point(0.f, 0.f, 1.f)),
	radius(.01f) {
}

LaserLight::~LaserLight() {
}

void LaserLight::Preprocess() {
	emittedFactor = gain * color * (power * efficency / (M_PI * radius * radius * color.Y()));
	if (emittedFactor.Black() || emittedFactor.IsInf() || emittedFactor.IsNaN())
		emittedFactor = gain * color;

	absoluteLightPos = lightToWorld * localPos;
	absoluteLightDir = Normalize(lightToWorld * (localTarget - localPos));
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void LaserLight::GetPreprocessedData(float *emittedFactorData, float *absolutePosData,
		 float *absoluteDirData, float *xData, float *yData) const {
	if (emittedFactorData) {
		emittedFactorData[0] = emittedFactor.c[0];
		emittedFactorData[1] = emittedFactor.c[1];
		emittedFactorData[2] = emittedFactor.c[2];
	}

	if (absolutePosData) {
		absolutePosData[0] = absoluteLightPos.x;
		absolutePosData[1] = absoluteLightPos.y;
		absolutePosData[2] = absoluteLightPos.z;
	}

	if (absoluteDirData) {
		absoluteDirData[0] = absoluteLightDir.x;
		absoluteDirData[1] = absoluteLightDir.y;
		absoluteDirData[2] = absoluteLightDir.z;
	}

	if (xData) {
		xData[0] = x.x;
		xData[1] = x.y;
		xData[2] = x.z;
	}

	if (yData) {
		yData[0] = y.x;
		yData[1] = y.y;
		yData[2] = y.z;
	}
}

float LaserLight::GetPower(const Scene &scene) const {
	return emittedFactor.Y() * M_PI * radius * radius;
}

Spectrum LaserLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = absoluteLightPos - radius * (d1 * x + d2 * y);
	*dir = absoluteLightDir;
	
	*emissionPdfW = 1.f / (M_PI * radius * radius);

	if (directPdfA)
		*directPdfA = 1.f;
	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	return emittedFactor;
}

Spectrum LaserLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = -absoluteLightDir;
	
	const Point &rayOrig = p;
	const Vector &rayDir = *dir;
	const Point &planeCenter = absoluteLightPos;
	const Vector &planeNormal = absoluteLightDir;

	// Intersect the shadow ray with light plane
	const float denom = Dot(planeNormal, rayDir);
	const Vector pr = planeCenter - rayOrig;
	float d = Dot(pr, planeNormal);

	if (fabsf(denom) > DEFAULT_COS_EPSILON_STATIC) {
		// There is a valid intersection
		d /= denom; 

		if ((d <= 0.f) || (denom >= 0.f))
			return Spectrum();
	} else
		return Spectrum();

	const Point lightPoint = rayOrig + d * rayDir;

	// Check if the point is inside the emitting circle
	const float radius2 = radius * radius;
	if ((lightPoint - absoluteLightPos).LengthSquared() > radius2)
		return Spectrum();
	
	// Ok, the light is visible
	
	*distance = d;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	*directPdfW = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 0.f;

	return emittedFactor;
}

Properties LaserLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("laser"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".power")(power));
	props.Set(Property(prefix + ".efficency")(efficency));
	props.Set(Property(prefix + ".position")(localPos));
	props.Set(Property(prefix + ".target")(localTarget));
	props.Set(Property(prefix + ".radius")(radius));

	return props;
}

//------------------------------------------------------------------------------
// SharpDistantLight
//------------------------------------------------------------------------------

SharpDistantLight::SharpDistantLight() :
		color(1.f), localLightDir(0.f, 0.f, 1.f) {
}

SharpDistantLight::~SharpDistantLight() {
}

void SharpDistantLight::Preprocess() {
	absoluteLightDir = Normalize(lightToWorld * localLightDir);
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void SharpDistantLight::GetPreprocessedData(float *absoluteLightDirData,
		float *xData, float *yData) const {
	if (absoluteLightDirData) {
		absoluteLightDirData[0] = absoluteLightDir.x;
		absoluteLightDirData[1] = absoluteLightDir.y;
		absoluteLightDirData[2] = absoluteLightDir.z;
	}

	if (xData) {
		xData[0] = x.x;
		xData[1] = x.y;
		xData[2] = x.z;
	}

	if (yData) {
		yData[0] = y.x;
		yData[1] = y.y;
		yData[2] = y.z;
	}
}

float SharpDistantLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * color.Y() * M_PI * worldRadius * worldRadius;
}

Spectrum SharpDistantLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*dir = absoluteLightDir;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	float d1, d2;
	ConcentricSampleDisk(u0, u1, &d1, &d2);
	*orig = worldCenter - worldRadius * (absoluteLightDir + d1 * x + d2 * y);

	*emissionPdfW = 1.f / (M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = 1.f;

	return gain * color;
}

Spectrum SharpDistantLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = -absoluteLightDir;

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	*directPdfW = 1.f;

	if (cosThetaAtLight)
		*cosThetaAtLight = 1.f;

	if (emissionPdfW)
		*emissionPdfW = 1.f / (M_PI * worldRadius * worldRadius);

	return gain * color;
}

Properties SharpDistantLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sharpdistant"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".direction")(localLightDir));

	return props;
}

//------------------------------------------------------------------------------
// DistantLight
//------------------------------------------------------------------------------

DistantLight::DistantLight() :
		color(1.f), localLightDir(0.f, 0.f, 1.f) {
}

DistantLight::~DistantLight() {
}

void DistantLight::Preprocess() {
	if (theta == 0.f) {
		sin2ThetaMax = 2.f * MachineEpsilon::E(1.f);
		cosThetaMax = 1.f - MachineEpsilon::E(1.f);
	} else {
		const float radTheta = Radians(theta);
		sin2ThetaMax = sinf( Radians(radTheta)) * sinf(radTheta);
		cosThetaMax = cosf(radTheta);
	}

	absoluteLightDir = Normalize(lightToWorld * localLightDir);
	CoordinateSystem(absoluteLightDir, &x, &y);
}

void DistantLight::GetPreprocessedData(float *absoluteLightDirData, float *xData, float *yData,
		float *sin2ThetaMaxData, float *cosThetaMaxData) const {
	if (absoluteLightDirData) {
		absoluteLightDirData[0] = absoluteLightDir.x;
		absoluteLightDirData[1] = absoluteLightDir.y;
		absoluteLightDirData[2] = absoluteLightDir.z;
	}
	
	if (xData) {
		xData[0] = x.x;
		xData[1] = x.y;
		xData[2] = x.z;
	}

	if (yData) {
		yData[0] = y.x;
		yData[1] = y.y;
		yData[2] = y.z;
	}

	if (sin2ThetaMaxData)
		*sin2ThetaMaxData = sin2ThetaMax;
	if (cosThetaMaxData)
		*cosThetaMaxData = cosThetaMax;
}

float DistantLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * color.Y() * M_PI * worldRadius * worldRadius;
}

Spectrum DistantLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	*dir = UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);
	const float uniformConePdf = UniformConePdf(cosThetaMax);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(*dir, absoluteLightDir);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	float d1, d2;
	ConcentricSampleDisk(u2, u3, &d1, &d2);
	*orig = worldCenter - worldRadius * (absoluteLightDir + d1 * x + d2 * y);

	*emissionPdfW = uniformConePdf / (M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = uniformConePdf;

	return gain * color;
}

Spectrum DistantLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = -UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteLightDir);

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	*directPdfW = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(-absoluteLightDir, *dir);

	if (emissionPdfW)
		*emissionPdfW =  uniformConePdf / (M_PI * worldRadius * worldRadius);

	return gain * color;
}

Properties DistantLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = NotIntersectableLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("distant"));
	props.Set(Property(prefix + ".color")(color));
	props.Set(Property(prefix + ".direction")(localLightDir));
	props.Set(Property(prefix + ".theta")(10.f));

	return props;
}

//------------------------------------------------------------------------------
// ConstantInfiniteLight
//------------------------------------------------------------------------------

ConstantInfiniteLight::ConstantInfiniteLight() : color(1.f) {
}

ConstantInfiniteLight::~ConstantInfiniteLight() {
}

float ConstantInfiniteLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * (4.f * M_PI * M_PI * worldRadius * worldRadius) *
		color.Y();
}

Spectrum ConstantInfiniteLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * color;
}

Spectrum ConstantInfiniteLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	// Choose p1 on scene bounding sphere
	const float phi = u0 * 2.f * M_PI;
	const float theta = u1 * M_PI;
	Point p1 = worldCenter + worldRadius * SphericalDirection(sinf(theta), cosf(theta), phi);

	// Choose p2 on scene bounding sphere
	Point p2 = worldCenter + worldRadius * UniformSampleSphere(u2, u3);

	// Construct ray between p1 and p2
	*orig = p1;
	*dir = Normalize((p2 - p1));

	// Compute InfiniteLight ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(Normalize(worldCenter -  p1), *dir);

	return GetRadiance(scene, *dir);
}

Spectrum ConstantInfiniteLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	const float phi = u0 * 2.f * M_PI;
	const float theta = u1 * M_PI;
	*dir = Normalize(SphericalDirection(sinf(theta), cosf(theta), phi));

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

	*directPdfW = 1.f / (4.f * M_PI);

	if (emissionPdfW)
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

	return gain * color;
}

Properties ConstantInfiniteLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("constantinfinite"));
	props.Set(Property(prefix + ".color")(color));

	return props;
}

//------------------------------------------------------------------------------
// InfiniteLight
//------------------------------------------------------------------------------

InfiniteLight::InfiniteLight() :
	imageMap(NULL), mapping(1.f, 1.f, 0.f, 0.f) {
}

InfiniteLight::~InfiniteLight() {
	delete imageMapDistribution;
}

void InfiniteLight::Preprocess() {
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

void InfiniteLight::GetPreprocessedData(const Distribution2D **imageMapDistributionData) const {
	if (imageMapDistributionData)
		*imageMapDistributionData = imageMapDistribution;
	
}

float InfiniteLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return gain.Y() * imageMap->GetSpectrumMeanY() *
			(4.f * M_PI * M_PI * worldRadius * worldRadius);
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

	// Choose p1 on scene bounding sphere according importance sampling
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

	// Compute InfiniteLight ray weight
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
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("infinite"));
	props.Set(Property(prefix + ".file")("imagemap-" + 
		(boost::format("%05d") % imgMapCache.GetImageMapIndex(imageMap)).str() + ".exr"));
	props.Set(Property(prefix + ".shift")(mapping.uDelta, mapping.vDelta));

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
	s->c[0] =  3.2410f * X - 1.5374f * Y - 0.4986f * Z;
	s->c[1] = -0.9692f * X + 1.8760f * Y + 0.0416f * Z;
	s->c[2] =  0.0556f * X - 0.2040f * Y + 1.0570f * Z;
}

SkyLight::SkyLight() {
}

SkyLight::~SkyLight() {
}

void SkyLight::Preprocess() {
	absoluteSunDir = Normalize(lightToWorld * localSunDir);

	absoluteTheta = SphericalTheta(absoluteSunDir);
	absolutePhi = SphericalPhi(absoluteSunDir);

	float aconst = 1.f;
	float bconst = 1.f;
	float cconst = 1.f;
	float dconst = 1.f;
	float econst = 1.f;

	float theta2 = absoluteTheta * absoluteTheta;
	float theta3 = theta2 * absoluteTheta;
	float T = turbidity;
	float T2 = T * T;

	float chi = (4.f / 9.f - T / 120.f) * (M_PI - 2.0f * absoluteTheta);
	zenith_Y = (4.0453f * T - 4.9710f) * tanf(chi) - 0.2155f * T + 2.4192f;
	zenith_Y *= 1000;  // conversion from kcd/m^2 to cd/m^2

	zenith_x =
	(0.00166f * theta3 - 0.00375f * theta2 + 0.00209f * absoluteTheta) * T2 +
	(-0.02903f * theta3 + 0.06377f * theta2 - 0.03202f * absoluteTheta + 0.00394f) * T +
	(0.11693f * theta3 - 0.21196f * theta2 + 0.06052f * absoluteTheta + 0.25886f);

	zenith_y =
	(0.00275f * theta3 - 0.00610f * theta2 + 0.00317f * absoluteTheta) * T2 +
	(-0.04214f * theta3 + 0.08970f * theta2 - 0.04153f * absoluteTheta  + 0.00516f) * T +
	(0.15346f * theta3 - 0.26756f * theta2 + 0.06670f * absoluteTheta  + 0.26688f);

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

	zenith_Y /= PerezBase(perez_Y, 0, absoluteTheta);
	zenith_x /= PerezBase(perez_x, 0, absoluteTheta);
	zenith_y /= PerezBase(perez_y, 0, absoluteTheta);
}

void SkyLight::GetPreprocessedData(float *absoluteSunDirData,
	float *absoluteThetaData, float *absolutePhiData,
	float *zenith_YData, float *zenith_xData, float *zenith_yData,
	float *perez_YData, float *perez_xData, float *perez_yData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
	}

	if (absoluteThetaData)
		*absoluteThetaData = absoluteTheta;

	if (absolutePhiData)
		*absolutePhiData = absolutePhi;

	if (zenith_YData)
		*zenith_YData = zenith_Y;

	if (zenith_xData)
		*zenith_xData = zenith_x;

	if (zenith_yData)
		*zenith_yData = zenith_y;

	for (size_t i = 0; i < 6; ++i) {
		if (perez_YData)
			perez_YData[i] = perez_Y[i];
		if (perez_xData)
			perez_xData[i] = perez_x[i];
		if (perez_yData)
			perez_yData[i] = perez_y[i];
	}
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
			float gamma = RiAngleBetween(theta, phi, absoluteTheta, absolutePhi);
			theta = Min<float>(theta, M_PI * .5f - .001f);
			power += zenith_Y * PerezBase(perez_Y, theta, gamma);
			cosTheta += deltaStep;
		}

		phi += deltaStep * M_PI;
	}
	power /= steps * steps;

	return gain.Y() * power * (4.f * M_PI * worldRadius * worldRadius) * 2.f * M_PI;
}

void SkyLight::GetSkySpectralRadiance(const float theta, const float phi, Spectrum * const spect) const {
	// Add bottom half of hemisphere with horizon colour
	const float theta_fin = Min<float>(theta, (M_PI * .5f) - .001f);
	const float gamma = RiAngleBetween(theta, phi, absoluteTheta, absolutePhi);

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

	// Compute SkyLight ray weight
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
	const float theta = SphericalTheta(-dir);
	const float phi = SphericalPhi(-dir);
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
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sky"));
	props.Set(Property(prefix + ".dir")(localSunDir));
	props.Set(Property(prefix + ".turbidity")(turbidity));

	return props;
}

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

SkyLight2::SkyLight2() {
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

	const Spectrum albedo(0.f);

	ComputeModel(turbidity, albedo, M_PI * .5f - SphericalTheta(absoluteSunDir), model);

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

void SkyLight2::GetPreprocessedData(float *absoluteSunDirData,
		float *aTermData, float *bTermData, float *cTermData, float *dTermData,
		float *eTermData, float *fTermData, float *gTermData, float *hTermData,
		float *iTermData, float *radianceTermData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
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
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
	
	const u_int steps = 100;
	const float deltaStep = 1.f / steps;
	float power = 0.f;
	for (u_int i = 0; i < steps; ++i) {
		for (u_int j = 0; j < steps; ++j)
			power += ComputeRadiance(UniformSampleSphere(i * deltaStep + deltaStep / 2.f,
					j * deltaStep + deltaStep / 2.f)).Y();
	}
	power /= steps * steps;

	return gain.Y() * power * (4.f * M_PI * worldRadius * worldRadius) * 2.f * M_PI;
}

Spectrum SkyLight2::Emit(const Scene &scene,
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

	// Compute SkyLight2 ray weight
	*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);

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

Spectrum SkyLight2::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	if (directPdfA)
		*directPdfA = 1.f / (4.f * M_PI);

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;
		*emissionPdfW = 1.f / (4.f * M_PI * M_PI * worldRadius * worldRadius);
	}

	return gain * ComputeRadiance(-dir);
}

Properties SkyLight2::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sky2"));
	props.Set(Property(prefix + ".dir")(localSunDir));
	props.Set(Property(prefix + ".turbidity")(turbidity));

	return props;
}

//------------------------------------------------------------------------------
// SunLight
//------------------------------------------------------------------------------

SunLight::SunLight() {
}

void SunLight::Preprocess() {
	absoluteSunDir = Normalize(lightToWorld * localSunDir);
	CoordinateSystem(absoluteSunDir, &x, &y);

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

	Vector wh = Normalize(absoluteSunDir);
	absoluteTheta = SphericalTheta(wh);
	absolutePhi = SphericalPhi(wh);

	// NOTE - lordcrc - sun_k_oWavelengths contains 64 elements, while sun_k_oAmplitudes contains 65?!?
	IrregularSPD k_oCurve(sun_k_oWavelengths, sun_k_oAmplitudes, 64);
	IrregularSPD k_gCurve(sun_k_gWavelengths, sun_k_gAmplitudes, 4);
	IrregularSPD k_waCurve(sun_k_waWavelengths, sun_k_waAmplitudes, 13);

	RegularSPD solCurve(sun_solAmplitudes, 380, 750, 38);  // every 5 nm

	float beta = 0.04608365822050f * turbidity - 0.04586025928522f;
	float tauR, tauA, tauO, tauG, tauWA;

	float m = 1.f / (cosf(absoluteTheta) + 0.00094f * powf(1.6386f - absoluteTheta, 
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
		tauO = expf(-m * k_oCurve.Sample(lambda) * lOzone);
			// Attenuation due to mixed gases absorption
		tauG = expf(-1.41f * k_gCurve.Sample(lambda) * m / powf(1.f +
			118.93f * k_gCurve.Sample(lambda) * m, 0.45f));
			// Attenuation due to water vapor absorption
			// w - precipitable water vapor in centimeters (standard = 2)
		const float w = 2.0f;
		tauWA = expf(-0.2385f * k_waCurve.Sample(lambda) * w * m /
		powf(1.f + 20.07f * k_waCurve.Sample(lambda) * w * m, 0.45f));

		Ldata[i] = (solCurve.Sample(lambda) * tauR * tauA * tauO * tauG * tauWA);
	}

	RegularSPD LSPD(Ldata, 350, 800, 91);
	color = (gain / (relSize * relSize)) *
			ColorSystem::DefaultColorSystem.ToRGBConstrained(LSPD.ToXYZ()).Clamp();
}

void SunLight::GetPreprocessedData(float *absoluteSunDirData,
		float *xData, float *yData,
		float *absoluteThetaData, float *absolutePhiData,
		float *VData, float *cosThetaMaxData, float *sin2ThetaMaxData) const {
	if (absoluteSunDirData) {
		absoluteSunDirData[0] = absoluteSunDir.x;
		absoluteSunDirData[1] = absoluteSunDir.y;
		absoluteSunDirData[2] = absoluteSunDir.z;
	}

	if (xData) {
		xData[0] = x.x;
		xData[1] = x.y;
		xData[2] = x.z;
	}

	if (yData) {
		yData[0] = y.x;
		yData[1] = y.y;
		yData[2] = y.z;
	}

	if (absoluteThetaData)
		*absoluteThetaData = absoluteTheta;

	if (absolutePhiData)
		*absolutePhiData = absolutePhi;

	if (VData)
		*VData = V;

	if (cosThetaMaxData)
		*cosThetaMaxData = cosThetaMax;

	if (sin2ThetaMaxData)
		*sin2ThetaMaxData = sin2ThetaMax;
}

float SunLight::GetPower(const Scene &scene) const {
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	return color.Y() * (M_PI * worldRadius * worldRadius) * 2.f * M_PI * sin2ThetaMax;
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
	*orig = worldCenter + worldRadius * (absoluteSunDir + d1 * x + d2 * y);
	*dir = -UniformSampleCone(u2, u3, cosThetaMax, x, y, absoluteSunDir);

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	*emissionPdfW = uniformConePdf / (M_PI * worldRadius * worldRadius);

	if (directPdfA)
		*directPdfA = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = Dot(absoluteSunDir, -(*dir));

	return color;
}

Spectrum SunLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	*dir = UniformSampleCone(u0, u1, cosThetaMax, x, y, absoluteSunDir);

	// Check if the point can be inside the sun cone of light
	const float cosAtLight = Dot(absoluteSunDir, *dir);
	if (cosAtLight <= cosThetaMax)
		return Spectrum();

	const Point worldCenter = scene.dataSet->GetBSphere().center;
	const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad * 1.01f;

	const Vector toCenter(worldCenter - p);
	const float centerDistance = Dot(toCenter, toCenter);
	const float approach = Dot(toCenter, *dir);
	*distance = approach + sqrtf(Max(0.f, worldRadius * worldRadius -
		centerDistance + approach * approach));
	
	const float uniformConePdf = UniformConePdf(cosThetaMax);
	*directPdfW = uniformConePdf;

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	if (emissionPdfW)
		*emissionPdfW =  uniformConePdf / (M_PI * worldRadius * worldRadius);
	
	return color;
}

Spectrum SunLight::GetRadiance(const Scene &scene,
		const Vector &dir,
		float *directPdfA,
		float *emissionPdfW) const {
	const float xD = Dot(-dir, x);
	const float yD = Dot(-dir, y);
	const float zD = Dot(-dir, absoluteSunDir);
	if ((cosThetaMax == 1.f) || (zD < 0.f) || ((xD * xD + yD * yD) > sin2ThetaMax))
		return Spectrum();

	const float uniformConePdf = UniformConePdf(cosThetaMax);
	if (directPdfA)
		*directPdfA = uniformConePdf;

	if (emissionPdfW) {
		const float worldRadius = LIGHT_WORLD_RADIUS_SCALE * scene.dataSet->GetBSphere().rad;
		*emissionPdfW = uniformConePdf / (M_PI * worldRadius * worldRadius);
	}

	return color;
}

Properties SunLight::ToProperties(const ImageMapCache &imgMapCache) const {
	const string prefix = "scene.lights." + GetName();
	Properties props = EnvLightSource::ToProperties(imgMapCache);

	props.Set(Property(prefix + ".type")("sun"));
	props.Set(Property(prefix + ".dir")(localSunDir));
	props.Set(Property(prefix + ".turbidity")(turbidity));
	props.Set(Property(prefix + ".relsize")(relSize));

	return props;
}

//------------------------------------------------------------------------------
// Triangle Area Light
//------------------------------------------------------------------------------

TriangleLight::TriangleLight() : mesh(NULL), meshIndex(NULL_INDEX),
		triangleIndex(NULL_INDEX), triangleArea(0.f), invTriangleArea(0.f),
		meshArea(0.f), invMeshArea(0.f){
}

TriangleLight::~TriangleLight() {
	
}

float TriangleLight::GetPower(const Scene &scene) const {
	return triangleArea * M_PI * lightMaterial->GetEmittedRadianceY();
}

void TriangleLight::Preprocess() {
	triangleArea = mesh->GetTriangleArea(0.f, triangleIndex);
	invTriangleArea = 1.f / triangleArea;

	meshArea = mesh->GetMeshArea(0.f);
	invMeshArea = 1.f / meshArea;
}

Spectrum TriangleLight::Emit(const Scene &scene,
		const float u0, const float u1, const float u2, const float u3, const float passThroughEvent,
		Point *orig, Vector *dir,
		float *emissionPdfW, float *directPdfA, float *cosThetaAtLight) const {
	// Origin
	float b0, b1, b2;
	mesh->Sample(0.f, triangleIndex, u0, u1, orig, &b0, &b1, &b2);

	// Build the local frame
	const Normal N = mesh->GetGeometryNormal(0.f, triangleIndex); // Light sources are supposed to be flat
	Frame frame(N);

	Spectrum emissionColor(1.f);
	Vector localDirOut;
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();
	if (emissionFunc) {
		emissionFunc->Sample(u2, u3, &localDirOut, emissionPdfW);
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localDirOut) / emissionFunc->Average();
	} else
		localDirOut = CosineSampleHemisphere(u2, u3, emissionPdfW);

	if (*emissionPdfW == 0.f)
			return Spectrum();
	*emissionPdfW *= invTriangleArea;

	// Cannot really not emit the particle, so just bias it to the correct angle
	localDirOut.z = Max(localDirOut.z, DEFAULT_COS_EPSILON_STATIC);

	// Direction
	*dir = frame.ToWorld(localDirOut);

	if (directPdfA)
		*directPdfA = invTriangleArea;

	if (cosThetaAtLight)
		*cosThetaAtLight = localDirOut.z;

	const UV triUV = mesh->InterpolateTriUV(triangleIndex, b1, b2);
	const Spectrum color = mesh->InterpolateTriColor(triangleIndex, b1, b2);
	const float alpha = mesh->InterpolateTriAlpha(triangleIndex, b1, b2);
	const HitPoint hitPoint = { Vector(-N), *orig, triUV, N, N,
		color, alpha, passThroughEvent, NULL, NULL, false, false };

	return lightMaterial->GetEmittedRadiance(hitPoint, invMeshArea) * localDirOut.z;
}

Spectrum TriangleLight::Illuminate(const Scene &scene, const Point &p,
		const float u0, const float u1, const float passThroughEvent,
        Vector *dir, float *distance, float *directPdfW,
		float *emissionPdfW, float *cosThetaAtLight) const {
	Point samplePoint;
	float b0, b1, b2;
	mesh->Sample(0.f, triangleIndex, u0, u1, &samplePoint, &b0, &b1, &b2);
	const Normal &sampleN = mesh->GetGeometryNormal(0.f, triangleIndex); // Light sources are supposed to be flat

	*dir = samplePoint - p;
	const float distanceSquared = dir->LengthSquared();
	*distance = sqrtf(distanceSquared);
	*dir /= (*distance);

	const float cosAtLight = Dot(sampleN, -(*dir));
	if (cosAtLight < DEFAULT_COS_EPSILON_STATIC)
		return Spectrum();

	if (cosThetaAtLight)
		*cosThetaAtLight = cosAtLight;

	Spectrum emissionColor(1.f);
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();
	if (emissionFunc) {
		// Build the local frame
		const Normal N = mesh->GetGeometryNormal(0.f, triangleIndex); // Light sources are supposed to be flat
		Frame frame(N);

		const Vector localFromLight = Normalize(frame.ToLocal(-(*dir)));
		
		if (emissionPdfW) {
			const float emissionFuncPdf = emissionFunc->Pdf(localFromLight);
			if (emissionFuncPdf == 0.f)
				return Spectrum();
			*emissionPdfW = emissionFuncPdf * invTriangleArea;
		}
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localFromLight) / emissionFunc->Average();
		
		*directPdfW = invTriangleArea * distanceSquared;
	} else {
		if (emissionPdfW)
			*emissionPdfW = invTriangleArea * cosAtLight * INV_PI;

		*directPdfW = invTriangleArea * distanceSquared / cosAtLight;
	}

	const UV triUV = mesh->InterpolateTriUV(triangleIndex, b1, b2);
	const Spectrum color = mesh->InterpolateTriColor(triangleIndex, b1, b2);
	const float alpha = mesh->InterpolateTriAlpha(triangleIndex, b1, b2);
	const HitPoint hitPoint = { Vector(-sampleN), samplePoint, triUV, sampleN, sampleN,
		color, alpha, passThroughEvent, NULL, NULL, false, false };

	return lightMaterial->GetEmittedRadiance(hitPoint, invMeshArea) * emissionColor;
}

Spectrum TriangleLight::GetRadiance(const HitPoint &hitPoint,
		float *directPdfA,
		float *emissionPdfW) const {
	const float cosOutLight = Dot(hitPoint.geometryN, hitPoint.fixedDir);
	if (cosOutLight <= 0.f)
		return Spectrum();

	if (directPdfA)
		*directPdfA = invTriangleArea;

	Spectrum emissionColor(1.f);
	const SampleableSphericalFunction *emissionFunc = lightMaterial->GetEmissionFunc();
	if (emissionFunc) {
		// Build the local frame
		const Normal N = mesh->GetGeometryNormal(0.f, triangleIndex); // Light sources are supposed to be flat
		Frame frame(N);

		const Vector localFromLight = Normalize(frame.ToLocal(hitPoint.fixedDir));
		
		if (emissionPdfW) {
			const float emissionFuncPdf = emissionFunc->Pdf(localFromLight);
			if (emissionFuncPdf == 0.f)
				return Spectrum();
			*emissionPdfW = emissionFuncPdf * invTriangleArea;
		}
		emissionColor = ((SphericalFunction *)emissionFunc)->Evaluate(localFromLight) / emissionFunc->Average();
	} else {
		if (emissionPdfW)
			*emissionPdfW = invTriangleArea * cosOutLight * INV_PI;
	}

	return lightMaterial->GetEmittedRadiance(hitPoint, invMeshArea) * emissionColor;
}
