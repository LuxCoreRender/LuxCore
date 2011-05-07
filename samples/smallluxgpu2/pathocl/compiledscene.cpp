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

#include"compiledscene.h"

CompiledScene::CompiledScene(RenderConfig *cfg, Film *flm) {
	renderConfig = cfg;
	film = flm;

	infiniteLight = NULL;
	infiniteLightMap = NULL;
	sunLight = NULL;
	skyLight = NULL;
	rgbTexMem = NULL;
	alphaTexMem = NULL;
	meshTexs = NULL;
	meshBumps = NULL;
	bumpMapScales = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete infiniteLight;
	// infiniteLightMap memory is handled from another class
	delete sunLight;
	delete skyLight;
	delete[] rgbTexMem;
	delete[] alphaTexMem;
	delete[] meshTexs;
	delete[] meshBumps;
	delete[] bumpMapScales;
}

void CompiledScene::CompileCamera() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile Camera" << endl;

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	Scene *scene = renderConfig->scene;

	camera.yon = scene->camera->clipYon;
	camera.hither = scene->camera->clipHither;
	camera.lensRadius = scene->camera->lensRadius;
	camera.focalDistance = scene->camera->focalDistance;
	memcpy(&camera.rasterToCameraMatrix[0][0], scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
	memcpy(&camera.cameraToWorldMatrix[0][0], scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));
}

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile Geometry" << endl;

	Scene *scene = renderConfig->scene;

	const unsigned int verticesCount = scene->dataSet->GetTotalVertexCount();
	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	uvs.resize(0);
	tris.resize(0);
	meshDescs.resize(0);

	meshIDs = scene->dataSet->GetMeshIDTable();

	// Check the used accelerator type
	if (scene->dataSet->GetAcceleratorType() == ACCEL_MQBVH) {
		// MQBVH geometry must be defined in a specific way.

		//----------------------------------------------------------------------
		// Translate mesh IDs
		//----------------------------------------------------------------------

		triangleIDs = scene->dataSet->GetMeshTriangleIDTable();

		//----------------------------------------------------------------------
		// Translate geometry
		//----------------------------------------------------------------------

		std::map<ExtMesh *, unsigned int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

		PathOCL::Mesh newMeshDesc;
		newMeshDesc.vertsOffset = 0;
		newMeshDesc.trisOffset = 0;
		memcpy(newMeshDesc.trans, Matrix4x4().m, sizeof(float[4][4]));
		memcpy(newMeshDesc.invTrans, Matrix4x4().m, sizeof(float[4][4]));

		PathOCL::Mesh currentMeshDesc;

		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			bool isExistingInstance;
			if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

				// Check if is one of the already defined meshes
				std::map<ExtMesh *, unsigned int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += imesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += imesh->GetTotalTriangleCount();

					isExistingInstance = false;

					const unsigned int index = meshDescs.size();
					definedMeshs[imesh->GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				memcpy(currentMeshDesc.trans, imesh->GetTransformation().GetMatrix().m, sizeof(float[4][4]));
				memcpy(currentMeshDesc.invTrans, imesh->GetInvTransformation().GetMatrix().m, sizeof(float[4][4]));
				mesh = imesh->GetExtTriangleMesh();
			} else {
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();

				isExistingInstance = false;
			}

			meshDescs.push_back(currentMeshDesc);

			if (!isExistingInstance) {
				assert (mesh->GetType() == TYPE_EXT_TRIANGLE);

				//--------------------------------------------------------------
				// Translate mesh normals
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetNormal(j));

				//----------------------------------------------------------------------
				// Translate vertex uvs
				//----------------------------------------------------------------------

				if (scene->texMapCache->GetSize()) {
					// TODO: I should check if the only texture map is used for infinitelight

					for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
						if (mesh->HasUVs())
							uvs.push_back(mesh->GetUV(j));
						else
							uvs.push_back(UV(0.f, 0.f));
					}
				}

				//--------------------------------------------------------------
				// Translate mesh vertices
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					verts.push_back(mesh->GetVertex(j));

				//--------------------------------------------------------------
				// Translate mesh indices
				//--------------------------------------------------------------

				Triangle *mtris = mesh->GetTriangles();
				for (unsigned int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
					tris.push_back(mtris[j]);
			}
		}
	} else {
		triangleIDs = NULL;

		//----------------------------------------------------------------------
		// Translate mesh normals
		//----------------------------------------------------------------------

		normals.reserve(verticesCount);
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				normals.push_back(mesh->GetNormal(j));
		}

		//----------------------------------------------------------------------
		// Translate vertex uvs
		//----------------------------------------------------------------------

		if (scene->texMapCache->GetSize()) {
			// TODO: I should check if the only texture map is used for infinitelight

			uvs.reserve(verticesCount);
			for (unsigned int i = 0; i < scene->objects.size(); ++i) {
				ExtMesh *mesh = scene->objects[i];

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
					if (mesh->HasUVs())
						uvs.push_back(mesh->GetUV(j));
					else
						uvs.push_back(UV(0.f, 0.f));
				}
			}
		}

		//----------------------------------------------------------------------
		// Translate mesh vertices
		//----------------------------------------------------------------------

		unsigned int *meshOffsets = new unsigned int[scene->objects.size()];
		verts.reserve(verticesCount);
		unsigned int vIndex = 0;
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			meshOffsets[i] = vIndex;
			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts.push_back(mesh->GetVertex(j));

			vIndex += mesh->GetTotalVertexCount();
		}

		//----------------------------------------------------------------------
		// Translate mesh indices
		//----------------------------------------------------------------------

		tris.reserve(trianglesCount);
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			Triangle *mtris = mesh->GetTriangles();
			const unsigned int moffset = meshOffsets[i];
			for (unsigned int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
				tris.push_back(Triangle(
						mtris[j].v[0] + moffset,
						mtris[j].v[1] + moffset,
						mtris[j].v[2] + moffset));
			}
		}
		delete[] meshOffsets;
	}

	const double tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::CompiledScene] Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::CompileMaterials() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile Materials" << endl;

	Scene *scene = renderConfig->scene;

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	enable_MAT_MATTE = false;
	enable_MAT_AREALIGHT = false;
	enable_MAT_MIRROR = false;
	enable_MAT_GLASS = false;
	enable_MAT_MATTEMIRROR = false;
	enable_MAT_METAL = false;
	enable_MAT_MATTEMETAL = false;
	enable_MAT_ALLOY = false;
	enable_MAT_ARCHGLASS = false;

	const unsigned int materialsCount = scene->materials.size();
	mats.resize(materialsCount);

	for (unsigned int i = 0; i < materialsCount; ++i) {
		Material *m = scene->materials[i];
		PathOCL::Material *gpum = &mats[i];

		switch (m->GetType()) {
			case MATTE: {
				enable_MAT_MATTE = true;
				MatteMaterial *mm = (MatteMaterial *)m;

				gpum->type = MAT_MATTE;
				gpum->param.matte.r = mm->GetKd().r;
				gpum->param.matte.g = mm->GetKd().g;
				gpum->param.matte.b = mm->GetKd().b;
				break;
			}
			case AREALIGHT: {
				enable_MAT_AREALIGHT = true;
				AreaLightMaterial *alm = (AreaLightMaterial *)m;

				gpum->type = MAT_AREALIGHT;
				gpum->param.areaLight.gain_r = alm->GetGain().r;
				gpum->param.areaLight.gain_g = alm->GetGain().g;
				gpum->param.areaLight.gain_b = alm->GetGain().b;
				break;
			}
			case MIRROR: {
				enable_MAT_MIRROR = true;
				MirrorMaterial *mm = (MirrorMaterial *)m;

				gpum->type = MAT_MIRROR;
				gpum->param.mirror.r = mm->GetKr().r;
				gpum->param.mirror.g = mm->GetKr().g;
				gpum->param.mirror.b = mm->GetKr().b;
				gpum->param.mirror.specularBounce = mm->HasSpecularBounceEnabled();
				break;
			}
			case GLASS: {
				enable_MAT_GLASS = true;
				GlassMaterial *gm = (GlassMaterial *)m;

				gpum->type = MAT_GLASS;
				gpum->param.glass.refl_r = gm->GetKrefl().r;
				gpum->param.glass.refl_g = gm->GetKrefl().g;
				gpum->param.glass.refl_b = gm->GetKrefl().b;

				gpum->param.glass.refrct_r = gm->GetKrefrct().r;
				gpum->param.glass.refrct_g = gm->GetKrefrct().g;
				gpum->param.glass.refrct_b = gm->GetKrefrct().b;

				gpum->param.glass.ousideIor = gm->GetOutsideIOR();
				gpum->param.glass.ior = gm->GetIOR();
				gpum->param.glass.R0 = gm->GetR0();
				gpum->param.glass.reflectionSpecularBounce = gm->HasReflSpecularBounceEnabled();
				gpum->param.glass.transmitionSpecularBounce = gm->HasRefrctSpecularBounceEnabled();
				break;
			}
			case MATTEMIRROR: {
				enable_MAT_MATTEMIRROR = true;
				MatteMirrorMaterial *mmm = (MatteMirrorMaterial *)m;

				gpum->type = MAT_MATTEMIRROR;
				gpum->param.matteMirror.matte.r = mmm->GetMatte().GetKd().r;
				gpum->param.matteMirror.matte.g = mmm->GetMatte().GetKd().g;
				gpum->param.matteMirror.matte.b = mmm->GetMatte().GetKd().b;

				gpum->param.matteMirror.mirror.r = mmm->GetMirror().GetKr().r;
				gpum->param.matteMirror.mirror.g = mmm->GetMirror().GetKr().g;
				gpum->param.matteMirror.mirror.b = mmm->GetMirror().GetKr().b;
				gpum->param.matteMirror.mirror.specularBounce = mmm->GetMirror().HasSpecularBounceEnabled();

				gpum->param.matteMirror.matteFilter = mmm->GetMatteFilter();
				gpum->param.matteMirror.totFilter = mmm->GetTotFilter();
				gpum->param.matteMirror.mattePdf = mmm->GetMattePdf();
				gpum->param.matteMirror.mirrorPdf = mmm->GetMirrorPdf();
				break;
			}
			case METAL: {
				enable_MAT_METAL = true;
				MetalMaterial *mm = (MetalMaterial *)m;

				gpum->type = MAT_METAL;
				gpum->param.metal.r = mm->GetKr().r;
				gpum->param.metal.g = mm->GetKr().g;
				gpum->param.metal.b = mm->GetKr().b;
				gpum->param.metal.exponent = mm->GetExp();
				gpum->param.metal.specularBounce = mm->HasSpecularBounceEnabled();
				break;
			}
			case MATTEMETAL: {
				enable_MAT_MATTEMETAL = true;
				MatteMetalMaterial *mmm = (MatteMetalMaterial *)m;

				gpum->type = MAT_MATTEMETAL;
				gpum->param.matteMetal.matte.r = mmm->GetMatte().GetKd().r;
				gpum->param.matteMetal.matte.g = mmm->GetMatte().GetKd().g;
				gpum->param.matteMetal.matte.b = mmm->GetMatte().GetKd().b;

				gpum->param.matteMetal.metal.r = mmm->GetMetal().GetKr().r;
				gpum->param.matteMetal.metal.g = mmm->GetMetal().GetKr().g;
				gpum->param.matteMetal.metal.b = mmm->GetMetal().GetKr().b;
				gpum->param.matteMetal.metal.exponent = mmm->GetMetal().GetExp();
				gpum->param.matteMetal.metal.specularBounce = mmm->GetMetal().HasSpecularBounceEnabled();

				gpum->param.matteMetal.matteFilter = mmm->GetMatteFilter();
				gpum->param.matteMetal.totFilter = mmm->GetTotFilter();
				gpum->param.matteMetal.mattePdf = mmm->GetMattePdf();
				gpum->param.matteMetal.metalPdf = mmm->GetMetalPdf();
				break;
			}
			case ALLOY: {
				enable_MAT_ALLOY = true;
				AlloyMaterial *am = (AlloyMaterial *)m;

				gpum->type = MAT_ALLOY;
				gpum->param.alloy.refl_r= am->GetKrefl().r;
				gpum->param.alloy.refl_g = am->GetKrefl().g;
				gpum->param.alloy.refl_b = am->GetKrefl().b;

				gpum->param.alloy.diff_r = am->GetKd().r;
				gpum->param.alloy.diff_g = am->GetKd().g;
				gpum->param.alloy.diff_b = am->GetKd().b;

				gpum->param.alloy.exponent = am->GetExp();
				gpum->param.alloy.R0 = am->GetR0();
				gpum->param.alloy.specularBounce = am->HasSpecularBounceEnabled();
				break;
			}
			case ARCHGLASS: {
				enable_MAT_ARCHGLASS = true;
				ArchGlassMaterial *agm = (ArchGlassMaterial *)m;

				gpum->type = MAT_ARCHGLASS;
				gpum->param.archGlass.refl_r = agm->GetKrefl().r;
				gpum->param.archGlass.refl_g = agm->GetKrefl().g;
				gpum->param.archGlass.refl_b = agm->GetKrefl().b;

				gpum->param.archGlass.refrct_r = agm->GetKrefrct().r;
				gpum->param.archGlass.refrct_g = agm->GetKrefrct().g;
				gpum->param.archGlass.refrct_b = agm->GetKrefrct().b;

				gpum->param.archGlass.transFilter = agm->GetTransFilter();
				gpum->param.archGlass.totFilter = agm->GetTotFilter();
				gpum->param.archGlass.reflPdf = agm->GetReflPdf();
				gpum->param.archGlass.transPdf = agm->GetTransPdf();
				break;
			}
			default: {
				enable_MAT_MATTE = true;
				gpum->type = MAT_MATTE;
				gpum->param.matte.r = 0.75f;
				gpum->param.matte.g = 0.75f;
				gpum->param.matte.b = 0.75f;
				break;
			}
		}
	}


	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const unsigned int meshCount = scene->objectMaterials.size();
	meshMats.resize(meshCount);
	for (unsigned int i = 0; i < meshCount; ++i) {
		Material *m = scene->objectMaterials[i];

		// Look for the index
		unsigned int index = 0;
		for (unsigned int j = 0; j < materialsCount; ++j) {
			if (m == scene->materials[j]) {
				index = j;
				break;
			}
		}

		meshMats[i] = index;
	}

	const double tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::CompiledScene] Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::CompileAreaLights() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile AreaLights" << endl;

	Scene *scene = renderConfig->scene;

	//--------------------------------------------------------------------------
	// Translate area lights
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// Count the area lights
	unsigned int areaLightCount = 0;
	for (unsigned int i = 0; i < scene->lights.size(); ++i) {
		if (scene->lights[i]->IsAreaLight())
			++areaLightCount;
	}

	areaLights.resize(areaLightCount);
	if (areaLightCount > 0) {
		unsigned int index = 0;
		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			if (scene->lights[i]->IsAreaLight()) {
				const TriangleLight *tl = (TriangleLight *)scene->lights[i];
				const ExtMesh *mesh = scene->objects[tl->GetMeshIndex()];
				const Triangle *tri = &(mesh->GetTriangles()[tl->GetTriIndex()]);

				PathOCL::TriangleLight *cpl = &areaLights[index];
				cpl->v0 = mesh->GetVertex(tri->v[0]);
				cpl->v1 = mesh->GetVertex(tri->v[1]);
				cpl->v2 = mesh->GetVertex(tri->v[2]);

				cpl->normal = mesh->GetNormal(tri->v[0]);

				cpl->area = tl->GetArea();

				AreaLightMaterial *alm = (AreaLightMaterial *)tl->GetMaterial();
				cpl->gain_r = alm->GetGain().r;
				cpl->gain_g = alm->GetGain().g;
				cpl->gain_b = alm->GetGain().b;

				++index;
			}
		}
	}

	const double tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::CompiledScene] Area lights compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;

}

