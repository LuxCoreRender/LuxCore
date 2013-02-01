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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <boost/lexical_cast.hpp>

#include"compiledscene.h"

using namespace std;
using namespace luxrays;
using namespace luxrays::sdl;
using namespace luxrays::utils;

namespace slg {

CompiledScene::CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS) {
	scene = scn;
	film = flm;
	maxMemPageSize = (u_int)Min<size_t>(maxMemPageS, 0xffffffffu);

	infiniteLight = NULL;
	sunLight = NULL;
	skyLight = NULL;
	
	meshFirstTriangleOffset = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete[] meshFirstTriangleOffset;
	delete infiniteLight;
	delete sunLight;
	delete skyLight;
}

void CompiledScene::CompileCamera() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Camera");

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	camera.yon = scene->camera->clipYon;
	camera.hither = scene->camera->clipHither;
	camera.lensRadius = scene->camera->lensRadius;
	camera.focalDistance = scene->camera->focalDistance;
	memcpy(camera.rasterToCamera.m.m, scene->camera->GetRasterToCameraMatrix().m, 4 * 4 * sizeof(float));
	memcpy(camera.cameraToWorld.m.m, scene->camera->GetCameraToWorldMatrix().m, 4 * 4 * sizeof(float));
}

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Geometry");

	const u_int verticesCount = scene->dataSet->GetTotalVertexCount();
	const u_int trianglesCount = scene->dataSet->GetTotalTriangleCount();
	const u_int objCount = scene->meshDefs.GetSize();
	std::vector<ExtMesh *> &objs = scene->meshDefs.GetAllMesh();

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

		delete[] meshFirstTriangleOffset;

		// This is a bit a trick, it does some assumption about how the merge
		// of Mesh works
		meshFirstTriangleOffset = new TriangleID[objCount];
		size_t currentIndex = 0;
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];
			meshFirstTriangleOffset[i] = currentIndex;
			currentIndex += mesh->GetTotalTriangleCount();
		}

		//----------------------------------------------------------------------
		// Translate geometry
		//----------------------------------------------------------------------

		std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

		luxrays::ocl::Mesh newMeshDesc;
		newMeshDesc.vertsOffset = 0;
		newMeshDesc.trisOffset = 0;
		memcpy(&newMeshDesc.trans.m, Matrix4x4().m, sizeof(float[4][4]));
		memcpy(&newMeshDesc.trans.mInv, Matrix4x4().m, sizeof(float[4][4]));

		luxrays::ocl::Mesh currentMeshDesc;

		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			bool isExistingInstance;
			if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
				ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

				// Check if is one of the already defined meshes
				std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
				if (it == definedMeshs.end()) {
					// It is a new one
					currentMeshDesc = newMeshDesc;

					newMeshDesc.vertsOffset += imesh->GetTotalVertexCount();
					newMeshDesc.trisOffset += imesh->GetTotalTriangleCount();

					isExistingInstance = false;

					const u_int index = meshDescs.size();
					definedMeshs[imesh->GetExtTriangleMesh()] = index;
				} else {
					currentMeshDesc = meshDescs[it->second];

					isExistingInstance = true;
				}

				memcpy(&currentMeshDesc.trans.m, &imesh->GetTransformation().m, sizeof(float[4][4]));
				memcpy(&currentMeshDesc.trans.mInv, &imesh->GetTransformation().mInv, sizeof(float[4][4]));
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

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetShadeNormal(j));

				//----------------------------------------------------------------------
				// Translate vertex uvs
				//----------------------------------------------------------------------

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
					if (mesh->HasUVs())
						uvs.push_back(mesh->GetUV(j));
					else
						uvs.push_back(UV(0.f, 0.f));
				}

				//--------------------------------------------------------------
				// Translate mesh vertices
				//--------------------------------------------------------------

				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					verts.push_back(mesh->GetVertex(j));

				//--------------------------------------------------------------
				// Translate mesh indices
				//--------------------------------------------------------------

				Triangle *mtris = mesh->GetTriangles();
				for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
					tris.push_back(mtris[j]);
			}
		}
	} else {
		meshFirstTriangleOffset = NULL;

		//----------------------------------------------------------------------
		// Translate mesh normals
		//----------------------------------------------------------------------

		normals.reserve(verticesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				normals.push_back(mesh->GetShadeNormal(j));
		}

		//----------------------------------------------------------------------
		// Translate vertex uvs
		//----------------------------------------------------------------------

		uvs.reserve(verticesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
				if (mesh->HasUVs())
					uvs.push_back(mesh->GetUV(j));
				else
					uvs.push_back(UV(0.f, 0.f));
			}
		}

		//----------------------------------------------------------------------
		// Translate mesh vertices
		//----------------------------------------------------------------------

		u_int *meshOffsets = new u_int[objCount];
		verts.reserve(verticesCount);
		u_int vIndex = 0;
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			meshOffsets[i] = vIndex;
			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts.push_back(mesh->GetVertex(j));

			vIndex += mesh->GetTotalVertexCount();
		}

		//----------------------------------------------------------------------
		// Translate mesh indices
		//----------------------------------------------------------------------

		tris.reserve(trianglesCount);
		for (u_int i = 0; i < objCount; ++i) {
			ExtMesh *mesh = objs[i];

			Triangle *mtris = mesh->GetTriangles();
			const u_int moffset = meshOffsets[i];
			for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j) {
				tris.push_back(Triangle(
						mtris[j].v[0] + moffset,
						mtris[j].v[1] + moffset,
						mtris[j].v[2] + moffset));
			}
		}
		delete[] meshOffsets;
	}

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileMaterials() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Materials");

	CompileTextures();

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedMaterialTypes.clear();

	const u_int materialsCount = scene->matDefs.GetSize();
	mats.resize(materialsCount);
	useBumpMapping = false;
	useNormalMapping = false;

	for (u_int i = 0; i < materialsCount; ++i) {
		Material *m = scene->matDefs.GetMaterial(i);
		luxrays::ocl::Material *mat = &mats[i];

		// Material emission
		const Texture *emitTex = m->GetEmitTexture();
		if (emitTex)
			mat->emitTexIndex = scene->texDefs.GetTextureIndex(emitTex);
		else
			mat->emitTexIndex = NULL_INDEX;

		// Material bump mapping
		const Texture *bumpTex = m->GetBumpTexture();
		if (bumpTex) {
			mat->bumpTexIndex = scene->texDefs.GetTextureIndex(bumpTex);
			useBumpMapping = true;
		} else
			mat->bumpTexIndex = NULL_INDEX;

		// Material normal mapping
		const Texture *normalTex = m->GetNormalTexture();
		if (normalTex) {
			mat->normalTexIndex = scene->texDefs.GetTextureIndex(normalTex);
			useNormalMapping = true;
		} else
			mat->normalTexIndex = NULL_INDEX;

		// Material specific parameters
		usedMaterialTypes.insert(m->GetType());
		switch (m->GetType()) {
			case MATTE: {
				MatteMaterial *mm = static_cast<MatteMaterial *>(m);

				mat->type = luxrays::ocl::MATTE;
				mat->matte.kdTexIndex = scene->texDefs.GetTextureIndex(mm->GetKd());
				break;
			}
			case MIRROR: {
				MirrorMaterial *mm = static_cast<MirrorMaterial *>(m);

				mat->type = luxrays::ocl::MIRROR;
				mat->mirror.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				break;
			}
			case GLASS: {
				GlassMaterial *gm = static_cast<GlassMaterial *>(m);

				mat->type = luxrays::ocl::GLASS;
				mat->glass.krTexIndex = scene->texDefs.GetTextureIndex(gm->GetKr());
				mat->glass.ktTexIndex = scene->texDefs.GetTextureIndex(gm->GetKt());
				mat->glass.ousideIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetOutsideIOR());
				mat->glass.iorTexIndex = scene->texDefs.GetTextureIndex(gm->GetIOR());
				break;
			}
			case METAL: {
				MetalMaterial *mm = static_cast<MetalMaterial *>(m);

				mat->type = luxrays::ocl::METAL;
				mat->metal.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				mat->metal.expTexIndex = scene->texDefs.GetTextureIndex(mm->GetExp());
				break;
			}
			case ARCHGLASS: {
				ArchGlassMaterial *am = static_cast<ArchGlassMaterial *>(m);

				mat->type = luxrays::ocl::ARCHGLASS;
				mat->archglass.krTexIndex = scene->texDefs.GetTextureIndex(am->GetKr());
				mat->archglass.ktTexIndex = scene->texDefs.GetTextureIndex(am->GetKt());
				break;
			}
			case MIX: {
				MixMaterial *mm = static_cast<MixMaterial *>(m);

				mat->type = luxrays::ocl::MIX;
				mat->mix.matAIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialA());
				mat->mix.matBIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialB());
				mat->mix.mixFactorTexIndex = scene->texDefs.GetTextureIndex(mm->GetMixFactor());
				break;
			}
			case NULLMAT: {
				mat->type = luxrays::ocl::NULLMAT;
				break;
			}
			case MATTETRANSLUCENT: {
				MatteTranslucentMaterial *mm = static_cast<MatteTranslucentMaterial *>(m);

				mat->type = luxrays::ocl::MATTETRANSLUCENT;
				mat->matteTranslucent.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				mat->matteTranslucent.ktTexIndex = scene->texDefs.GetTextureIndex(mm->GetKt());
				break;
			}
			case GLOSSY2: {
				Glossy2Material *g2m = static_cast<Glossy2Material *>(m);

				mat->type = luxrays::ocl::GLOSSY2;
				mat->glossy2.kdTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKd());
				mat->glossy2.ksTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKs());
				mat->glossy2.nuTexIndex = scene->texDefs.GetTextureIndex(g2m->GetNu());
				mat->glossy2.nvTexIndex = scene->texDefs.GetTextureIndex(g2m->GetNv());
				mat->glossy2.kaTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKa());
				mat->glossy2.depthTexIndex = scene->texDefs.GetTextureIndex(g2m->GetDepth());
				mat->glossy2.indexTexIndex = scene->texDefs.GetTextureIndex(g2m->GetIndex());
				mat->glossy2.multibounce = g2m->IsMultibounce() ? 1 : 0;
				break;
			}
			case METAL2: {
				Metal2Material *m2m = static_cast<Metal2Material *>(m);

				mat->type = luxrays::ocl::METAL2;
				mat->metal2.nTexIndex = scene->texDefs.GetTextureIndex(m2m->GetN());
				mat->metal2.kTexIndex = scene->texDefs.GetTextureIndex(m2m->GetK());
				mat->metal2.nuTexIndex = scene->texDefs.GetTextureIndex(m2m->GetNu());
				mat->metal2.nvTexIndex = scene->texDefs.GetTextureIndex(m2m->GetNv());
				break;
			}
			default:
				throw std::runtime_error("Unknown material: " + boost::lexical_cast<std::string>(m->GetType()));
				break;
		}
	}

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const u_int meshCount = scene->meshDefs.GetSize();
	meshMats.resize(meshCount);
	for (u_int i = 0; i < meshCount; ++i) {
		Material *m = scene->objectMaterials[i];
		meshMats[i] = scene->matDefs.GetMaterialIndex(m);
	}

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileAreaLights() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Triangle AreaLights");

	//--------------------------------------------------------------------------
	// Translate area lights
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	// Used to speedup the TriangleLight lookup
	std::map<const TriangleLight *, u_int> triLightByPtr;
	for (u_int i = 0; i < scene->triLightDefs.size(); ++i)
		triLightByPtr.insert(std::make_pair(scene->triLightDefs[i], i));

	const u_int areaLightCount = scene->triLightDefs.size();
	triLightDefs.resize(areaLightCount);
	if (areaLightCount > 0) {
		for (u_int i = 0; i < scene->triLightDefs.size(); ++i) {
			const TriangleLight *tl = scene->triLightDefs[i];
			const ExtMesh *mesh = tl->GetMesh();
			const Triangle *tri = &(mesh->GetTriangles()[tl->GetTriIndex()]);

			luxrays::ocl::TriangleLight *triLight = &triLightDefs[i];
			ASSIGN_VECTOR(triLight->v0, mesh->GetVertex(tri->v[0]));
			ASSIGN_VECTOR(triLight->v1, mesh->GetVertex(tri->v[1]));
			ASSIGN_VECTOR(triLight->v2, mesh->GetVertex(tri->v[2]));
			if (mesh->HasUVs()) {
				ASSIGN_UV(triLight->uv0, mesh->GetUV(tri->v[0]));
				ASSIGN_UV(triLight->uv1, mesh->GetUV(tri->v[1]));
				ASSIGN_UV(triLight->uv2, mesh->GetUV(tri->v[2]));
			} else {
				const UV zero;
				ASSIGN_UV(triLight->uv0, zero);
				ASSIGN_UV(triLight->uv1, zero);
				ASSIGN_UV(triLight->uv2, zero);
			}
			triLight->invArea = 1.f / tl->GetArea();

			triLight->materialIndex = scene->matDefs.GetMaterialIndex(tl->GetMaterial());
		}

		meshLights.resize(scene->triangleLights.size());
		for (u_int i = 0; i < scene->triangleLights.size(); ++i) {
			if (scene->triangleLights[i]) {
				// Look for the index
				meshLights[i] = triLightByPtr.find(scene->triangleLights[i])->second;
			} else
				meshLights[i] = NULL_INDEX;
		}
	} else
		meshLights.resize(0);

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Triangle area lights compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileInfiniteLight() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile InfiniteLight");

	delete infiniteLight;

	//--------------------------------------------------------------------------
	// Check if there is an infinite light source
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	InfiniteLight *il = (InfiniteLight *)scene->GetLightByType(TYPE_IL);
	if (il) {
		infiniteLight = new luxrays::ocl::InfiniteLight();

		ASSIGN_SPECTRUM(infiniteLight->gain, il->GetGain());
		infiniteLight->shiftU = il->GetShiftU();
		infiniteLight->shiftV = il->GetShiftV();

		const ImageMapInstance *im = il->GetImageMapInstance();
		infiniteLight->imageMapInstance.gain = im->GetGain();
		infiniteLight->imageMapInstance.uScale = im->GetUScale();
		infiniteLight->imageMapInstance.vScale = im->GetVScale();
		infiniteLight->imageMapInstance.uDelta = im->GetUDelta();
		infiniteLight->imageMapInstance.vDelta = im->GetVDelta();
		infiniteLight->imageMapInstance.Du = im->GetDuDv().u;
		infiniteLight->imageMapInstance.Dv = im->GetDuDv().v;
		infiniteLight->imageMapInstance.imageMapIndex = scene->imgMapCache.GetImageMapIndex(im->GetImgMap());
	} else
		infiniteLight = NULL;

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Infinitelight compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileSunLight() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile SunLight");

	delete sunLight;

	//--------------------------------------------------------------------------
	// Check if there is an sun light source
	//--------------------------------------------------------------------------

	SunLight *sl = (SunLight *)scene->GetLightByType(TYPE_SUN);
	if (sl) {
		sunLight = new luxrays::ocl::SunLight();

		ASSIGN_VECTOR(sunLight->sunDir, sl->GetDir());
		ASSIGN_SPECTRUM(sunLight->gain, sl->GetGain());
		sunLight->turbidity = sl->GetTubidity();
		sunLight->relSize= sl->GetRelSize();
		float tmp;
		sl->GetInitData(reinterpret_cast<Vector *>(&sunLight->x),
				reinterpret_cast<Vector *>(&sunLight->y), &tmp, &tmp, &tmp,
				&sunLight->cosThetaMax, &tmp, reinterpret_cast<Spectrum *>(&sunLight->sunColor));
	} else
		sunLight = NULL;
}

