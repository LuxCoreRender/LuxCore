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

#include <iostream>
#include <cstring>

#include "luxrays/utils/core/exttrianglemesh.h"
#include "luxrays/utils/plymesh/rply.h"

using namespace luxrays;

// rply vertex callback
static int VertexCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	Point *p = *static_cast<Point **> (userData);

	long vertIndex;
	ply_get_argument_element(argument, NULL, &vertIndex);

	if (userIndex == 0)
		p[vertIndex].x =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 1)
		p[vertIndex].y =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 2)
		p[vertIndex].z =
			static_cast<float>(ply_get_argument_value(argument));

	return 1;
}

// rply normal callback
static int NormalCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = NULL;

	ply_get_argument_user_data(argument, &userData, &userIndex);

	Normal *n = *static_cast<Normal **> (userData);

	long normIndex;
	ply_get_argument_element(argument, NULL, &normIndex);

	if (userIndex == 0)
		n[normIndex].x =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 1)
		n[normIndex].y =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 2)
		n[normIndex].z =
			static_cast<float>(ply_get_argument_value(argument));

	return 1;
}

// rply uv callback
static int UVCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	UV *uv = *static_cast<UV **> (userData);

	long uvIndex;
	ply_get_argument_element(argument, NULL, &uvIndex);

	if (userIndex == 0)
		uv[uvIndex].u =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 1)
		uv[uvIndex].v =
			static_cast<float>(ply_get_argument_value(argument));

	return 1;
}

// rply face callback
static int FaceCB(p_ply_argument argument) {
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, NULL);

	Triangle *verts = *static_cast<Triangle **> (userData);

	long triIndex;
	ply_get_argument_element(argument, NULL, &triIndex);

	long length, valueIndex;
	ply_get_argument_property(argument, NULL, &length, &valueIndex);

	if (valueIndex >= 0 && valueIndex < 3) {
		verts[triIndex].v[valueIndex] =
				static_cast<unsigned int> (ply_get_argument_value(argument));
	}

	return 1;
}

ExtTriangleMesh *ExtTriangleMesh::LoadExtTriangleMesh(const std::string &fileName, const bool usePlyNormals) {
	p_ply plyfile = ply_open(fileName.c_str(), NULL);
	if (!plyfile) {
		std::stringstream ss;
		ss << "Unable to read PLY mesh file '" << fileName << "'";
		throw std::runtime_error(ss.str());
	}

	if (!ply_read_header(plyfile)) {
		std::stringstream ss;
		ss << "Unable to read PLY header from '" << fileName << "'";
		throw std::runtime_error(ss.str());
	}

	Point *p;
	long plyNbVerts = ply_set_read_cb(plyfile, "vertex", "x", VertexCB, &p, 0);
	ply_set_read_cb(plyfile, "vertex", "y", VertexCB, &p, 1);
	ply_set_read_cb(plyfile, "vertex", "z", VertexCB, &p, 2);
	if (plyNbVerts <= 0) {
		std::stringstream ss;
		ss << "No vertices found in '" << fileName << "'";
		throw std::runtime_error(ss.str());
	}

	Triangle *vi;
	long plyNbTris = ply_set_read_cb(plyfile, "face", "vertex_indices", FaceCB, &vi, 0);
	if (plyNbTris <= 0) {
		std::stringstream ss;
		ss << "No triangles found in '" << fileName << "'";
		throw std::runtime_error(ss.str());
	}

	Normal *n;
	long plyNbNormals = ply_set_read_cb(plyfile, "vertex", "nx", NormalCB, &n, 0);
	ply_set_read_cb(plyfile, "vertex", "ny", NormalCB, &n, 1);
	ply_set_read_cb(plyfile, "vertex", "nz", NormalCB, &n, 2);
	if (((plyNbNormals > 0) || usePlyNormals) && (plyNbNormals != plyNbVerts)) {
		std::stringstream ss;
		ss << "Wrong count of normals in '" << fileName << "'";
		throw std::runtime_error(ss.str());
	}

	UV *uv;
	long plyNbUVs = ply_set_read_cb(plyfile, "vertex", "s", UVCB, &uv, 0);
	ply_set_read_cb(plyfile, "vertex", "t", UVCB, &uv, 1);

	p = new Point[plyNbVerts];
	vi = new Triangle[plyNbTris];
	n = new Normal[plyNbVerts];
	if (plyNbUVs == 0)
		uv = NULL;
	else
		uv = new UV[plyNbUVs];

	if (!ply_read(plyfile)) {
		std::stringstream ss;
		ss << "Unable to parse PLY file '" << fileName << "'";

		delete[] p;
		delete[] vi;
		delete[] n;
		delete[] uv;

		throw std::runtime_error(ss.str());
	}

	ply_close(plyfile);

	return CreateExtTriangleMesh(plyNbVerts, plyNbTris, p, vi, n, uv, usePlyNormals);
}

