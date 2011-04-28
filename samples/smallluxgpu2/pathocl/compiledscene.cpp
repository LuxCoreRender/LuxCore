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

	enable_MAT_MATTE = false;
	enable_MAT_AREALIGHT = false;
	enable_MAT_MIRROR = false;
	enable_MAT_GLASS = false;
	enable_MAT_MATTEMIRROR = false;
	enable_MAT_METAL = false;
	enable_MAT_MATTEMETAL = false;
	enable_MAT_ALLOY = false;
	enable_MAT_ARCHGLASS = false;

	meshIDs = NULL;
	triangleIDs = NULL;

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
	cerr << "[PathOCLRenderThread::CompiledSession] Scene geometry translation time: " << int((tEnd - tStart) * 1000.0) << "ms" << endl;
}

void CompiledScene::Recompile(const EditActionList &editActions) {
	if (editActions.Has(FILM_EDIT) || editActions.Has(CAMERA_EDIT))
		CompileCamera();
	if (editActions.Has(GEOMETRY_EDIT))
		CompileGeometry();
}
