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

#if !defined(LUXRAYS_DISABLE_OPENCL)

#include <limits>

#include <boost/lexical_cast.hpp>

#include"slg/engines/pathoclbase/compiledscene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

CompiledScene::CompiledScene(Scene *scn, Film *flm, const size_t maxMemPageS) {
	scene = scn;
	film = flm;
	maxMemPageSize = (u_int)Min<size_t>(maxMemPageS, 0xffffffffu);

	lightsDistribution = NULL;

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
	delete[] lightsDistribution;
}

void CompiledScene::CompileCamera() {
	//SLG_LOG("Compile Camera");

	//--------------------------------------------------------------------------
	// Camera definition
	//--------------------------------------------------------------------------

	switch (scene->camera->GetType()) {
		case Camera::PERSPECTIVE: {
			const PerspectiveCamera *perspCamera = (PerspectiveCamera *)scene->camera;
			camera.yon = perspCamera->clipYon;
			camera.hither = perspCamera->clipHither;
			camera.lensRadius = perspCamera->lensRadius;
			camera.focalDistance = perspCamera->focalDistance;

			memcpy(camera.rasterToCamera[0].m.m, perspCamera->GetRasterToCameraMatrix(0).m, 4 * 4 * sizeof(float));
			memcpy(camera.cameraToWorld[0].m.m, perspCamera->GetCameraToWorldMatrix(0).m, 4 * 4 * sizeof(float));

			if (perspCamera->IsHorizontalStereoEnabled()) {
				enableCameraHorizStereo = true;
				enableOculusRiftBarrel = perspCamera->IsOculusRiftBarrelEnabled();

				memcpy(camera.rasterToCamera[1].m.m, perspCamera->GetRasterToCameraMatrix(1).m, 4 * 4 * sizeof(float));
				memcpy(camera.cameraToWorld[1].m.m, perspCamera->GetCameraToWorldMatrix(1).m, 4 * 4 * sizeof(float));		
			} else {
				enableCameraHorizStereo = false;
				enableOculusRiftBarrel = false;
			}
			
			if (perspCamera->IsClippingPlaneEnabled()) {
				enableCameraClippingPlane = true;
				ASSIGN_VECTOR(camera.clippingPlaneCenter, perspCamera->clippingPlaneCenter);
				ASSIGN_VECTOR(camera.clippingPlaneNormal, perspCamera->clippingPlaneNormal);
			} else
				enableCameraClippingPlane = false;
			break;
		}
		default:
			throw std::runtime_error("Unknown camera type: " + boost::lexical_cast<std::string>(scene->camera->GetType()));
	}
						
}

static bool MeshPtrCompare(Mesh *p0, Mesh *p1) {
	return p0 < p1;
}