ExtTriangleMesh *ExtTriangleMesh::CreateExtTriangleMesh(
	const long plyNbVerts, const long plyNbTris,
	Point *p, Triangle *vi, Normal *n, UV *uv,
	const bool usePlyNormals) {

	const unsigned int vertexCount = plyNbVerts;
	const unsigned int triangleCount = plyNbTris;
	Point *vertices = p;
	Triangle *triangles = vi;
	Normal *vertNormals = n;
	UV *vertUV = uv;

	if (!usePlyNormals) {
		// It looks like normals exported by Blender are bugged
		for (unsigned int i = 0; i < vertexCount; ++i)
			vertNormals[i] = Normal(0.f, 0.f, 0.f);
		for (unsigned int i = 0; i < triangleCount; ++i) {
			const Vector e1 = vertices[triangles[i].v[1]] - vertices[triangles[i].v[0]];
			const Vector e2 = vertices[triangles[i].v[2]] - vertices[triangles[i].v[0]];
			const Normal N = Normal(Normalize(Cross(e1, e2)));
			vertNormals[triangles[i].v[0]] += N;
			vertNormals[triangles[i].v[1]] += N;
			vertNormals[triangles[i].v[2]] += N;
		}
		//int printedWarning = 0;
		for (unsigned int i = 0; i < vertexCount; ++i) {
			vertNormals[i] = Normalize(vertNormals[i]);
			// Check for degenerate triangles/normals, they can freeze the GPU
			if (isnan(vertNormals[i].x) || isnan(vertNormals[i].y) || isnan(vertNormals[i].z)) {
				/*if (printedWarning < 15) {
					SDL_LOG("The model contains a degenerate normal (index " << i << ")");
					++printedWarning;
				} else if (printedWarning == 15) {
					SDL_LOG("The model contains more degenerate normals");
					++printedWarning;
				}*/
				vertNormals[i] = Normal(0.f, 0.f, 1.f);
			}
		}
	}

	return new ExtTriangleMesh(vertexCount, triangleCount, vertices, triangles, vertNormals, vertUV);
}

//------------------------------------------------------------------------------
// ExtTriangleMesh
//------------------------------------------------------------------------------

ExtTriangleMesh::ExtTriangleMesh(ExtTriangleMesh *mesh) {
	assert (mesh != NULL);

	vertCount = mesh->vertCount;
	triCount = mesh->triCount;
	vertices = mesh->vertices;
	tris = mesh->tris;

	normals = mesh->normals;
	triNormals = mesh->triNormals;
	uvs = mesh->uvs;
}

ExtTriangleMesh::ExtTriangleMesh(const unsigned int meshVertCount, const unsigned int meshTriCount,
		Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV) {
	assert (meshVertCount > 0);
	assert (meshTriCount > 0);
	assert (meshVertices != NULL);
	assert (meshTris != NULL);

	vertCount = meshVertCount;
	triCount = meshTriCount;
	vertices = meshVertices;
	tris = meshTris;

	normals = meshNormals;
	uvs = meshUV;

	// Compute all triangle normals
	triNormals = new Normal[triCount];
	for (u_int i = 0; i < triCount; ++i)
		triNormals[i] = tris[i].GetGeometryNormal(vertices);;
}

BBox ExtTriangleMesh::GetBBox() const {
	BBox bbox;
	for (unsigned int i = 0; i < vertCount; ++i)
		bbox = Union(bbox, vertices[i]);

	return bbox;
}

void ExtTriangleMesh::ApplyTransform(const Transform &trans) {
	for (unsigned int i = 0; i < vertCount; ++i)
		vertices[i] *= trans;
}