void CompiledScene::CompileInfiniteLight() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile InfiniteLight" << endl;

	Scene *scene = renderConfig->scene;

	delete infiniteLight;

	//--------------------------------------------------------------------------
	// Check if there is an infinite light source
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	InfiniteLight *il = NULL;
	if (scene->infiniteLight && (
			(scene->infiniteLight->GetType() == TYPE_IL_BF) ||
			(scene->infiniteLight->GetType() == TYPE_IL_PORTAL) ||
			(scene->infiniteLight->GetType() == TYPE_IL_IS)))
		il = scene->infiniteLight;
	else {
		// Look for the infinite light

		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			LightSource *l = scene->lights[i];

			if ((l->GetType() == TYPE_IL_BF) || (l->GetType() == TYPE_IL_PORTAL) ||
					(l->GetType() == TYPE_IL_IS)) {
				il = (InfiniteLight *)l;
				break;
			}
		}
	}

	if (il) {
		infiniteLight = new PathOCL::InfiniteLight();

		infiniteLight->gain = il->GetGain();
		infiniteLight->shiftU = il->GetShiftU();
		infiniteLight->shiftV = il->GetShiftV();
		const TextureMap *texMap = il->GetTexture()->GetTexMap();
		infiniteLight->width = texMap->GetWidth();
		infiniteLight->height = texMap->GetHeight();

		infiniteLightMap = texMap->GetPixels();
	} else {
		infiniteLight = NULL;
		infiniteLightMap = NULL;
	}

	const double tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::CompiledScene] Infinitelight compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::CompileSunLight() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile SunLight" << endl;

	Scene *scene = renderConfig->scene;

	delete sunLight;

	//--------------------------------------------------------------------------
	// Check if there is an sun light source
	//--------------------------------------------------------------------------

	SunLight *sl = NULL;

	// Look for the sun light
	for (unsigned int i = 0; i < scene->lights.size(); ++i) {
		LightSource *l = scene->lights[i];

		if (l->GetType() == TYPE_SUN) {
			sl = (SunLight *)l;
			break;
		}
	}

	if (sl) {
		sunLight = new PathOCL::SunLight();

		sunLight->sundir = sl->GetDir();
		sunLight->gain = sl->GetGain();
		sunLight->turbidity = sl->GetTubidity();
		sunLight->relSize= sl->GetRelSize();
		float tmp;
		sl->GetInitData(&sunLight->x, &sunLight->y, &tmp, &tmp, &tmp,
				&sunLight->cosThetaMax, &tmp, &sunLight->suncolor);
	} else
		sunLight = NULL;
}