void CompiledScene::CompileGeometry() {
	SLG_LOG("Compile Geometry");

	const u_int objCount = scene->objDefs.GetSize();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	normals.resize(0);
	uvs.resize(0);
	cols.resize(0);
	alphas.resize(0);
	tris.resize(0);
	meshDescs.resize(0);

	//----------------------------------------------------------------------
	// Translate geometry
	//----------------------------------------------------------------------

	// Not using boost::unordered_map because because the key is a ExtMesh pointer
	map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)> definedMeshs(MeshPtrCompare);

	slg::ocl::Mesh newMeshDesc;
	newMeshDesc.vertsOffset = 0;
	newMeshDesc.trisOffset = 0;
	newMeshDesc.normalsOffset = 0;
	newMeshDesc.uvsOffset = 0;
	newMeshDesc.colsOffset = 0;
	newMeshDesc.alphasOffset = 0;
	memcpy(&newMeshDesc.trans.m, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));
	memcpy(&newMeshDesc.trans.mInv, &Matrix4x4::MAT_IDENTITY, sizeof(float[4][4]));

	slg::ocl::Mesh currentMeshDesc;
	for (u_int i = 0; i < objCount; ++i) {
		const ExtMesh *mesh = scene->objDefs.GetSceneObject(i)->GetExtMesh();

		bool isExistingInstance;
		if (mesh->GetType() == TYPE_EXT_TRIANGLE_INSTANCE) {
			// It is a instanced mesh
			ExtInstanceTriangleMesh *imesh = (ExtInstanceTriangleMesh *)mesh;

			// Check if is one of the already defined meshes
			map<ExtMesh *, u_int, bool (*)(Mesh *, Mesh *)>::iterator it = definedMeshs.find(imesh->GetExtTriangleMesh());
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

			// In order to express normals and vertices in local coordinates
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

		if (!isExistingInstance) {
			//------------------------------------------------------------------
			// Translate mesh normals (expressed in local coordinates)
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
			// Translate mesh vertices (expressed in local coordinates)
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

	worldBSphere = scene->dataSet->GetBSphere();

	const double tEnd = WallClockTime();
	SLG_LOG("Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
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

	return numeric_limits<float>::infinity();
}

void CompiledScene::CompileMaterials() {
	SLG_LOG("Compile Materials");

	CompileTextures();

	//--------------------------------------------------------------------------
	// Translate material definitions
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	usedMaterialTypes.clear();

	const u_int materialsCount = scene->matDefs.GetSize();
	mats.resize(materialsCount);
	useBumpMapping = false;

	for (u_int i = 0; i < materialsCount; ++i) {
		Material *m = scene->matDefs.GetMaterial(i);
		slg::ocl::Material *mat = &mats[i];
		//SLG_LOG(" Type: " << m->GetType());

		mat->matID = m->GetID();
		mat->lightID = m->GetLightID();
        mat->bumpSampleDistance = m->GetBumpSampleDistance();

		// Material emission
		const Texture *emitTex = m->GetEmitTexture();
		if (emitTex)
			mat->emitTexIndex = scene->texDefs.GetTextureIndex(emitTex);
		else
			mat->emitTexIndex = NULL_INDEX;
		ASSIGN_SPECTRUM(mat->emittedFactor, m->GetEmittedFactor());
		mat->usePrimitiveArea = m->IsUsingPrimitiveArea();

		// Material bump mapping
		const Texture *bumpTex = m->GetBumpTexture();
		if (bumpTex) {
			mat->bumpTexIndex = scene->texDefs.GetTextureIndex(bumpTex);
			useBumpMapping = true;
		} else
			mat->bumpTexIndex = NULL_INDEX;

		mat->samples = m->GetSamples();
		mat->visibility =
				(m->IsVisibleIndirectDiffuse() ? DIFFUSE : NONE) |
				(m->IsVisibleIndirectGlossy() ? GLOSSY : NONE) |
				(m->IsVisibleIndirectSpecular() ? SPECULAR : NONE);

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
				mat->glass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetExteriorIOR());
				mat->glass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(gm->GetInteriorIOR());
				break;
			}
			case ARCHGLASS: {
				ArchGlassMaterial *am = static_cast<ArchGlassMaterial *>(m);

				mat->type = slg::ocl::ARCHGLASS;
				mat->archglass.krTexIndex = scene->texDefs.GetTextureIndex(am->GetKr());
				mat->archglass.ktTexIndex = scene->texDefs.GetTextureIndex(am->GetKt());
				mat->archglass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(am->GetExteriorIOR());
				mat->archglass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(am->GetInteriorIOR());
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
				mat->roughglass.exteriorIorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetExteriorIOR());
				mat->roughglass.interiorIorTexIndex = scene->texDefs.GetTextureIndex(rgm->GetInteriorIOR());

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
			case VELVET: {
				VelvetMaterial *vm = static_cast<VelvetMaterial *>(m);

				mat->type = slg::ocl::VELVET;
				mat->velvet.kdTexIndex = scene->texDefs.GetTextureIndex(vm->GetKd());
				mat->velvet.p1TexIndex = scene->texDefs.GetTextureIndex(vm->GetP1());
				mat->velvet.p2TexIndex = scene->texDefs.GetTextureIndex(vm->GetP2());
				mat->velvet.p3TexIndex = scene->texDefs.GetTextureIndex(vm->GetP3());
				mat->velvet.thicknessTexIndex = scene->texDefs.GetTextureIndex(vm->GetThickness());
				break;
			}
            case CLOTH: {
                ClothMaterial *cm = static_cast<ClothMaterial *>(m);

				mat->type = slg::ocl::CLOTH;
                mat->cloth.Preset = cm->GetPreset();
                mat->cloth.Weft_KdIndex = scene->texDefs.GetTextureIndex(cm->GetWeftKd());
                mat->cloth.Weft_KsIndex = scene->texDefs.GetTextureIndex(cm->GetWeftKs());
                mat->cloth.Warp_KdIndex = scene->texDefs.GetTextureIndex(cm->GetWarpKd());
                mat->cloth.Warp_KsIndex = scene->texDefs.GetTextureIndex(cm->GetWarpKs());
                mat->cloth.Repeat_U = cm->GetRepeatU();
                mat->cloth.Repeat_V = cm->GetRepeatV();
                mat->cloth.specularNormalization = cm->GetSpecularNormalization();
                break;
            }
			case HOMOGENEOUS_VOL: {
				SLG_LOG("Volume rendering is not yet supported by OpenCL code");
				break;
			}
			default:
				throw runtime_error("Unknown material: " + boost::lexical_cast<string>(m->GetType()));
		}
	}

	//--------------------------------------------------------------------------
	// Translate mesh material indices
	//--------------------------------------------------------------------------

	const u_int objCount = scene->objDefs.GetSize();
	meshMats.resize(objCount);
	for (u_int i = 0; i < objCount; ++i) {
		const Material *m = scene->objDefs.GetSceneObject(i)->GetMaterial();
		meshMats[i] = scene->matDefs.GetMaterialIndex(m);
	}

	const double tEnd = WallClockTime();
	SLG_LOG("Material compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileLights() {
	SLG_LOG("Compile Lights");

	//--------------------------------------------------------------------------
	// Translate lights
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	const std::vector<LightSource *> &lightSources = scene->lightDefs.GetLightSources();

	const u_int lightCount = lightSources.size();
	lightDefs.resize(lightCount);
	envLightIndices.clear();
	infiniteLightDistributions.clear();

	for (u_int i = 0; i < lightSources.size(); ++i) {
		const LightSource *l = lightSources[i];
		slg::ocl::LightSource *oclLight = &lightDefs[i];
		oclLight->lightSceneIndex = l->lightSceneIndex;
		oclLight->lightID = l->GetID();
		oclLight->samples = l->GetSamples();
		oclLight->visibility = 
				(l->IsVisibleIndirectDiffuse() ? DIFFUSE : NONE) |
				(l->IsVisibleIndirectGlossy() ? GLOSSY : NONE) |
				(l->IsVisibleIndirectSpecular() ? SPECULAR : NONE);

		switch (l->GetType()) {
			case TYPE_TRIANGLE: {
				const TriangleLight *tl = (const TriangleLight *)l;

				const ExtMesh *mesh = tl->mesh;
				const Triangle *tri = &(mesh->GetTriangles()[tl->triangleIndex]);

				// LightSource data
				oclLight->type = slg::ocl::TYPE_TRIANGLE;

				// TriangleLight data
				ASSIGN_VECTOR(oclLight->triangle.v0, mesh->GetVertex(tri->v[0]));
				ASSIGN_VECTOR(oclLight->triangle.v1, mesh->GetVertex(tri->v[1]));
				ASSIGN_VECTOR(oclLight->triangle.v2, mesh->GetVertex(tri->v[2]));
				if (mesh->HasUVs()) {
					ASSIGN_UV(oclLight->triangle.uv0, mesh->GetUV(tri->v[0]));
					ASSIGN_UV(oclLight->triangle.uv1, mesh->GetUV(tri->v[1]));
					ASSIGN_UV(oclLight->triangle.uv2, mesh->GetUV(tri->v[2]));
				} else {
					const UV zero;
					ASSIGN_UV(oclLight->triangle.uv0, zero);
					ASSIGN_UV(oclLight->triangle.uv1, zero);
					ASSIGN_UV(oclLight->triangle.uv2, zero);
				}
				oclLight->triangle.invArea = 1.f / tl->GetArea();

				oclLight->triangle.materialIndex = scene->matDefs.GetMaterialIndex(tl->lightMaterial);

				const SampleableSphericalFunction *emissionFunc = tl->lightMaterial->GetEmissionFunc();
				if (emissionFunc) {
					oclLight->triangle.avarage = emissionFunc->Average();
					oclLight->triangle.imageMapIndex = scene->imgMapCache.GetImageMapIndex(
							// I use only ImageMapSphericalFunction
							((const ImageMapSphericalFunction *)(emissionFunc->GetFunc()))->GetImageMap());
				} else {
					oclLight->triangle.avarage = 0.f;
					oclLight->triangle.imageMapIndex = NULL_INDEX;
				}
				break;
			}
			case TYPE_IL: {
				const InfiniteLight *il = (const InfiniteLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &il->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &il->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, il->gain);

				// InfiniteLight data
				CompileTextureMapping2D(&oclLight->notIntersecable.infinite.mapping, &il->mapping);
				oclLight->notIntersecable.infinite.imageMapIndex = scene->imgMapCache.GetImageMapIndex(il->imageMap);

				// Compile the image map Distribution2D
				const Distribution2D *dist;
				il->GetPreprocessedData(&dist);
				u_int distributionSize;
				const float *infiniteLightDistribution = CompileDistribution2D(dist,
						&distributionSize);

				// Copy the Distribution2D data in the right place
				const u_int size = infiniteLightDistributions.size();
				infiniteLightDistributions.resize(size + distributionSize);
				copy(infiniteLightDistribution, infiniteLightDistribution + distributionSize,
						&infiniteLightDistributions[size]);
				delete[] infiniteLightDistribution;

				oclLight->notIntersecable.infinite.distributionOffset = size;
				break;
			}
			case TYPE_IL_SKY: {
				const SkyLight *sl = (const SkyLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL_SKY;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, sl->gain);

				// SkyLight data
				sl->GetPreprocessedData(
						NULL,
						&oclLight->notIntersecable.sky.absoluteTheta, &oclLight->notIntersecable.sky.absolutePhi,
						&oclLight->notIntersecable.sky.zenith_Y, &oclLight->notIntersecable.sky.zenith_x,
						&oclLight->notIntersecable.sky.zenith_y, oclLight->notIntersecable.sky.perez_Y,
						oclLight->notIntersecable.sky.perez_x, oclLight->notIntersecable.sky.perez_y);
				break;
			}
			case TYPE_IL_SKY2: {
				const SkyLight2 *sl = (const SkyLight2 *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL_SKY2;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, sl->gain);

				// SkyLight2 data
				sl->GetPreprocessedData(
						&oclLight->notIntersecable.sky2.absoluteSunDir.x,
						oclLight->notIntersecable.sky2.aTerm.c,
						oclLight->notIntersecable.sky2.bTerm.c,
						oclLight->notIntersecable.sky2.cTerm.c,
						oclLight->notIntersecable.sky2.dTerm.c,
						oclLight->notIntersecable.sky2.eTerm.c,
						oclLight->notIntersecable.sky2.fTerm.c,
						oclLight->notIntersecable.sky2.gTerm.c,
						oclLight->notIntersecable.sky2.hTerm.c,
						oclLight->notIntersecable.sky2.iTerm.c,
						oclLight->notIntersecable.sky2.radianceTerm.c);
				break;
			}
			case TYPE_SUN: {
				const SunLight *sl = (const SunLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SUN;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, sl->gain);

				// SunLight data
				sl->GetPreprocessedData(
						&oclLight->notIntersecable.sun.absoluteDir.x,
						&oclLight->notIntersecable.sun.x.x,
						&oclLight->notIntersecable.sun.y.x,
						NULL, NULL, NULL,
						&oclLight->notIntersecable.sun.cosThetaMax, &oclLight->notIntersecable.sun.sin2ThetaMax);
				ASSIGN_SPECTRUM(oclLight->notIntersecable.sun.color, sl->color);
				oclLight->notIntersecable.sun.turbidity = sl->turbidity;
				oclLight->notIntersecable.sun.relSize= sl->relSize;
				break;
			}
			case TYPE_POINT: {
				const PointLight *pl = (const PointLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_POINT;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &pl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &pl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, pl->gain);

				// PointLight data
				pl->GetPreprocessedData(
					NULL,
					&oclLight->notIntersecable.point.absolutePos.x,
					oclLight->notIntersecable.point.emittedFactor.c);
				break;
			}
			case TYPE_MAPPOINT: {
				const MapPointLight *mpl = (const MapPointLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_MAPPOINT;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &mpl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &mpl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, mpl->gain);

				// MapPointLight data
				const SampleableSphericalFunction *funcData;
				mpl->GetPreprocessedData(
					&oclLight->notIntersecable.mapPoint.localPos.x,
					&oclLight->notIntersecable.mapPoint.absolutePos.x,
					oclLight->notIntersecable.mapPoint.emittedFactor.c,
					&funcData);
				oclLight->notIntersecable.mapPoint.avarage = funcData->Average();
				oclLight->notIntersecable.mapPoint.imageMapIndex = scene->imgMapCache.GetImageMapIndex(mpl->imageMap);
				break;
			}
			case TYPE_SPOT: {
				const SpotLight *sl = (const SpotLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SPOT;

				// NotIntersecableLightSource data
				//memcpy(&oclLight->notIntersecable.light2World.m, &sl->lightToWorld.m, sizeof(float[4][4]));
				//memcpy(&oclLight->notIntersecable.light2World.mInv, &sl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, sl->gain);

				// SpotLight data
				const Transform *alignedWorld2Light;
				sl->GetPreprocessedData(
					oclLight->notIntersecable.spot.emittedFactor.c,
					&oclLight->notIntersecable.spot.absolutePos.x,
					&oclLight->notIntersecable.spot.cosTotalWidth,
					&oclLight->notIntersecable.spot.cosFalloffStart,
					&alignedWorld2Light);
				
				memcpy(&oclLight->notIntersecable.light2World.m, &alignedWorld2Light->m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &alignedWorld2Light->mInv, sizeof(float[4][4]));
				break;
			}
			case TYPE_PROJECTION: {
				const ProjectionLight *pl = (const ProjectionLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_PROJECTION;

				// NotIntersecableLightSource data
				//memcpy(&oclLight->notIntersecable.light2World.m, &pl->lightToWorld.m, sizeof(float[4][4]));
				//memcpy(&oclLight->notIntersecable.light2World.mInv, &pl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, pl->gain);

				// ProjectionLight data
				oclLight->notIntersecable.projection.imageMapIndex = scene->imgMapCache.GetImageMapIndex(pl->imageMap);
				const Transform *alignedWorld2Light, *lightProjection;
				pl->GetPreprocessedData(
					oclLight->notIntersecable.projection.emittedFactor.c,
					&(oclLight->notIntersecable.projection.absolutePos.x),
					&(oclLight->notIntersecable.projection.lightNormal.x),
					&oclLight->notIntersecable.projection.screenX0,
					&oclLight->notIntersecable.projection.screenX1,
					&oclLight->notIntersecable.projection.screenY0,
					&oclLight->notIntersecable.projection.screenY1,
					&alignedWorld2Light, &lightProjection);

				memcpy(&oclLight->notIntersecable.light2World.m, &alignedWorld2Light->m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &alignedWorld2Light->mInv, sizeof(float[4][4]));

				memcpy(&oclLight->notIntersecable.projection.lightProjection.m, &lightProjection->m, sizeof(float[4][4]));
				break;
			}
			case TYPE_IL_CONSTANT: {
				const ConstantInfiniteLight *cil = (const ConstantInfiniteLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_IL_CONSTANT;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &cil->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &cil->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, cil->gain);

				// ConstantInfiniteLight data
				ASSIGN_SPECTRUM(oclLight->notIntersecable.constantInfinite.color, cil->color);
				break;
			}
		case TYPE_SHARPDISTANT: {
				const SharpDistantLight *sdl = (const SharpDistantLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_SHARPDISTANT;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &sdl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &sdl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, sdl->gain);

				// SharpDistantLight data
				ASSIGN_SPECTRUM(oclLight->notIntersecable.sharpDistant.color, sdl->color);
				sdl->GetPreprocessedData(&(oclLight->notIntersecable.sharpDistant.absoluteLightDir.x), NULL, NULL);
				break;
			}
			case TYPE_DISTANT: {
				const DistantLight *dl = (const DistantLight *)l;

				// LightSource data
				oclLight->type = slg::ocl::TYPE_DISTANT;

				// NotIntersecableLightSource data
				memcpy(&oclLight->notIntersecable.light2World.m, &dl->lightToWorld.m, sizeof(float[4][4]));
				memcpy(&oclLight->notIntersecable.light2World.mInv, &dl->lightToWorld.mInv, sizeof(float[4][4]));
				ASSIGN_SPECTRUM(oclLight->notIntersecable.gain, dl->gain);

				// DistantLight data
				ASSIGN_SPECTRUM(oclLight->notIntersecable.sharpDistant.color, dl->color);
				dl->GetPreprocessedData(
					&(oclLight->notIntersecable.distant.absoluteLightDir.x),
					&(oclLight->notIntersecable.distant.x.x),
					&(oclLight->notIntersecable.distant.y.x),
					NULL,
					&(oclLight->notIntersecable.distant.cosThetaMax));
				break;
			}
			default:
				throw runtime_error("Unknown Light source type in CompiledScene::CompileLights()");
		}
		
		if (l->IsEnvironmental())
			envLightIndices.push_back(i);
	}

	lightTypeCounts = scene->lightDefs.GetLightTypeCounts();
	meshTriLightDefsOffset = scene->lightDefs.GetLightIndexByMeshIndex();

	// Compile LightDistribution
	delete[] lightsDistribution;
	lightsDistribution = CompileDistribution1D(scene->lightDefs.GetLightsDistribution(), &lightsDistributionSize);

	const double tEnd = WallClockTime();
	SLG_LOG("Lights compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
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
			throw runtime_error("Unknown 2D texture mapping: " + boost::lexical_cast<string>(m->GetType()));
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
			throw runtime_error("Unknown texture mapping: " + boost::lexical_cast<string>(m->GetType()));
	}
}

float *CompiledScene::CompileDistribution1D(const Distribution1D *dist, u_int *size) {
	const u_int count = dist->GetCount();
	*size = sizeof(u_int) + count * sizeof(float) + (count + 1) * sizeof(float);
	float *compDist = new float[*size];

	*((u_int *)&compDist[0]) = count;
	copy(dist->GetFuncs(), dist->GetFuncs() + count,
			compDist + 1);
	copy(dist->GetCDFs(), dist->GetCDFs() + count + 1,
			compDist + 1 + count);

	return compDist;
}

float *CompiledScene::CompileDistribution2D(const Distribution2D *dist, u_int *size) {
	u_int marginalSize;
	float *marginalDist = CompileDistribution1D(dist->GetMarginalDistribution(),
			&marginalSize);

	u_int condSize;
	vector<float *> condDists;
	for (u_int i = 0; i < dist->GetHeight(); ++i) {
		condDists.push_back(CompileDistribution1D(dist->GetConditionalDistribution(i),
			&condSize));
	}

	*size = 2 * sizeof(u_int) + marginalSize + condDists.size() * condSize;
	float *compDist = new float[*size];

	*((u_int *)&compDist[0]) = dist->GetWidth();
	*((u_int *)&compDist[1]) = dist->GetHeight();

	float *ptr = &compDist[2];
	copy(marginalDist, marginalDist + marginalSize, ptr);
	ptr += marginalSize / 4;
	delete[] marginalDist;

	const u_int condSize4 = condSize / sizeof(float);
	for (u_int i = 0; i < dist->GetHeight(); ++i) {
		copy(condDists[i], condDists[i] + condSize4, ptr);
		ptr += condSize4;
		delete[] condDists[i];
	}

	return compDist;
}

void CompiledScene::CompileTextures() {
	SLG_LOG("Compile Textures");
	//SLG_LOG("  Texture size: " << sizeof(slg::ocl::Texture));

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
			case BLENDER_CLOUDS: {
				BlenderCloudsTexture *wt = static_cast<BlenderCloudsTexture *>(t);
				tex->type = slg::ocl::BLENDER_CLOUDS;
				CompileTextureMapping3D(&tex->blenderClouds.mapping, wt->GetTextureMapping());				 
				tex->blenderClouds.noisesize = wt->GetNoiseSize();
				tex->blenderClouds.noisedepth = wt->GetNoiseDepth();
				tex->blenderClouds.bright = wt->GetBright();
				tex->blenderClouds.contrast = wt->GetContrast();
				tex->blenderClouds.hard = wt->GetNoiseType();

				break;
			}
			case BLENDER_WOOD: {
				BlenderWoodTexture *wt = static_cast<BlenderWoodTexture *>(t);

				tex->type = slg::ocl::BLENDER_WOOD;
				CompileTextureMapping3D(&tex->blenderWood.mapping, wt->GetTextureMapping());				 
				tex->blenderWood.turbulence = wt->GetTurbulence();
				tex->blenderWood.bright = wt->GetBright();
				tex->blenderWood.contrast = wt->GetContrast();
				tex->blenderWood.hard = wt->GetNoiseType();
				tex->blenderWood.noisesize = wt->GetNoiseSize();
				switch (wt->GetNoiseBasis2()) {
					default:
					case TEX_SIN:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_SIN;
						break;
					case TEX_SAW:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_SAW;
						break;
					case TEX_TRI:
						tex->blenderWood.noisebasis2 = slg::ocl::TEX_TRI;
						break;
				}
				
				switch (wt->GetWoodType()) {
					default:
					case BANDS:
						tex->blenderWood.type = slg::ocl::BANDS;
						break;
					case RINGS:
						tex->blenderWood.type = slg::ocl::RINGS;
						break;
					case BANDNOISE:
						tex->blenderWood.type = slg::ocl::BANDNOISE;
						break;
					case RINGNOISE:
						tex->blenderWood.type = slg::ocl::RINGNOISE;
						break;
				}
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

				const vector<float> &offsets = bt->GetOffsets();
				const vector<Spectrum> &values = bt->GetValues();
				if (offsets.size() > BAND_TEX_MAX_SIZE)
					throw runtime_error("BandTexture with more than " + ToString(BAND_TEX_MAX_SIZE) + " are not supported");
				tex->band.size = offsets.size();
				for (u_int i = 0; i < BAND_TEX_MAX_SIZE; ++i) {
					if (i < offsets.size()) {
						tex->band.offsets[i] = offsets[i];
						ASSIGN_SPECTRUM(tex->band.values[i], values[i]);
					} else {
						tex->band.offsets[i] = 1.f;
						tex->band.values[i].c[0] = 0.f;
						tex->band.values[i].c[1] = 0.f;
						tex->band.values[i].c[2] = 0.f;
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
            case NORMALMAP_TEX: {
                NormalMapTexture *nmt = static_cast<NormalMapTexture *>(t);

                tex->type = slg::ocl::NORMALMAP_TEX;
                const Texture *normalTex = nmt->GetTexture();
				tex->normalMap.texIndex = scene->texDefs.GetTextureIndex(normalTex);
				break;
            }
			default:
				throw runtime_error("Unknown texture: " + boost::lexical_cast<string>(t->GetType()));
				break;
		}
	}
		
	const double tEnd = WallClockTime();
	SLG_LOG("Textures compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::CompileImageMaps() {
	SLG_LOG("Compile ImageMaps");

	imageMapDescs.resize(0);
	imageMapMemBlocks.resize(0);

	//--------------------------------------------------------------------------
	// Translate image maps
	//--------------------------------------------------------------------------

	const double tStart = WallClockTime();

	vector<const ImageMap *> ims;
	scene->imgMapCache.GetImageMaps(ims);

	imageMapDescs.resize(ims.size());
	for (u_int i = 0; i < ims.size(); ++i) {
		const ImageMap *im = ims[i];
		slg::ocl::ImageMap *imd = &imageMapDescs[i];

		const u_int pixelCount = im->GetWidth() * im->GetHeight();
		const u_int memSize = pixelCount * im->GetChannelCount() * sizeof(float);

		if (memSize > maxMemPageSize)
			throw runtime_error("An image map is too big to fit in a single block of memory");

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
			if (imageMapMemBlocks.size() > 8)
				throw runtime_error("More than 8 blocks of memory are required for image maps");

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

	SLG_LOG("Image maps page count: " << imageMapMemBlocks.size());
	for (u_int i = 0; i < imageMapMemBlocks.size(); ++i)
		SLG_LOG(" RGB channel page " << i << " size: " << imageMapMemBlocks[i].size() * sizeof(float) / 1024 << "Kbytes");

	const double tEnd = WallClockTime();
	SLG_LOG("Texture maps compilation time: " << int((tEnd - tStart) * 1000.0) << "ms");
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT) || editActions.Has(MATERIAL_TYPES_EDIT))
		CompileMaterials();
	if (editActions.Has(LIGHTS_EDIT))
		CompileLights();
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
