/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
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

#include <limits>

#include <boost/lexical_cast.hpp>

#include"slg/engines/pathocl/compiledscene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

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
	//SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Camera");

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

	const u_int objCount = scene->meshDefs.GetSize();
	std::vector<ExtMesh *> &objs = scene->meshDefs.GetAllMesh();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	uvs.resize(0);
	cols.resize(0);
	alphas.resize(0);
	tris.resize(0);
	meshDescs.resize(0);

	meshIDs = scene->dataSet->GetMeshIDTable();

	//----------------------------------------------------------------------
	// Translate geometry
	//----------------------------------------------------------------------

	std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

	slg::ocl::Mesh newMeshDesc;
	newMeshDesc.vertsOffset = 0;
	newMeshDesc.trisOffset = 0;
	newMeshDesc.normalsOffset = 0;
	newMeshDesc.uvsOffset = 0;
	newMeshDesc.colsOffset = 0;
	newMeshDesc.alphasOffset = 0;
	newMeshDesc.firstTriangleOffset = 0;
	memcpy(&newMeshDesc.trans.m, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));
	memcpy(&newMeshDesc.trans.mInv, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));

	slg::ocl::Mesh currentMeshDesc;
	u_int firstTriangleOffset = 0;
	for (u_int i = 0; i < objCount; ++i) {
		ExtMesh *mesh = objs[i];

		bool isExistingInstance;
		if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
			// It is a instanced mesh
			ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

			// Check if is one of the already defined meshes
			std::map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
			if (it == definedMeshs.end()) {
				// It is a new one
				currentMeshDesc = newMeshDesc;

				newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
				newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();
				if (mesh->HasNormals())
					newMeshDesc.normalsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasUVs())
					newMeshDesc.uvsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasColors())
					newMeshDesc.colsOffset += mesh->GetTotalVertexCount();
				if (mesh->HasAlphas())
					newMeshDesc.alphasOffset += mesh->GetTotalVertexCount();

				isExistingInstance = false;

				const u_int index = meshDescs.size();
				definedMeshs[imesh->GetExtTriangleMesh()] = index;
			} else {
				currentMeshDesc = meshDescs[it->second];

				isExistingInstance = true;
			}

			// Overwrite the only different fields in an instanced mesh
			memcpy(&currentMeshDesc.trans.m, &imesh->GetTransformation().m, sizeof(float[4][4]));
			memcpy(&currentMeshDesc.trans.mInv, &imesh->GetTransformation().mInv, sizeof(float[4][4]));

			mesh = imesh->GetExtTriangleMesh();
		} else {
			// It is a not instanced mesh
			currentMeshDesc = newMeshDesc;

			newMeshDesc.vertsOffset += mesh->GetTotalVertexCount();
			newMeshDesc.trisOffset += mesh->GetTotalTriangleCount();
			if (mesh->HasNormals())
				newMeshDesc.normalsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasUVs())
				newMeshDesc.uvsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasColors())
				newMeshDesc.colsOffset += mesh->GetTotalVertexCount();
			if (mesh->HasAlphas())
				newMeshDesc.alphasOffset += mesh->GetTotalVertexCount();

			memcpy(&currentMeshDesc.trans.m, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));
			memcpy(&currentMeshDesc.trans.mInv, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));

			isExistingInstance = false;
		}

		currentMeshDesc.firstTriangleOffset = firstTriangleOffset;
		firstTriangleOffset += mesh->GetTotalTriangleCount();

		if (!isExistingInstance) {
			//------------------------------------------------------------------
			// Translate mesh normals
			//------------------------------------------------------------------

			if (mesh->HasNormals()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					normals.push_back(mesh->GetShadeNormal(j));
			} else
				currentMeshDesc.normalsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex uvs
			//------------------------------------------------------------------

			if (mesh->HasUVs()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					uvs.push_back(mesh->GetUV(j));
			} else
				currentMeshDesc.uvsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex colors
			//------------------------------------------------------------------

			if (mesh->HasColors()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					cols.push_back(mesh->GetColor(j));
			} else
				currentMeshDesc.colsOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate vertex alphas
			//------------------------------------------------------------------

			if (mesh->HasAlphas()) {
				for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
					alphas.push_back(mesh->GetAlpha(j));
			} else
				currentMeshDesc.alphasOffset = NULL_INDEX;

			//------------------------------------------------------------------
			// Translate mesh vertices
			//------------------------------------------------------------------

			for (u_int j = 0; j < mesh->GetTotalVertexCount(); ++j)
				verts.push_back(mesh->GetVertex(j));

			//------------------------------------------------------------------
			// Translate mesh indices
			//------------------------------------------------------------------

			Triangle *mtris = mesh->GetTriangles();
			for (u_int j = 0; j < mesh->GetTotalTriangleCount(); ++j)
				tris.push_back(mtris[j]);
		}

		meshDescs.push_back(currentMeshDesc);
	}

	const double tEnd = WallClockTime();
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

