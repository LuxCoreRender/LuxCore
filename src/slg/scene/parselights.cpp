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

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "slg/scene/scene.h"

#include "slg/lights/constantinfinitelight.h"
#include "slg/lights/distantlight.h"
#include "slg/lights/infinitelight.h"
#include "slg/lights/laserlight.h"
#include "slg/lights/mappointlight.h"
#include "slg/lights/pointlight.h"
#include "slg/lights/projectionlight.h"
#include "slg/lights/sharpdistantlight.h"
#include "slg/lights/sky2light.h"
#include "slg/lights/skylight.h"
#include "slg/lights/spotlight.h"
#include "slg/lights/sunlight.h"
#include "slg/lights/trianglelight.h"

using namespace std;
using namespace luxrays;
using namespace slg;

void Scene::ParseLights(const Properties &props) {
	// The following code is used only for compatibility with the past syntax
	if (props.HaveNames("scene.skylight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.skylight", props);
		lightDefs.DefineLightSource("skylight", newLight);
		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
	if (props.HaveNames("scene.infinitelight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.infinitelight", props);
		lightDefs.DefineLightSource("infinitelight", newLight);
		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
	if (props.HaveNames("scene.sunlight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.sunlight", props);
		lightDefs.DefineLightSource("sunlight", newLight);
		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}

	vector<string> lightKeys = props.GetAllUniqueSubNames("scene.lights");
	if (lightKeys.size() == 0) {
		// There are not light definitions
		return;
	}

	BOOST_FOREACH(const string &key, lightKeys) {
		// Extract the light name
		const string lightName = Property::ExtractField(key, 2);
		if (lightName == "")
			throw runtime_error("Syntax error in light definition: " + lightName);

		SDL_LOG("Light definition: " << lightName);

		LightSource *newLight = CreateLightSource(lightName, props);
		lightDefs.DefineLightSource(lightName, newLight);
		
		if ((newLight->GetType() == TYPE_IL) ||
				(newLight->GetType() == TYPE_MAPPOINT) ||
				(newLight->GetType() == TYPE_PROJECTION))
			editActions.AddActions(IMAGEMAPS_EDIT);
	}

	editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
}


ImageMap *Scene::CreateEmissionMap(const string &propName, const luxrays::Properties &props) {
	const string imgMapName = props.Get(Property(propName + ".mapfile")("")).Get<string>();
	const string iesName = props.Get(Property(propName + ".iesfile")("")).Get<string>();
	const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
	const u_int width = props.Get(Property(propName + ".map.width")(0)).Get<u_int>();
	const u_int height = props.Get(Property(propName + ".map.height")(0)).Get<u_int>();

	ImageMap *map = NULL;
	if ((imgMapName != "") && (iesName != "")) {
		ImageMap *imgMap = imgMapCache.GetImageMap(imgMapName, gamma,
				ImageMapStorage::DEFAULT, ImageMapStorage::FLOAT);

		ImageMap *iesMap;
		PhotometricDataIES data(iesName.c_str());
		if (data.IsValid()) {
			const bool flipZ = props.Get(Property(propName + ".flipz")(false)).Get<bool>();
			iesMap = IESSphericalFunction::IES2ImageMap(data, flipZ,
					(width > 0) ? width : 512,
					(height > 0) ? height : 256);
		} else
			throw runtime_error("Invalid IES file in property " + propName + ": " + iesName);

		// Merge the 2 maps
		map = ImageMap::Merge(imgMap, iesMap, imgMap->GetChannelCount(),
				(width > 0) ? width: Max(imgMap->GetWidth(), iesMap->GetWidth()),
				(height > 0) ? height : Max(imgMap->GetHeight(), iesMap->GetHeight()));
		delete iesMap;

		if ((width > 0) || (height > 0)) {
			// I have to resample the image
			ImageMap *resampledMap = ImageMap::Resample(map, map->GetChannelCount(),
					(width > 0) ? width: map->GetWidth(),
					(height > 0) ? height : map->GetHeight());
			delete map;
			map = resampledMap;
		}

		// Add the image map to the cache
		imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_MERGEDMAP_" + propName, map);
	} else if (imgMapName != "") {
		map = imgMapCache.GetImageMap(imgMapName, gamma,
				ImageMapStorage::DEFAULT, ImageMapStorage::FLOAT);

		if ((width > 0) || (height > 0)) {
			// I have to resample the image
			map = ImageMap::Resample(map, map->GetChannelCount(),
					(width > 0) ? width: map->GetWidth(),
					(height > 0) ? height : map->GetHeight());

			// Add the image map to the cache
			imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_RESAMPLED_" + propName, map);
		}
	} else if (iesName != "") {
		PhotometricDataIES data(iesName.c_str());
		if (data.IsValid()) {
			const bool flipZ = props.Get(Property(propName + ".flipz")(false)).Get<bool>();
			map = IESSphericalFunction::IES2ImageMap(data, flipZ,
					(width > 0) ? width : 512,
					(height > 0) ? height : 256);

			// Add the image map to the cache
			imgMapCache.DefineImageMap("LUXCORE_EMISSIONMAP_IES2IMAGEMAP_" + propName, map);
		} else
			throw runtime_error("Invalid IES file in property " + propName + ": " + iesName);
	}

	return map;
}

LightSource *Scene::CreateLightSource(const string &lightName, const luxrays::Properties &props) {
	string propName, lightType;

	// The following code is used only for compatibility with the past syntax
	if (lightName == "scene.skylight") {
		SLG_LOG("WARNING: deprecated property scene.skylight");

		propName = "scene.skylight";
		lightType = "sky";
	} else if (lightName == "scene.infinitelight") {
		SLG_LOG("WARNING: deprecated property scene.infinitelight");

		propName = "scene.infinitelight";
		lightType = "infinite";
	} else if (lightName == "scene.sunlight") {
		SLG_LOG("WARNING: deprecated property scene.sunlight");

		propName = "scene.sunlight";
		lightType = "sun";
	} else {
		propName = "scene.lights." + lightName;
		lightType = props.Get(Property(propName + ".type")("sky")).Get<string>();
	}

	NotIntersectableLightSource *lightSource = NULL;
	if (lightType == "sky") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight *sl = new SkyLight();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "sky2") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight2 *sl = new SkyLight2();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->groundAlbedo = props.Get(Property(propName + ".groundalbedo")(Spectrum())).Get<Spectrum>().Clamp(0.f);
		sl->hasGround = props.Get(Property(propName + ".ground.enable")(false)).Get<bool>();
		sl->hasGroundAutoScale = props.Get(Property(propName + ".ground.autoscale")(true)).Get<bool>();
		sl->groundColor = props.Get(Property(propName + ".ground.color")(Spectrum(.75f, .75f, .75f))).Get<Spectrum>().Clamp(0.f);
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "infinite") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".file")("image.png")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const ImageMapStorage::StorageType storageType = ImageMapStorage::String2StorageType(
			props.Get(Property(propName + ".storage")("auto")).Get<string>());

		const ImageMap *imgMap = imgMapCache.GetImageMap(imageName, gamma,
				ImageMapStorage::DEFAULT, storageType);

		InfiniteLight *il = new InfiniteLight();
		il->lightToWorld = light2World;
		il->imageMap = imgMap;
		il->sampleUpperHemisphereOnly = props.Get(Property(propName + ".sampleupperhemisphereonly")(false)).Get<bool>();

		// An old parameter kept only for compatibility
		const UV shift = props.Get(Property(propName + ".shift")(0.f, 0.f)).Get<UV>();
		il->mapping.uDelta = shift.u;
		il->mapping.vDelta = shift.v;

		il->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		il->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		il->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = il;
	} else if (lightType == "sun") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SunLight *sl = new SunLight();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->relSize = Max(0.01f, props.Get(Property(propName + ".relsize")(1.f)).Get<float>());
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = sl;
	} else if (lightType == "point") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		PointLight *pl = new PointLight();
		pl->lightToWorld = light2World;
		pl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		pl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = pl;
	} else if (lightType == "mappoint") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		ImageMap *map = CreateEmissionMap(propName, props);
		if (!map)
			throw runtime_error("MapPoint light source (" + propName + ") is missing mapfile or iesfile property");

		MapPointLight *mpl = new MapPointLight();
		mpl->lightToWorld = light2World;
		mpl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		mpl->imageMap = map;
		mpl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		mpl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		mpl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = mpl;
	} else if (lightType == "spot") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SpotLight *sl = new SpotLight();
		sl->lightToWorld = light2World;
		sl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		sl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		sl->coneAngle = Max(0.f, props.Get(Property(propName + ".coneangle")(30.f)).Get<float>());
		sl->coneDeltaAngle = Max(0.f, props.Get(Property(propName + ".conedeltaangle")(5.f)).Get<float>());
		sl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		sl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		sl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = sl;
	} else if (lightType == "projection") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".mapfile")("")).Get<string>();
		const float gamma = props.Get(Property(propName + ".gamma")(2.2f)).Get<float>();
		const ImageMapStorage::StorageType storageType = ImageMapStorage::String2StorageType(
			props.Get(Property(propName + ".storage")("auto")).Get<string>());

		const ImageMap *imgMap = (imageName == "") ?
			NULL :
			imgMapCache.GetImageMap(imageName, gamma, ImageMapStorage::DEFAULT, storageType);

		ProjectionLight *pl = new ProjectionLight();
		pl->lightToWorld = light2World;
		pl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		pl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());
		pl->imageMap = imgMap;
		pl->fov = Max(0.f, props.Get(Property(propName + ".fov")(45.f)).Get<float>());

		lightSource = pl;
	} else if (lightType == "laser") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		LaserLight *ll = new LaserLight();
		ll->lightToWorld = light2World;
		ll->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		ll->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		ll->radius = Max(0.f, props.Get(Property(propName + ".radius")(.01f)).Get<float>());
		ll->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		ll->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		ll->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = ll;
	} else if (lightType == "constantinfinite") {
		ConstantInfiniteLight *cil = new ConstantInfiniteLight();

		cil->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		cil->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		cil->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		cil->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		lightSource = cil;
	} else if (lightType == "sharpdistant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SharpDistantLight *sdl = new SharpDistantLight();
		sdl->lightToWorld = light2World;
		sdl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		sdl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());

		lightSource = sdl;
	} else if (lightType == "distant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		DistantLight *dl = new DistantLight();
		dl->lightToWorld = light2World;
		dl->color = props.Get(Property(propName + ".color")(Spectrum(1.f))).Get<Spectrum>();
		dl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());
		dl->theta = props.Get(Property(propName + ".theta")(10.f)).Get<float>();

		lightSource = dl;
	} else
		throw runtime_error("Unknown light type: " + lightType);

	lightSource->gain = props.Get(Property(propName + ".gain")(Spectrum(1.f))).Get<Spectrum>();
	lightSource->SetSamples(props.Get(Property(propName + ".samples")(-1)).Get<int>());
	lightSource->SetID(props.Get(Property(propName + ".id")(0)).Get<int>());
	lightSource->Preprocess();
	lightSource->SetImportance(props.Get(Property(propName + ".importance")(1.f)).Get<float>());

	return lightSource;
}
