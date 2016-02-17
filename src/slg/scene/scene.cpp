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

#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <sstream>
#include <set>
#include <vector>
#include <memory>

#include <boost/detail/container_fwd.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/format.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include "luxrays/core/randomgen.h"
#include "luxrays/core/dataset.h"
#include "luxrays/core/intersectiondevice.h"
#include "luxrays/utils/properties.h"
#include "slg/core/sphericalfunction/sphericalfunction.h"
#include "slg/editaction.h"
#include "slg/samplers/sampler.h"
#include "slg/core/sdl.h"
#include "slg/scene/scene.h"
#include "slg/textures/constfloat.h"
#include "slg/textures/constfloat3.h"

using namespace std;
using namespace luxrays;
using namespace slg;

Scene::Scene(const float imageScale) {
	Init(imageScale);
}

Scene::Scene(const string &fileName, const float imageScale) {
	Init(imageScale);

	SDL_LOG("Reading scene: " << fileName);

	Properties scnProp(fileName);
	Parse(scnProp);
}

void Scene::Init(const float imageScale) {
	defaultWorldVolume = NULL;
	// Just in case there is an unexpected exception during the scene loading
    camera = NULL;

	dataSet = NULL;

	editActions.AddAllAction();
	imgMapCache.SetImageResize(imageScale);

	enableParsePrint = false;
}

Scene::~Scene() {
	delete camera;

	delete dataSet;
}

void Scene::PreprocessCamera(const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion) {
	camera->Update(filmWidth, filmHeight, filmSubRegion);
}

void Scene::Preprocess(Context *ctx,
		const u_int filmWidth, const u_int filmHeight, const u_int *filmSubRegion,
		const AcceleratorType accelType, const bool enableInstanceSupport) {
	if (lightDefs.GetSize() == 0) {
		throw runtime_error("The scene doesn't include any light source (note: volume emission doesn't count for this check)");

		// There may be only a volume emitting light. However I ignore this case
		// because a lot of code has been written assuming that there is always
		// at least one light source (i.e. for direct light sampling).
		/*bool hasEmittingVolume = false;
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (vol && vol->GetVolumeEmissionTexture() &&
					(vol->GetVolumeEmissionTexture()->Y() > 0.f)) {
				hasEmittingVolume = true;
				break;
			}
		}

		if (!hasEmittingVolume)
			throw runtime_error("The scene doesn't include any light source");*/
	}

	// Check if I have to update the camera
	if (editActions.Has(CAMERA_EDIT))
		PreprocessCamera(filmWidth, filmHeight, filmSubRegion);

	// Check if I have to rebuild the dataset
	if (editActions.Has(GEOMETRY_EDIT)) {
		// Rebuild the data set
		delete dataSet;
		dataSet = new DataSet(ctx);
		dataSet->SetInstanceSupport(enableInstanceSupport);
		dataSet->SetAcceleratorType(accelType);

		// Add all objects
		for (u_int i = 0; i < objDefs.GetSize(); ++i)
			dataSet->Add(objDefs.GetSceneObject(i)->GetExtMesh());

		dataSet->Preprocess();
	}

	// Check if something has changed in light sources
	if (editActions.Has(GEOMETRY_EDIT) ||
			editActions.Has(MATERIALS_EDIT) ||
			editActions.Has(MATERIAL_TYPES_EDIT) ||
			editActions.Has(LIGHTS_EDIT) ||
			editActions.Has(LIGHT_TYPES_EDIT) ||
			editActions.Has(IMAGEMAPS_EDIT)) {
		lightDefs.Preprocess(this);
	}

	editActions.Reset();
}