void CompiledScene::CompileSkyLight() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile SkyLight" << endl;

	Scene *scene = renderConfig->scene;

	delete skyLight;

	//--------------------------------------------------------------------------
	// Check if there is an sky light source
	//--------------------------------------------------------------------------

	SkyLight *sl = NULL;

	if (scene->infiniteLight && (scene->infiniteLight->GetType() == TYPE_IL_SKY))
		sl = (SkyLight *)scene->infiniteLight;
	else {
		// Look for the sky light
		for (unsigned int i = 0; i < scene->lights.size(); ++i) {
			LightSource *l = scene->lights[i];

			if (l->GetType() == TYPE_IL_SKY) {
				sl = (SkyLight *)l;
				break;
			}
		}
	}

	if (sl) {
		skyLight = new PathOCL::SkyLight();

		skyLight->gain = sl->GetGain();
		sl->GetInitData(&skyLight->thetaS, &skyLight->phiS,
				&skyLight->zenith_Y, &skyLight->zenith_x, &skyLight->zenith_y,
				skyLight->perez_Y, skyLight->perez_x, skyLight->perez_y);
	} else
		skyLight = NULL;
}

void CompiledScene::CompileTextureMaps() {
	cerr << "[PathOCLRenderThread::CompiledScene] Compile TextureMaps" << endl;

	Scene *scene = renderConfig->scene;

	gpuTexMaps.resize(0);
	delete[] rgbTexMem;
	delete[] alphaTexMem;
	delete[] meshTexs;
	delete[] meshBumps;
	delete[] bumpMapScales;

	//--------------------------------------------------------------------------
	// Translate mesh texture maps
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	std::vector<TextureMap *> tms;
	scene->texMapCache->GetTexMaps(tms);
	// Calculate the amount of ram to allocate
	totRGBTexMem = 0;
	totAlphaTexMem = 0;

	for (unsigned int i = 0; i < tms.size(); ++i) {
		TextureMap *tm = tms[i];
		const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

		totRGBTexMem += pixelCount;
		if (tm->HasAlpha())
			totAlphaTexMem += pixelCount;
	}

	// Allocate texture map memory
	if ((totRGBTexMem > 0) || (totAlphaTexMem > 0)) {
		gpuTexMaps.resize(tms.size());

		if (totRGBTexMem > 0) {
			unsigned int rgbOffset = 0;
			rgbTexMem = new Spectrum[totRGBTexMem];

			for (unsigned int i = 0; i < tms.size(); ++i) {
				TextureMap *tm = tms[i];
				const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

				memcpy(&rgbTexMem[rgbOffset], tm->GetPixels(), pixelCount * sizeof(Spectrum));
				gpuTexMaps[i].rgbOffset = rgbOffset;
				rgbOffset += pixelCount;
			}
		} else
			rgbTexMem = NULL;

		if (totAlphaTexMem > 0) {
			unsigned int alphaOffset = 0;
			alphaTexMem = new float[totAlphaTexMem];

			for (unsigned int i = 0; i < tms.size(); ++i) {
				TextureMap *tm = tms[i];
				const unsigned int pixelCount = tm->GetWidth() * tm->GetHeight();

				if (tm->HasAlpha()) {
					memcpy(&alphaTexMem[alphaOffset], tm->GetAlphas(), pixelCount * sizeof(float));
					gpuTexMaps[i].alphaOffset = alphaOffset;
					alphaOffset += pixelCount;
				} else
					gpuTexMaps[i].alphaOffset = 0xffffffffu;
			}
		} else
			alphaTexMem = NULL;

		//----------------------------------------------------------------------

		// Translate texture map description
		for (unsigned int i = 0; i < tms.size(); ++i) {
			TextureMap *tm = tms[i];
			gpuTexMaps[i].width = tm->GetWidth();
			gpuTexMaps[i].height = tm->GetHeight();
		}

		//----------------------------------------------------------------------

		// Translate mesh texture indices
		const unsigned int meshCount = meshMats.size();
		meshTexs = new unsigned int[meshCount];
		for (unsigned int i = 0; i < meshCount; ++i) {
			TexMapInstance *t = scene->objectTexMaps[i];

			if (t) {
				// Look for the index
				unsigned int index = 0;
				for (unsigned int j = 0; j < tms.size(); ++j) {
					if (t->GetTexMap() == tms[j]) {
						index = j;
						break;
					}
				}

				meshTexs[i] = index;
			} else
				meshTexs[i] = 0xffffffffu;
		}

		//----------------------------------------------------------------------

		// Translate mesh bump map indices
		bool hasBumpMapping = false;
		meshBumps = new unsigned int[meshCount];
		for (unsigned int i = 0; i < meshCount; ++i) {
			BumpMapInstance *bm = scene->objectBumpMaps[i];

			if (bm) {
				// Look for the index
				unsigned int index = 0;
				for (unsigned int j = 0; j < tms.size(); ++j) {
					if (bm->GetTexMap() == tms[j]) {
						index = j;
						break;
					}
				}

				meshBumps[i] = index;
				hasBumpMapping = true;
			} else
				meshBumps[i] = 0xffffffffu;
		}

		if (hasBumpMapping) {
			bumpMapScales = new float[meshCount];
			for (unsigned int i = 0; i < meshCount; ++i) {
				BumpMapInstance *bm = scene->objectBumpMaps[i];

				if (bm)
					bumpMapScales[i] = bm->GetScale();
				else
					bumpMapScales[i] = 1.f;
			}
		} else {
			delete[] meshBumps;
			meshBumps = NULL;
			bumpMapScales = NULL;
		}
	} else {
		gpuTexMaps.resize(0);
		rgbTexMem = NULL;
		alphaTexMem = NULL;
		meshTexs = NULL;
		meshBumps = NULL;
		bumpMapScales = NULL;
	}

	const double tEnd = WallClockTime();
	cerr << "[PathOCLRenderThread::CompiledScene] Texture maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT) || editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileMaterials();
	if (editActions.Has(AREALIGHTS_EDIT))
		CompileAreaLights();
	if (editActions.Has(INFINITELIGHT_EDIT))
		CompileInfiniteLight();
	if (editActions.Has(SUNLIGHT_EDIT))
		CompileSunLight();
	if (editActions.Has(SKYLIGHT_EDIT))
		CompileSkyLight();
	if (editActions.Has(TEXTUREMAPS_EDIT))
		CompileTextureMaps();
}

bool CompiledScene::IsMaterialCompiled(const MaterialType type) const {
	switch (type) {
		case MATTE:
			return enable_MAT_MATTE;
		case AREALIGHT:
			return enable_MAT_AREALIGHT;
		case MIRROR:
			return enable_MAT_MIRROR;
		case MATTEMIRROR:
			return enable_MAT_MATTEMIRROR;
		case GLASS:
			return enable_MAT_GLASS;
		case METAL:
			return enable_MAT_METAL;
		case MATTEMETAL:
			return enable_MAT_MATTEMETAL;
		case ARCHGLASS:
			return enable_MAT_ARCHGLASS;
		case ALLOY:
			return enable_MAT_ALLOY;
		default:
			assert (false);
			return false;
			break;
	}
}
