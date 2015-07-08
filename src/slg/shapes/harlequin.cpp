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

#include "luxrays/core/randomgen.h"
#include "slg/shapes/harlequin.h"
#include "slg/scene/scene.h"

using namespace std;
using namespace luxrays;
using namespace slg;

#define HARLEQUIN_COLOR_TABLE_SIZE 0x1f

HarlequinShape::HarlequinShape(ExtMesh *srcMesh) : Shape() {
	// Build the harlequin color table
	
	vector<Spectrum> hrlqColorTable(HARLEQUIN_COLOR_TABLE_SIZE);
	for (u_int i = 0; i < HARLEQUIN_COLOR_TABLE_SIZE; i++) {
		hrlqColorTable[i] = Spectrum(
				RadicalInverse(i + 1, 2),
				RadicalInverse(i + 1, 3),
				RadicalInverse(i + 1, 5));
	}
	
	// Build the new mesh with harlequin colors

	const u_int triCount = srcMesh->GetTotalTriangleCount();

	const Point *verts = srcMesh->GetVertices();
	const Triangle *tris = srcMesh->GetTriangles();

	const Normal *norms = srcMesh->HasNormals() ? srcMesh->GetNormals() : NULL;
	const UV *uvs = srcMesh->HasUVs() ? srcMesh->GetUVs() : NULL;
	const float *alphas = srcMesh->HasAlphas() ? srcMesh->GetAlphas() : NULL;

	const u_int hrlqVertCount = 3 * triCount;
	Point *hrlqVerts = TriangleMesh::AllocVerticesBuffer(hrlqVertCount);
	Triangle *hrlqTris = TriangleMesh::AllocTrianglesBuffer(triCount);

	Normal *hrlqNorms = srcMesh->HasNormals() ? (new Normal[hrlqVertCount]) : NULL;
	UV *hrlqUVs = srcMesh->HasUVs() ? (new UV[hrlqVertCount]) : NULL;
	Spectrum *hrlqCols = new Spectrum[hrlqVertCount];
	float *hrlqAlphas = srcMesh->HasAlphas() ? (new float[hrlqVertCount]) : NULL;

	u_int vIndex = 0;
	for (u_int i = 0; i < triCount; ++i) {
		const Triangle &tri = tris[i];

		for (u_int j = 0; j < 3; ++j) {
			hrlqVerts[vIndex] = verts[tri.v[j]];
			if (hrlqNorms)
				hrlqNorms[vIndex] = norms[tri.v[j]];
			if (hrlqUVs)
				hrlqUVs[vIndex] = uvs[tri.v[j]];

			hrlqCols[vIndex] = hrlqColorTable[i % HARLEQUIN_COLOR_TABLE_SIZE];

			if (hrlqAlphas)
				hrlqAlphas[vIndex] = alphas[tri.v[j]];

			hrlqTris[i].v[j] = vIndex;

			++vIndex;
		}
	}

	mesh = new ExtTriangleMesh(hrlqVertCount, triCount, hrlqVerts, hrlqTris,
			hrlqNorms, hrlqUVs, hrlqCols, hrlqAlphas);
}

HarlequinShape::~HarlequinShape() {
	if (!refined)
		delete mesh;
}

ExtMesh *HarlequinShape::RefineImpl(const Scene *scene) {
	return mesh;
}