Properties Scene::ToProperties() {
		Properties props;

		// Write the camera information
		props.Set(camera->ToProperties());

		// Save all not intersectable light sources
		for (u_int i = 0; i < lightDefs.GetSize(); ++i) {
			const LightSource *l = lightDefs.GetLightSource(i);
			if (dynamic_cast<const NotIntersectableLightSource *>(l))
				props.Set(((const NotIntersectableLightSource *)l)->ToProperties(imgMapCache));
		}

		// Write the textures information
		for (u_int i = 0; i < texDefs.GetSize(); ++i) {
			const Texture *tex = texDefs.GetTexture(i);
			props.Set(tex->ToProperties(imgMapCache));
		}

		// Write the volumes information
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (vol)
				props.Set(vol->ToProperties());
		}

		// Set the default world interior/exterior volume if required
		if (defaultWorldVolume) {
			const u_int index = matDefs.GetMaterialIndex(defaultWorldVolume);
			props.Set(Property("scene.world.volume.default")(matDefs.GetMaterial(index)->GetName()));
		}

		// Write the materials information
		for (u_int i = 0; i < matDefs.GetSize(); ++i) {
			const Material *mat = matDefs.GetMaterial(i);
			// Check if it is not a volume
			const Volume *vol = dynamic_cast<const Volume *>(mat);
			if (!vol)
				props.Set(mat->ToProperties());
		}

		// Write the object information
		for (u_int i = 0; i < objDefs.GetSize(); ++i) {
			const SceneObject *obj = objDefs.GetSceneObject(i);
			props.Set(obj->ToProperties(extMeshCache));
		}

		return props;
}

//--------------------------------------------------------------------------
// Methods to build and edit a scene
//--------------------------------------------------------------------------

void Scene::DefineImageMap(const string &name, ImageMap *im) {
	imgMapCache.DefineImageMap(name, im);

	editActions.AddAction(IMAGEMAPS_EDIT);
}

bool Scene::IsImageMapDefined(const string &imgMapName) const {
	return imgMapCache.IsImageMapDefined(imgMapName);
}