void CompiledScene::CompileSkyLight() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile SkyLight");

	delete skyLight;

	//--------------------------------------------------------------------------
	// Check if there is an sky light source
	//--------------------------------------------------------------------------

	SkyLight *sl = (SkyLight *)scene->GetLightByType(TYPE_IL_SKY);
	if (sl) {
		skyLight = new luxrays::ocl::SkyLight();

		ASSIGN_SPECTRUM(skyLight->gain, sl->GetGain());
		sl->GetInitData(&skyLight->thetaS, &skyLight->phiS,
				&skyLight->zenith_Y, &skyLight->zenith_x, &skyLight->zenith_y,
				skyLight->perez_Y, skyLight->perez_x, skyLight->perez_y);
	} else
		skyLight = NULL;
}

void CompiledScene::CompileTextures() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Textures");

	//--------------------------------------------------------------------------
	// Translate textures
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedTextureTypes.clear();

	const u_int texturesCount = scene->texDefs.GetSize();
	texs.resize(texturesCount);

	for (u_int i = 0; i < texturesCount; ++i) {
		Texture *t = scene->texDefs.GetTexture(i);
		luxrays::ocl::Texture *tex = &texs[i];

		usedTextureTypes.insert(t->GetType());
		switch (t->GetType()) {
			case CONST_FLOAT: {
				ConstFloatTexture *cft = static_cast<ConstFloatTexture *>(t);

				tex->type = luxrays::ocl::CONST_FLOAT;
				tex->constFloat.value = cft->GetValue();
				break;
			}
			case CONST_FLOAT3: {
				ConstFloat3Texture *cft = static_cast<ConstFloat3Texture *>(t);

				tex->type = luxrays::ocl::CONST_FLOAT3;
				ASSIGN_SPECTRUM(tex->constFloat3.color, cft->GetColor());
				break;
			}
			case CONST_FLOAT4: {
				ConstFloat4Texture *cft = static_cast<ConstFloat4Texture *>(t);

				tex->type = luxrays::ocl::CONST_FLOAT4;
				ASSIGN_SPECTRUM(tex->constFloat4.color, cft->GetColor());
				tex->constFloat4.alpha = cft->GetAlpha();
				break;
			}
			case IMAGEMAP: {
				ImageMapTexture *imt = static_cast<ImageMapTexture *>(t);

				tex->type = luxrays::ocl::IMAGEMAP;
				tex->imageMapInstance.gain = imt->GetImageMapInstance()->GetGain();
				tex->imageMapInstance.uScale = imt->GetImageMapInstance()->GetUScale();
				tex->imageMapInstance.vScale = imt->GetImageMapInstance()->GetVScale();
				tex->imageMapInstance.uDelta = imt->GetImageMapInstance()->GetUDelta();
				tex->imageMapInstance.vDelta = imt->GetImageMapInstance()->GetVDelta();
				tex->imageMapInstance.Du = imt->GetImageMapInstance()->GetDuDv().u;
				tex->imageMapInstance.Dv = imt->GetImageMapInstance()->GetDuDv().v;
				tex->imageMapInstance.imageMapIndex = scene->imgMapCache.GetImageMapIndex(imt->GetImageMapInstance()->GetImgMap());
				break;
			}
			case SCALE_TEX: {
				ScaleTexture *st = static_cast<ScaleTexture *>(t);

				tex->type = luxrays::ocl::SCALE_TEX;
				const Texture *tex1 = st->GetTexture1();
				if (dynamic_cast<const ScaleTexture *>(tex1) ||
						dynamic_cast<const FresnelApproxNTexture *>(tex1) ||
						dynamic_cast<const FresnelApproxKTexture *>(tex1))
					throw std::runtime_error("Recursive scale texture is not supported");
				tex->scaleTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				if (dynamic_cast<const ScaleTexture *>(tex2))
					throw std::runtime_error("Recursive scale texture is not supported");
				tex->scaleTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case FRESNEL_APPROX_N: {
				FresnelApproxNTexture *ft = static_cast<FresnelApproxNTexture *>(t);

				tex->type = luxrays::ocl::FRESNEL_APPROX_N;
				const Texture *tx = ft->GetTexture();
				if (dynamic_cast<const ScaleTexture *>(tx) ||
						dynamic_cast<const FresnelApproxNTexture *>(tx) ||
						dynamic_cast<const FresnelApproxKTexture *>(tx))
					throw std::runtime_error("Recursive fresnel texture is not supported");
				tex->fresnelApproxN.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case FRESNEL_APPROX_K: {
				FresnelApproxKTexture *ft = static_cast<FresnelApproxKTexture *>(t);

				tex->type = luxrays::ocl::FRESNEL_APPROX_K;
				const Texture *tx = ft->GetTexture();
				if (dynamic_cast<const ScaleTexture *>(tx) ||
						dynamic_cast<const FresnelApproxNTexture *>(tx) ||
						dynamic_cast<const FresnelApproxKTexture *>(tx))
					throw std::runtime_error("Recursive fresnel texture is not supported");
				tex->fresnelApproxK.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			default:
				throw std::runtime_error("Unknown texture: " + boost::lexical_cast<std::string>(t->GetType()));
				break;
		}
	}
		
	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Textures compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileImageMaps() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile ImageMaps");

	imageMapDescs.resize(0);
	imageMapMemBlocks.resize(0);

	//--------------------------------------------------------------------------
	// Translate image maps
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	std::vector<ImageMap *> ims;
	scene->imgMapCache.GetImageMaps(ims);

	imageMapDescs.resize(ims.size());
	for (u_int i = 0; i < ims.size(); ++i) {
		const ImageMap *im = ims[i];
		luxrays::ocl::ImageMap *imd = &imageMapDescs[i];

		const u_int pixelCount = im->GetWidth() * im->GetHeight();
		const u_int memSize = pixelCount * im->GetChannelCount() * sizeof(float);

		if (memSize > maxMemPageSize)
			throw std::runtime_error("An image map is too big to fit in a single block of memory");

		bool found = false;
		u_int page;
		for (u_int j = 0; j < imageMapMemBlocks.size(); ++j) {
			// Check if it fits in the this page
			if (memSize + imageMapMemBlocks[j].size() * sizeof(float) <= maxMemPageSize) {
				found = true;
				page = j;
				break;
			}
		}

		if (!found) {
			// Check if I can add a new page
			if (imageMapMemBlocks.size() > 5)
				throw std::runtime_error("More than 5 blocks of memory are required for image maps");

			// Add a new page
			imageMapMemBlocks.push_back(vector<float>());
			page = imageMapMemBlocks.size() - 1;
		}

		imd->width = im->GetWidth();
		imd->height = im->GetHeight();
		imd->channelCount = im->GetChannelCount();
		imd->pageIndex = page;
		imd->pixelsIndex = (u_int)imageMapMemBlocks[page].size();
		imageMapMemBlocks[page].insert(imageMapMemBlocks[page].end(), im->GetPixels(),
				im->GetPixels() + pixelCount * im->GetChannelCount());
	}

	SLG_LOG("[PathOCLRenderThread::CompiledScene] Image maps page count: " << imageMapMemBlocks.size());
	for (u_int i = 0; i < imageMapMemBlocks.size(); ++i)
		SLG_LOG("[PathOCLRenderThread::CompiledScene]  RGB channel page " << i << " size: " << imageMapMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Texture maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
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
	if (editActions.Has(IMAGEMAPS_EDIT))
		CompileImageMaps();
}

bool CompiledScene::IsMaterialCompiled(const MaterialType type) const {
	return (usedMaterialTypes.find(type) != usedMaterialTypes.end());
}

bool CompiledScene::IsTextureCompiled(const TextureType type) const {
	return (usedTextureTypes.find(type) != usedTextureTypes.end());
}

}

#endif