static bool IsTexConstant(const Texture *tex) {
	return (dynamic_cast<const ConstFloatTexture *>(tex)) ||
			(dynamic_cast<const ConstFloat3Texture *>(tex));
}

static float GetTexConstantFloatValue(const Texture *tex) {
	// Check if a texture is constant and return the value
	const ConstFloatTexture *cft = dynamic_cast<const ConstFloatTexture *>(tex);
	if (cft)
		return cft->GetValue();
	const ConstFloat3Texture *cf3t = dynamic_cast<const ConstFloat3Texture *>(tex);
	if (cf3t)
		return cf3t->GetColor().Y();

	return std::numeric_limits<float>::infinity();
}

//static Spectrum GetTexConstantFloat3Value(const Texture *tex) {
//	// Check if a texture is constant and return the value
//	const ConstFloatTexture *cft = dynamic_cast<const ConstFloatTexture *>(tex);
//	if (cft)
//		return Spectrum(cft->GetValue());
//	const ConstFloat3Texture *cf3t = dynamic_cast<const ConstFloat3Texture *>(tex);
//	if (cf3t)
//		return cf3t->GetColor();
//
//	return std::numeric_limits<float>::infinity();
//}

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
		slg::ocl::Material *mat = &mats[i];
		//SLG_LOG("[PathOCLRenderThread::CompiledScene]  Type: " << m->GetType());

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

				mat->type = slg::ocl::MATTE;
				mat->matte.kdTexIndex = scene->texDefs.GetTextureIndex(mm->GetKd());
				break;
			}
			case MIRROR: {
				MirrorMaterial *mm = static_cast<MirrorMaterial *>(m);

				mat->type = slg::ocl::MIRROR;
				mat->mirror.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				break;
			}
			case GLASS: {
				GlassMaterial *gm = static_cast<GlassMaterial *>(m);

				mat->type = slg::ocl::GLASS;
				mat->glass.krTexIndex = scene->texDefs.GetTextureIndex(gm->GetKr());
				mat->glass.ktTexIndex = scene->texDefs.GetTextureIndex(gm->GetKt());
				mat->glass.ousideIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetOutsideIOR());
				mat->glass.iorTexIndex = scene->texDefs.GetTextureIndex(gm->GetIOR());
				break;
			}
			case METAL: {
				MetalMaterial *mm = static_cast<MetalMaterial *>(m);

				mat->type = slg::ocl::METAL;
				mat->metal.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				mat->metal.expTexIndex = scene->texDefs.GetTextureIndex(mm->GetExp());
				break;
			}
			case ARCHGLASS: {
				ArchGlassMaterial *am = static_cast<ArchGlassMaterial *>(m);

				mat->type = slg::ocl::ARCHGLASS;
				mat->archglass.krTexIndex = scene->texDefs.GetTextureIndex(am->GetKr());
				mat->archglass.ktTexIndex = scene->texDefs.GetTextureIndex(am->GetKt());
				mat->archglass.ousideIorTexIndex = scene->texDefs.GetTextureIndex(am->GetOutsideIOR());
				mat->archglass.iorTexIndex = scene->texDefs.GetTextureIndex(am->GetIOR());
				break;
			}
			case MIX: {
				MixMaterial *mm = static_cast<MixMaterial *>(m);

				mat->type = slg::ocl::MIX;
				mat->mix.matAIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialA());
				mat->mix.matBIndex = scene->matDefs.GetMaterialIndex(mm->GetMaterialB());
				mat->mix.mixFactorTexIndex = scene->texDefs.GetTextureIndex(mm->GetMixFactor());
				break;
			}
			case NULLMAT: {
				mat->type = slg::ocl::NULLMAT;
				break;
			}
			case MATTETRANSLUCENT: {
				MatteTranslucentMaterial *mm = static_cast<MatteTranslucentMaterial *>(m);

				mat->type = slg::ocl::MATTETRANSLUCENT;
				mat->matteTranslucent.krTexIndex = scene->texDefs.GetTextureIndex(mm->GetKr());
				mat->matteTranslucent.ktTexIndex = scene->texDefs.GetTextureIndex(mm->GetKt());
				break;
			}
			case GLOSSY2: {
				Glossy2Material *g2m = static_cast<Glossy2Material *>(m);

				mat->type = slg::ocl::GLOSSY2;
				mat->glossy2.kdTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKd());
				mat->glossy2.ksTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKs());

				const Texture *nuTex = g2m->GetNu();
				const Texture *nvTex = g2m->GetNv();
				mat->glossy2.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->glossy2.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(GLOSSY2_ANISOTROPIC);

				const Texture *depthTex = g2m->GetDepth();
				mat->glossy2.kaTexIndex = scene->texDefs.GetTextureIndex(g2m->GetKa());
				mat->glossy2.depthTexIndex = scene->texDefs.GetTextureIndex(depthTex);
				// Check if depth is just 0.0
				if (IsTexConstant(depthTex) && (GetTexConstantFloatValue(depthTex) > 0.f))
					usedMaterialTypes.insert(GLOSSY2_ABSORPTION);

				const Texture *indexTex = g2m->GetIndex();
				mat->glossy2.indexTexIndex = scene->texDefs.GetTextureIndex(indexTex);
				// Check if index is just 0.0
				if (IsTexConstant(depthTex) && (GetTexConstantFloatValue(indexTex) > 0.f))
					usedMaterialTypes.insert(GLOSSY2_INDEX);

				mat->glossy2.multibounce = g2m->IsMultibounce() ? 1 : 0;
				// Check if multibounce is enabled
				if (g2m->IsMultibounce())
					usedMaterialTypes.insert(GLOSSY2_MULTIBOUNCE);
				break;
			}
			case METAL2: {
				Metal2Material *m2m = static_cast<Metal2Material *>(m);

				mat->type = slg::ocl::METAL2;
				mat->metal2.nTexIndex = scene->texDefs.GetTextureIndex(m2m->GetN());
				mat->metal2.kTexIndex = scene->texDefs.GetTextureIndex(m2m->GetK());

				const Texture *nuTex = m2m->GetNu();
				const Texture *nvTex = m2m->GetNv();
				mat->metal2.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->metal2.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(METAL2_ANISOTROPIC);
				break;
			}
			case ROUGHGLASS: {
				RoughGlassMaterial *rgm = static_cast<RoughGlassMaterial *>(m);

				mat->type = slg::ocl::ROUGHGLASS;
				mat->roughglass.krTexIndex = scene->texDefs.GetTextureIndex(rgm->GetKr());
				mat->roughglass.ktTexIndex = scene->texDefs.GetTextureIndex(rgm->GetKt());
				mat->roughglass.ousideIorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetOutsideIOR());
				mat->roughglass.iorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetIOR());

				const Texture *nuTex = rgm->GetNu();
				const Texture *nvTex = rgm->GetNv();
				mat->roughglass.nuTexIndex = scene->texDefs.GetTextureIndex(nuTex);
				mat->roughglass.nvTexIndex = scene->texDefs.GetTextureIndex(nvTex);
				// Check if it an anisotropic material
				if (IsTexConstant(nuTex) && IsTexConstant(nvTex) &&
						(GetTexConstantFloatValue(nuTex) != GetTexConstantFloatValue(nvTex)))
					usedMaterialTypes.insert(ROUGHGLASS_ANISOTROPIC);
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

			slg::ocl::TriangleLight *triLight = &triLightDefs[i];
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

