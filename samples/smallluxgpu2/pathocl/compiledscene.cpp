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

	EditActionList editActions;
	editActions.AddAllAction();
	Recompile(editActions);
}

CompiledScene::~CompiledScene() {
}

void CompiledScene::CompileCamera() {
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
	Scene *scene = renderConfig->scene;

	const unsigned int verticesCount = scene->dataSet->GetTotalVertexCount();
	const unsigned int trianglesCount = scene->dataSet->GetTotalTriangleCount();

	const double tStart = WallClockTime();

	// Clear vectors
	verts.resize(0);
	colors.resize(0);
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

					definedMeshs[imesh->GetExtTriangleMesh()] = definedMeshs.size();
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
				// Translate mesh colors
				//--------------------------------------------------------------

				for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
					if (mesh->HasColors())
						colors.push_back(mesh->GetColor(j));
					else
						colors.push_back(Spectrum(1.f, 1.f, 1.f));
				}

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
		// Translate mesh colors
		//----------------------------------------------------------------------

		colors.reserve(verticesCount);
		for (unsigned int i = 0; i < scene->objects.size(); ++i) {
			ExtMesh *mesh = scene->objects[i];

			for (unsigned int j = 0; j < mesh->GetTotalVertexCount(); ++j) {
				if (mesh->HasColors())
					colors.push_back(mesh->GetColor(j));
				else
					colors.push_back(Spectrum(1.f, 1.f, 1.f));
			}
		}

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
	cerr << "[PathOCLRenderThread::CompiledSession] Scene geometry compilation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::CompileMaterials() {
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

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT) || editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
	if (editActions.Has(MATERIALS_EDIT))
		CompileMaterials();
}
