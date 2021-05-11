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
#include "slg/lights/spotlight.h"
#include "slg/lights/sunlight.h"
#include "slg/lights/trianglelight.h"
#include "slg/lights/spherelight.h"
#include "slg/lights/mapspherelight.h"
#include "slg/utils/filenameresolver.h"


using namespace std;
using namespace luxrays;
using namespace slg;

void Scene::ParseLights(const Properties &props) {
	// The following code is used only for compatibility with the past syntax
	if (props.HaveNames("scene.skylight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.skylight", props);
		lightDefs.DefineLightSource(newLight);
		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
	if (props.HaveNames("scene.infinitelight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.infinitelight", props);
		lightDefs.DefineLightSource(newLight);
		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
	if (props.HaveNames("scene.sunlight")) {
		// Parse all syntax
		LightSource *newLight = CreateLightSource("scene.sunlight", props);
		lightDefs.DefineLightSource(newLight);
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

		if (lightDefs.IsLightSourceDefined(lightName)) {
			SDL_LOG("Light re-definition: " << lightName);
		} else {
			SDL_LOG("Light definition: " << lightName);
		}

		LightSource *newLight = CreateLightSource(lightName, props);
		lightDefs.DefineLightSource(newLight);
		
		if ((newLight->GetType() == TYPE_IL) ||
				(newLight->GetType() == TYPE_MAPPOINT) ||
				(newLight->GetType() == TYPE_MAPSPHERE) ||
				(newLight->GetType() == TYPE_PROJECTION))
			editActions.AddActions(IMAGEMAPS_EDIT);
	}

	editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
}


ImageMap *Scene::CreateEmissionMap(const string &propName, const luxrays::Properties &props) {
	const u_int width = props.Get(Property(propName + ".map.width")(0)).Get<u_int>();
	const u_int height = props.Get(Property(propName + ".map.height")(0)).Get<u_int>();

	//--------------------------------------------------------------------------
	// Read the IES map if available
	//--------------------------------------------------------------------------

	PhotometricDataIES *iesData = NULL;
	if (props.IsDefined(propName + ".iesblob")) {
		const Blob &blob = props.Get(propName + ".iesblob").Get<const Blob &>();

		istringstream ss(string(blob.GetData(), blob.GetSize()));
		
		iesData = new PhotometricDataIES(ss);
	} else if (props.IsDefined(propName + ".iesfile")) {
		const string iesName = SLG_FileNameResolver.ResolveFile(props.Get(propName + ".iesfile").Get<string>());
		iesData = new PhotometricDataIES(iesName.c_str());
	}

	ImageMap *iesMap = NULL;
	if (iesData) {
		if (iesData->IsValid()) {
			const bool flipZ = props.Get(Property(propName + ".flipz")(false)).Get<bool>();
			iesMap = IESSphericalFunction::IES2ImageMap(*iesData, flipZ,
					(width > 0) ? width : 512,
					(height > 0) ? height : 256);

			// Add the image map to the cache
			const string name ="LUXCORE_EMISSIONMAP_IES2IMAGEMAP_" + propName;
			iesMap->SetName(name);
		} else
			throw runtime_error("Invalid IES file in property " + propName);

		delete iesData;
	}

	//--------------------------------------------------------------------------
	// Read the image map if available
	//--------------------------------------------------------------------------

	ImageMap *imgMap = NULL;
	if (props.IsDefined(propName + ".mapfile")) {
		const string imgMapName = props.Get(propName + ".mapfile").Get<string>();

		ImageMapConfig imgCfg(props, propName);
		// Force float storage
		imgCfg.storageType = ImageMapStorage::FLOAT;

		imgMap = imgMapCache.GetImageMap(imgMapName, imgCfg, false);

		if ((width > 0) || (height > 0)) {
			// I have to resample the image
			ImageMap *resampledImgMap = ImageMap::Resample(imgMap, imgMap->GetChannelCount(),
					(width > 0) ? width: imgMap->GetWidth(),
					(height > 0) ? height : imgMap->GetHeight());
			resampledImgMap->Preprocess();

			// Delete the old map
			imgMapCache.DeleteImageMap(imgMap);

			// Add the image map to the cache
			const string name ="LUXCORE_EMISSIONMAP_RESAMPLED_" + propName;
			resampledImgMap->SetName(name);
			imgMapCache.DefineImageMap(resampledImgMap, false);
			imgMap = resampledImgMap;
		}
	}

	//--------------------------------------------------------------------------
	// Define the light source image map
	//--------------------------------------------------------------------------

	// Nothing was defined
	if (!iesMap && !imgMap)
		return NULL;

	ImageMap *map = NULL;
	if (iesMap && imgMap) {
		// Merge the 2 maps
		map = ImageMap::Merge(imgMap, iesMap, imgMap->GetChannelCount());
		map->Preprocess();
		delete iesMap;
		imgMapCache.DeleteImageMap(imgMap);

		// Add the image map to the cache
		const string name ="LUXCORE_EMISSIONMAP_MERGEDMAP_" + propName;
		map->SetName(name);
		imgMapCache.DefineImageMap(map, false);
	} else if (imgMap)
		map = imgMap;
	else if (iesMap) {
		map = iesMap;
		imgMapCache.DefineImageMap(map, false);
	}

	return map;
}

LightSource *Scene::CreateLightSource(const string &name, const luxrays::Properties &props) {
	string propName, lightType, lightName;

	// The following code is used only for compatibility with the past syntax
	if (name == "scene.skylight") {
		SLG_LOG("WARNING: deprecated property scene.skylight");

		lightName = "skylight";
		propName = "scene.skylight";
		lightType = "sky2";
	} else if (name == "scene.infinitelight") {
		SLG_LOG("WARNING: deprecated property scene.infinitelight");

		lightName = "infinitelight";
		propName = "scene.infinitelight";
		lightType = "infinite";
	} else if (name == "scene.sunlight") {
		SLG_LOG("WARNING: deprecated property scene.sunlight");

		lightName = "sunlight";
		propName = "scene.sunlight";
		lightType = "sun";
	} else {
		lightName = name;
		propName = "scene.lights." + name;
		lightType = props.Get(Property(propName + ".type")("sky2")).Get<string>();
	}

	NotIntersectableLightSource *lightSource = NULL;
	if (lightType == "sky2") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SkyLight2 *sl = new SkyLight2();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->groundAlbedo = GetColor(Property(propName + ".groundalbedo")(Spectrum())).Clamp(0.f);
		sl->hasGround = props.Get(Property(propName + ".ground.enable")(false)).Get<bool>();
		sl->hasGroundAutoScale = props.Get(Property(propName + ".ground.autoscale")(true)).Get<bool>();
		sl->groundColor = GetColor(Property(propName + ".ground.color")(Spectrum(.75f, .75f, .75f))).Clamp(0.f);
		sl->localSunDir = Normalize(props.Get(Property(propName + ".dir")(0.f, 0.f, 1.f)).Get<Vector>());

		sl->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		sl->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		sl->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		// Visibility map related options
		sl->distributionWidth = props.Get(Property(propName + ".distribution.width")(512)).Get<u_int>();
		sl->distributionHeight = props.Get(Property(propName + ".distribution.height")(256)).Get<u_int>();

		// Visibility map cache related options
		sl->useVisibilityMapCache = props.Get(Property(propName + ".visibilitymapcache.enable")(false)).Get<bool>();
		if (sl->useVisibilityMapCache)
			sl->visibilityMapCacheParams = EnvLightVisibilityCache::Properties2Params(propName, props);

		lightSource = sl;
	} else if (lightType == "infinite") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".file")("image.png")).Get<string>();

		const ImageMap *imgMap = imgMapCache.GetImageMap(imageName, ImageMapConfig(props, propName), false);

		InfiniteLight *il = new InfiniteLight();
		il->lightToWorld = light2World;
		il->imageMap = imgMap;
		il->sampleUpperHemisphereOnly = props.Get(Property(propName + ".sampleupperhemisphereonly")(false)).Get<bool>();

		il->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		il->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		il->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		// Visibility map cache related options
		il->useVisibilityMapCache = props.Get(Property(propName + ".visibilitymapcache.enable")(false)).Get<bool>();
		if (il->useVisibilityMapCache)
			il->visibilityMapCacheParams = EnvLightVisibilityCache::Properties2Params(propName, props);

		lightSource = il;
	} else if (lightType == "sun") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SunLight *sl = new SunLight();
		sl->lightToWorld = light2World;
		sl->turbidity = Max(0.f, props.Get(Property(propName + ".turbidity")(2.2f)).Get<float>());
		sl->relSize = Max(1.f, props.Get(Property(propName + ".relsize")(1.f)).Get<float>());
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
		pl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
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
		mpl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		mpl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		mpl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
		mpl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = mpl;
	} else if (lightType == "sphere") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SphereLight *sl = new SphereLight();
		sl->lightToWorld = light2World;
		sl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		sl->radius = Max(0.f, props.Get(Property(propName + ".radius")(1.f)).Get<float>());
		sl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		sl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		sl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
		sl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = sl;
	} else if (lightType == "mapsphere") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		ImageMap *map = CreateEmissionMap(propName, props);
		if (!map)
			throw runtime_error("MapSphere light source (" + propName + ") is missing mapfile or iesfile property");

		MapSphereLight *msl = new MapSphereLight();
		msl->lightToWorld = light2World;
		msl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		msl->radius = Max(0.f, props.Get(Property(propName + ".radius")(1.f)).Get<float>());
		msl->imageMap = map;
		msl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		msl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		msl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
		msl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = msl;
	} else if (lightType == "spot") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SpotLight *sl = new SpotLight();
		sl->lightToWorld = light2World;
		sl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		sl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		sl->coneAngle = Max(0.f, props.Get(Property(propName + ".coneangle")(30.f)).Get<float>());
		sl->coneDeltaAngle = Max(0.f, props.Get(Property(propName + ".conedeltaangle")(5.f)).Get<float>());
		sl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		sl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		sl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
		sl->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = sl;
	} else if (lightType == "projection") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		const string imageName = props.Get(Property(propName + ".mapfile")("")).Get<string>();

		const ImageMap *imgMap = (imageName == "") ?
			NULL :
			imgMapCache.GetImageMap(imageName, ImageMapConfig(props, propName), false);

		ProjectionLight *pl = new ProjectionLight();
		pl->lightToWorld = light2World;
		pl->localPos = props.Get(Property(propName + ".position")(Point())).Get<Point>();
		pl->localTarget = props.Get(Property(propName + ".target")(Point(0.f, 0.f, 1.f))).Get<Point>();
		pl->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		pl->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
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
		ll->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		ll->power = Max(0.f, props.Get(Property(propName + ".power")(0.f)).Get<float>());
		ll->emittedPowerNormalize = props.Get(Property(propName + ".normalizebycolor")(true)).Get<bool>();
		ll->efficency = Max(0.f, props.Get(Property(propName + ".efficency")(0.f)).Get<float>());

		lightSource = ll;
	} else if (lightType == "constantinfinite") {
		ConstantInfiniteLight *cil = new ConstantInfiniteLight();

		cil->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		cil->SetIndirectDiffuseVisibility(props.Get(Property(propName + ".visibility.indirect.diffuse.enable")(true)).Get<bool>());
		cil->SetIndirectGlossyVisibility(props.Get(Property(propName + ".visibility.indirect.glossy.enable")(true)).Get<bool>());
		cil->SetIndirectSpecularVisibility(props.Get(Property(propName + ".visibility.indirect.specular.enable")(true)).Get<bool>());

		// Visibility map cache related options
		cil->useVisibilityMapCache = props.Get(Property(propName + ".visibilitymapcache.enable")(false)).Get<bool>();
		if (cil->useVisibilityMapCache)
			cil->visibilityMapCacheParams = EnvLightVisibilityCache::Properties2Params(propName, props);

		lightSource = cil;
	} else if (lightType == "sharpdistant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		SharpDistantLight *sdl = new SharpDistantLight();
		sdl->lightToWorld = light2World;
		sdl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		sdl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());

		lightSource = sdl;
	} else if (lightType == "distant") {
		const Matrix4x4 mat = props.Get(Property(propName + ".transformation")(Matrix4x4::MAT_IDENTITY)).Get<Matrix4x4>();
		const Transform light2World(mat);

		DistantLight *dl = new DistantLight();
		dl->lightToWorld = light2World;
		dl->color = GetColor(Property(propName + ".color")(Spectrum(1.f)));
		dl->localLightDir = Normalize(props.Get(Property(propName + ".direction")(Vector(0.f, 0.f, 1.f))).Get<Vector>());
		dl->theta = props.Get(Property(propName + ".theta")(10.f)).Get<float>();

		lightSource = dl;
	} else
		throw runtime_error("Unknown light type: " + lightType);

	lightSource->SetName(lightName);
	// Gain is not really a color so I avoid to use GetColor()
	lightSource->gain = props.Get(Property(propName + ".gain")(Spectrum(1.f))).Get<Spectrum>();
	lightSource->SetID(props.Get(Property(propName + ".id")(0)).Get<int>());
	lightSource->SetImportance(props.Get(Property(propName + ".importance")(1.f)).Get<float>());

	if (!lightSource->IsIntersectable()) {
		NotIntersectableLightSource *nils = (NotIntersectableLightSource *)lightSource;

		nils->temperature = props.Get(Property(propName + ".temperature")(-1.f)).Get<float>();
		nils->normalizeTemperature = props.Get(Property(propName + ".temperature.normalize")(false)).Get<bool>();
	}

	lightSource->Preprocess();

	return lightSource;
}