void CompiledScene::CompileTextureMapping2D(slg::ocl::TextureMapping2D *mapping, const TextureMapping2D *m) {
	switch (m->GetType()) {
		case UVMAPPING2D: {
			mapping->type = slg::ocl::UVMAPPING2D;
			const UVMapping2D *uvm = static_cast<const UVMapping2D *>(m);
			mapping->uvMapping2D.uScale = uvm->uScale;
			mapping->uvMapping2D.vScale = uvm->vScale;
			mapping->uvMapping2D.uDelta = uvm->uDelta;
			mapping->uvMapping2D.vDelta = uvm->vDelta;
			break;
		}
		default:
			throw std::runtime_error("Unknown 2D texture mapping: " + boost::lexical_cast<std::string>(m->GetType()));
	}
}

void CompiledScene::CompileTextureMapping3D(slg::ocl::TextureMapping3D *mapping, const TextureMapping3D *m) {
	switch (m->GetType()) {
		case UVMAPPING3D: {
			mapping->type = slg::ocl::UVMAPPING3D;
			const UVMapping3D *uvm = static_cast<const UVMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &uvm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &uvm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		case GLOBALMAPPING3D: {
			mapping->type = slg::ocl::GLOBALMAPPING3D;
			const GlobalMapping3D *gm = static_cast<const GlobalMapping3D *>(m);
			memcpy(&mapping->worldToLocal.m, &gm->worldToLocal.m, sizeof(float[4][4]));
			memcpy(&mapping->worldToLocal.mInv, &gm->worldToLocal.mInv, sizeof(float[4][4]));
			break;
		}
		default:
			throw std::runtime_error("Unknown texture mapping: " + boost::lexical_cast<std::string>(m->GetType()));
	}
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
		infiniteLight = new slg::ocl::InfiniteLight();

		ASSIGN_SPECTRUM(infiniteLight->gain, il->GetGain());
		CompileTextureMapping2D(&infiniteLight->mapping, il->GetUVMapping());
		infiniteLight->imageMapIndex = scene->imgMapCache.GetImageMapIndex(il->GetImageMap());

		memcpy(&infiniteLight->light2World.m, &il->GetTransformation().m, sizeof(float[4][4]));
		memcpy(&infiniteLight->light2World.mInv, &il->GetTransformation().mInv, sizeof(float[4][4]));
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
		sunLight = new slg::ocl::SunLight();

		const Vector globalSunDir = Normalize(sl->GetTransformation() * sl->GetDir());
		ASSIGN_VECTOR(sunLight->sunDir, globalSunDir);
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
		skyLight = new slg::ocl::SkyLight();

		ASSIGN_SPECTRUM(skyLight->gain, sl->GetGain());
		sl->GetInitData(&skyLight->thetaS, &skyLight->phiS,
				&skyLight->zenith_Y, &skyLight->zenith_x, &skyLight->zenith_y,
				skyLight->perez_Y, skyLight->perez_x, skyLight->perez_y);

		memcpy(&skyLight->light2World.m, &sl->GetTransformation().m, sizeof(float[4][4]));
		memcpy(&skyLight->light2World.mInv, &sl->GetTransformation().mInv, sizeof(float[4][4]));
	} else
		skyLight = NULL;
}