void Scene::DefineMesh(const string &meshName, luxrays::ExtTriangleMesh *mesh) {
	extMeshCache.DefineExtMesh(meshName, mesh);

	editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DefineMesh(const string &shapeName,
	const long plyNbVerts, const long plyNbTris,
	luxrays::Point *p, luxrays::Triangle *vi, luxrays::Normal *n, luxrays::UV *uv,
	luxrays::Spectrum *cols, float *alphas) {
	extMeshCache.DefineExtMesh(shapeName, plyNbVerts, plyNbTris, p, vi, n, uv, cols, alphas);

	editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DefineStrands(const string &shapeName, const cyHairFile &strandsFile,
		const StrendsShape::TessellationType tesselType,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	StrendsShape shape(this,
			&strandsFile, tesselType,
			adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);

	ExtMesh *mesh = shape.Refine(this);
	extMeshCache.DefineExtMesh(shapeName, mesh);

	editActions.AddAction(GEOMETRY_EDIT);
}

bool Scene::IsTextureDefined(const string &texName) const {
	return texDefs.IsTextureDefined(texName);
}

bool Scene::IsMaterialDefined(const string &matName) const {
	return matDefs.IsMaterialDefined(matName);
}

bool Scene::IsMeshDefined(const string &meshName) const {
	return extMeshCache.IsExtMeshDefined(meshName);
}

void Scene::Parse(const Properties &props) {
	if (enableParsePrint) {
		SDL_LOG("========================Scene::Parse()=========================" << endl <<
				props);
		SDL_LOG("===============================================================");
	}

	//--------------------------------------------------------------------------
	// Read camera position and target
	//--------------------------------------------------------------------------

	ParseCamera(props);

	//--------------------------------------------------------------------------
	// Read all textures
	//--------------------------------------------------------------------------

	ParseTextures(props);

	//--------------------------------------------------------------------------
	// Read all volumes
	//--------------------------------------------------------------------------

	ParseVolumes(props);

	//--------------------------------------------------------------------------
	// Read all materials
	//--------------------------------------------------------------------------

	ParseMaterials(props);

	//--------------------------------------------------------------------------
	// Read all shapes
	//--------------------------------------------------------------------------

	ParseShapes(props);

	//--------------------------------------------------------------------------
	// Read all objects
	//--------------------------------------------------------------------------

	ParseObjects(props);

	//--------------------------------------------------------------------------
	// Read all env. lights
	//--------------------------------------------------------------------------

	ParseLights(props);
}

void Scene::RemoveUnusedImageMaps() {
	// Build a list of all referenced image maps
	boost::unordered_set<const ImageMap *> referencedImgMaps;
	for (u_int i = 0; i < texDefs.GetSize(); ++i)
		texDefs.GetTexture(i)->AddReferencedImageMaps(referencedImgMaps);

	// Add the light image maps
	BOOST_FOREACH(LightSource *l, lightDefs.GetLightSources())
		l->AddReferencedImageMaps(referencedImgMaps);

	// Add the material image maps
	BOOST_FOREACH(Material *m, matDefs.GetMaterials())
		m->AddReferencedImageMaps(referencedImgMaps);

	// Get the list of all defined image maps
	vector<const ImageMap *> ims;
	imgMapCache.GetImageMaps(ims);
	bool deleted = false;
	BOOST_FOREACH(const ImageMap *im, ims) {
		if (referencedImgMaps.count(im) == 0) {
			SDL_LOG("Deleting unreferenced image map: " << imgMapCache.GetPath(im));
			imgMapCache.DeleteImageMap(im);
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(IMAGEMAPS_EDIT);
		// Indices of image maps are changed so I need to update also the
		// textures, materials and light sources
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
		editActions.AddAction(LIGHTS_EDIT);
		editActions.AddAction(LIGHT_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedTextures() {
	// Build a list of all referenced textures names
	boost::unordered_set<const Texture *> referencedTexs;
	for (u_int i = 0; i < matDefs.GetSize(); ++i)
		matDefs.GetMaterial(i)->AddReferencedTextures(referencedTexs);

	// Get the list of all defined textures
	vector<string> definedTexs = texDefs.GetTextureNames();
	bool deleted = false;
	BOOST_FOREACH(const string  &texName, definedTexs) {
		Texture *t = texDefs.GetTexture(texName);

		if (referencedTexs.count(t) == 0) {
			SDL_LOG("Deleting unreferenced texture: " << texName);
			texDefs.DeleteTexture(texName);
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedMaterials() {
	// Build a list of all referenced material names
	boost::unordered_set<const Material *> referencedMats;

	// Add the default world volume
	if (defaultWorldVolume)
		referencedMats.insert(defaultWorldVolume);

	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMaterials(referencedMats);

	// Get the list of all defined materials
	const vector<string> definedMats = matDefs.GetMaterialNames();
	bool deleted = false;
	BOOST_FOREACH(const string  &matName, definedMats) {
		Material *m = matDefs.GetMaterial(matName);

		if (referencedMats.count(m) == 0) {
			SDL_LOG("Deleting unreferenced material: " << matName);
			matDefs.DeleteMaterial(matName);
			deleted = true;
		}
	}

	if (deleted) {
		editActions.AddAction(MATERIALS_EDIT);
		editActions.AddAction(MATERIAL_TYPES_EDIT);
	}
}

void Scene::RemoveUnusedMeshes() {
	// Build a list of all referenced meshes
	boost::unordered_set<const ExtMesh *> referencedMesh;
	for (u_int i = 0; i < objDefs.GetSize(); ++i)
		objDefs.GetSceneObject(i)->AddReferencedMeshes(referencedMesh);

	// Get the list of all defined objects
	const vector<string> definedObjects = objDefs.GetSceneObjectNames();
	bool deleted = false;
	BOOST_FOREACH(const string  &objName, definedObjects) {
		SceneObject *obj = objDefs.GetSceneObject(objName);

		if (referencedMesh.count(obj->GetExtMesh()) == 0) {
			SDL_LOG("Deleting unreferenced mesh: " << objName);
			objDefs.DeleteSceneObject(objName);
			deleted = true;
		}
	}

	if (deleted)
		editActions.AddAction(GEOMETRY_EDIT);
}

void Scene::DeleteObject(const string &objName) {
	if (objDefs.IsSceneObjectDefined(objName)) {
		const SceneObject *oldObj = objDefs.GetSceneObject(objName);
		const bool wasLightSource = oldObj->GetMaterial()->IsLightSource();

		// Check if the old object was a light source
		if (wasLightSource) {
			editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);

			// Delete all old triangle lights
			const ExtMesh *mesh = oldObj->GetExtMesh();
			for (u_int i = 0; i < mesh->GetTotalTriangleCount(); ++i)
				lightDefs.DeleteLightSource(oldObj->GetName() + TRIANGLE_LIGHT_POSTFIX + ToString(i));
		}

		objDefs.DeleteSceneObject(objName);

		editActions.AddAction(GEOMETRY_EDIT);
	}
}

void Scene::DeleteLight(const string &lightName) {
	if (lightDefs.IsLightSourceDefined(lightName)) {
		lightDefs.DeleteLightSource(lightName);

		editActions.AddActions(LIGHTS_EDIT | LIGHT_TYPES_EDIT);
	}
}

//------------------------------------------------------------------------------

bool Scene::Intersect(IntersectionDevice *device,
		const bool fromLight, PathVolumeInfo *volInfo,
		const float initialPassThrough, Ray *ray, RayHit *rayHit, BSDF *bsdf,
		Spectrum *connectionThroughput, const Spectrum *pathThroughput,
		SampleResult *sampleResult) const {
	*connectionThroughput = Spectrum(1.f);

	float passThrough = initialPassThrough;
	const float originalMaxT = ray->maxt;

	for (;;) {
		const bool hit = device->TraceRay(ray, rayHit);

		const Volume *rayVolume = volInfo->GetCurrentVolume();
		if (hit) {
			bsdf->Init(fromLight, *this, *ray, *rayHit, passThrough, volInfo);
			rayVolume = bsdf->hitPoint.intoObject ? bsdf->hitPoint.exteriorVolume : bsdf->hitPoint.interiorVolume;
			ray->maxt = rayHit->t;
		} else if (!rayVolume) {
			// No volume information, I use the default volume
			rayVolume = defaultWorldVolume;
		}

		// Check if there is volume scatter event
		if (rayVolume) {
			// This applies volume transmittance too
			//
			// Note: by using passThrough here, I introduce subtle correlation
			// between scattering events and pass-through events
			Spectrum emis;
			const float t = rayVolume->Scatter(*ray, passThrough, volInfo->IsScatteredStart(),
					connectionThroughput, &emis);

			// Add the volume emitted light to the appropriate light group
			if (!emis.Black()) {
				if (sampleResult)
					sampleResult->AddEmission(rayVolume->GetVolumeLightID(), *pathThroughput, emis);
			}

			if (t > 0.f) {
				// There was a volume scatter event

				// I have to set RayHit fields even if there wasn't a real
				// ray hit
				rayHit->t = t;
				// This is a trick in order to have RayHit::Miss() return
				// false. I assume 0xfffffffeu will trigger a memory fault if
				// used (and the bug will be noticed)
				rayHit->meshIndex = 0xfffffffeu;

				bsdf->Init(fromLight, *this, *ray, *rayVolume, t, passThrough);
				volInfo->SetScatteredStart(true);

				return true;
			}
		}

		if (hit) {
			// Check if the volume priority system tells me to continue to trace the ray
			bool continueToTrace = volInfo->ContinueToTrace(*bsdf);

			// Check if it is a pass through point
			if (!continueToTrace) {
				const Spectrum transp = bsdf->GetPassThroughTransparency();
				if (!transp.Black()) {
					*connectionThroughput *= transp;
					continueToTrace = true;
				}
			}

			if (continueToTrace) {
				// Update volume information
				volInfo->Update(bsdf->GetEventTypes(), *bsdf);

				// It is a transparent material, continue to trace the ray
				ray->mint = rayHit->t + MachineEpsilon::E(rayHit->t);
				ray->maxt = originalMaxT;

				// A safety check
				if (ray->mint >= ray->maxt)
					return false;
			} else
				return true;
		} else {
			// Nothing was hit
			return false;
		}

		// I generate a new random variable starting from the previous one. I'm
		// not really sure about the kind of correlation introduced by this
		// trick.
		passThrough = fabsf(passThrough - .5f) * 2.f;
	}
}