void CompiledScene::CompileTextures() {
	SLG_LOG("[PathOCLRenderThread::CompiledScene] Compile Textures");
	SLG_LOG("[PathOCLRenderThread::CompiledScene]   Texture size: " << sizeof(slg::ocl::Texture));

	//--------------------------------------------------------------------------
	// Translate textures
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedTextureTypes.clear();

	const u_int texturesCount = scene->texDefs.GetSize();
	texs.resize(texturesCount);

	for (u_int i = 0; i < texturesCount; ++i) {
		Texture *t = scene->texDefs.GetTexture(i);
		slg::ocl::Texture *tex = &texs[i];

		usedTextureTypes.insert(t->GetType());
		switch (t->GetType()) {
			case CONST_FLOAT: {
				ConstFloatTexture *cft = static_cast<ConstFloatTexture *>(t);

				tex->type = slg::ocl::CONST_FLOAT;
				tex->constFloat.value = cft->GetValue();
				break;
			}
			case CONST_FLOAT3: {
				ConstFloat3Texture *cft = static_cast<ConstFloat3Texture *>(t);

				tex->type = slg::ocl::CONST_FLOAT3;
				ASSIGN_SPECTRUM(tex->constFloat3.color, cft->GetColor());
				break;
			}
			case IMAGEMAP: {
				ImageMapTexture *imt = static_cast<ImageMapTexture *>(t);

				tex->type = slg::ocl::IMAGEMAP;
				const ImageMap *im = imt->GetImageMap();
				tex->imageMapTex.gain = imt->GetGain();
				CompileTextureMapping2D(&tex->imageMapTex.mapping, imt->GetTextureMapping());
				tex->imageMapTex.Du = imt->GetDuDv().u;
				tex->imageMapTex.Dv = imt->GetDuDv().v;
				tex->imageMapTex.imageMapIndex = scene->imgMapCache.GetImageMapIndex(im);
				break;
			}
			case SCALE_TEX: {
				ScaleTexture *st = static_cast<ScaleTexture *>(t);

				tex->type = slg::ocl::SCALE_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->scaleTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->scaleTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case FRESNEL_APPROX_N: {
				FresnelApproxNTexture *ft = static_cast<FresnelApproxNTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_N;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxN.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case FRESNEL_APPROX_K: {
				FresnelApproxKTexture *ft = static_cast<FresnelApproxKTexture *>(t);

				tex->type = slg::ocl::FRESNEL_APPROX_K;
				const Texture *tx = ft->GetTexture();
				tex->fresnelApproxK.texIndex = scene->texDefs.GetTextureIndex(tx);
				break;
			}
			case CHECKERBOARD2D: {
				CheckerBoard2DTexture *cb = static_cast<CheckerBoard2DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD2D;
				CompileTextureMapping2D(&tex->checkerBoard2D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard2D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard2D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case CHECKERBOARD3D: {
				CheckerBoard3DTexture *cb = static_cast<CheckerBoard3DTexture *>(t);

				tex->type = slg::ocl::CHECKERBOARD3D;
				CompileTextureMapping3D(&tex->checkerBoard3D.mapping, cb->GetTextureMapping());
				const Texture *tex1 = cb->GetTexture1();
				tex->checkerBoard3D.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = cb->GetTexture2();
				tex->checkerBoard3D.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case MIX_TEX: {
				MixTexture *mt = static_cast<MixTexture *>(t);

				tex->type = slg::ocl::MIX_TEX;
				const Texture *amount = mt->GetAmountTexture();
				tex->mixTex.amountTexIndex = scene->texDefs.GetTextureIndex(amount);

				const Texture *tex1 = mt->GetTexture1();
				tex->mixTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);
				const Texture *tex2 = mt->GetTexture2();
				tex->mixTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case FBM_TEX: {
				FBMTexture *ft = static_cast<FBMTexture *>(t);

				tex->type = slg::ocl::FBM_TEX;
				CompileTextureMapping3D(&tex->fbm.mapping, ft->GetTextureMapping());
				tex->fbm.octaves = ft->GetOctaves();
				tex->fbm.omega = ft->GetOmega();
				break;
			}
			case MARBLE: {
				MarbleTexture *mt = static_cast<MarbleTexture *>(t);

				tex->type = slg::ocl::MARBLE;
				CompileTextureMapping3D(&tex->fbm.mapping, mt->GetTextureMapping());
				tex->marble.octaves = mt->GetOctaves();
				tex->marble.omega = mt->GetOmega();
				tex->marble.scale = mt->GetScale();
				tex->marble.variation = mt->GetVariation();
				break;
			}
			case DOTS: {
				DotsTexture *dt = static_cast<DotsTexture *>(t);

				tex->type = slg::ocl::DOTS;
				CompileTextureMapping2D(&tex->dots.mapping, dt->GetTextureMapping());
				const Texture *insideTex = dt->GetInsideTex();
				tex->dots.insideIndex = scene->texDefs.GetTextureIndex(insideTex);

				const Texture *outsideTex = dt->GetOutsideTex();
				tex->dots.outsideIndex = scene->texDefs.GetTextureIndex(outsideTex);
				break;
			}
			case BRICK: {
				BrickTexture *bt = static_cast<BrickTexture *>(t);

				tex->type = slg::ocl::BRICK;
				CompileTextureMapping3D(&tex->brick.mapping, bt->GetTextureMapping());
				const Texture *tex1 = bt->GetTexture1();
				tex->brick.tex1Index = scene->texDefs.GetTextureIndex(tex1);
				const Texture *tex2 = bt->GetTexture2();
				tex->brick.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				const Texture *tex3 = bt->GetTexture3();
				tex->brick.tex3Index = scene->texDefs.GetTextureIndex(tex3);

				switch (bt->GetBond()) {
					case FLEMISH:
						tex->brick.bond = slg::ocl::FLEMISH;
						break;
					default:
					case RUNNING:
						tex->brick.bond = slg::ocl::RUNNING;
						break;
					case ENGLISH:
						tex->brick.bond = slg::ocl::ENGLISH;
						break;
					case HERRINGBONE:
						tex->brick.bond = slg::ocl::HERRINGBONE;
						break;
					case BASKET:
						tex->brick.bond = slg::ocl::BASKET;
						break;
					case KETTING:
						tex->brick.bond = slg::ocl::KETTING;
						break; 
				}

				tex->brick.offsetx = bt->GetOffset().x;
				tex->brick.offsety = bt->GetOffset().y;
				tex->brick.offsetz = bt->GetOffset().z;
				tex->brick.brickwidth = bt->GetBrickWidth();
				tex->brick.brickheight = bt->GetBrickHeight();
				tex->brick.brickdepth = bt->GetBrickDepth();
				tex->brick.mortarsize = bt->GetMortarSize();
				tex->brick.proportion = bt->GetProportion();
				tex->brick.invproportion = bt->GetInvProportion();
				tex->brick.run = bt->GetRun();
				tex->brick.mortarwidth = bt->GetMortarWidth();
				tex->brick.mortarheight = bt->GetMortarHeight();
				tex->brick.mortardepth = bt->GetMortarDepth();
				tex->brick.bevelwidth = bt->GetBevelWidth();
				tex->brick.bevelheight = bt->GetBevelHeight();
				tex->brick.beveldepth = bt->GetBevelDepth();
				tex->brick.usebevel = bt->GetUseBevel();
				break;
			}
			case ADD_TEX: {
				ScaleTexture *st = static_cast<ScaleTexture *>(t);

				tex->type = slg::ocl::ADD_TEX;
				const Texture *tex1 = st->GetTexture1();
				tex->addTex.tex1Index = scene->texDefs.GetTextureIndex(tex1);

				const Texture *tex2 = st->GetTexture2();
				tex->addTex.tex2Index = scene->texDefs.GetTextureIndex(tex2);
				break;
			}
			case WINDY: {
				WindyTexture *wt = static_cast<WindyTexture *>(t);

				tex->type = slg::ocl::WINDY;
				CompileTextureMapping3D(&tex->windy.mapping, wt->GetTextureMapping());
				break;
			}
			case WRINKLED: {
				WrinkledTexture *wt = static_cast<WrinkledTexture *>(t);

				tex->type = slg::ocl::WRINKLED;
				CompileTextureMapping3D(&tex->wrinkled.mapping, wt->GetTextureMapping());
				tex->wrinkled.octaves = wt->GetOctaves();
				tex->wrinkled.omega = wt->GetOmega();
				break;
			}
			case UV_TEX: {
				UVTexture *uvt = static_cast<UVTexture *>(t);

				tex->type = slg::ocl::UV_TEX;
				CompileTextureMapping2D(&tex->uvTex.mapping, uvt->GetTextureMapping());
				break;
			}
			case BAND_TEX: {
				BandTexture *bt = static_cast<BandTexture *>(t);

				tex->type = slg::ocl::BAND_TEX;
				const Texture *amount = bt->GetAmountTexture();
				tex->band.amountTexIndex = scene->texDefs.GetTextureIndex(amount);

				const std::vector<float> &offsets = bt->GetOffsets();
				const std::vector<Spectrum> &values = bt->GetValues();
				if (offsets.size() > BAND_TEX_MAX_SIZE)
					throw std::runtime_error("BandTexture with more than " + ToString(BAND_TEX_MAX_SIZE) + " are not supported");
				tex->band.size = offsets.size();
				for (u_int i = 0; i < BAND_TEX_MAX_SIZE; ++i) {
					if (i < offsets.size()) {
						tex->band.offsets[i] = offsets[i];
						ASSIGN_SPECTRUM(tex->band.values[i], values[i]);
					} else {
						tex->band.offsets[i] = 1.f;
						tex->band.values[i].r = 0.f;
						tex->band.values[i].g = 0.f;
						tex->band.values[i].b = 0.f;
					}
				}
				break;
			}
			case HITPOINTCOLOR: {
				tex->type = slg::ocl::HITPOINTCOLOR;
				break;
			}
			case HITPOINTALPHA: {
				tex->type = slg::ocl::HITPOINTALPHA;
				break;
			}
			case HITPOINTGREY: {
				HitPointGreyTexture *hpg = static_cast<HitPointGreyTexture *>(t);

				tex->type = slg::ocl::HITPOINTGREY;
				tex->hitPointGrey.channel = hpg->GetChannel();
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
		slg::ocl::ImageMap *imd = &imageMapDescs[i];

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

#endif
