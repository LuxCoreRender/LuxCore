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

#include <iostream>
#include <fstream>
#include <cstring>

#include <boost/format.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/ply/rply.h"
#include "luxrays/utils/serializationutils.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// ExtMesh PLY reader
//------------------------------------------------------------------------------

// rply vertex callback
static int VertexCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	Point *p = *static_cast<Point **> (userData);
	if (!p) return 1;

	long vertIndex;
	ply_get_argument_element(argument, nullptr, &vertIndex);

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
	void *userData = nullptr;

	ply_get_argument_user_data(argument, &userData, &userIndex);

	Normal *n = *static_cast<Normal **> (userData);
	if (!n) return 1;

	long normIndex;
	ply_get_argument_element(argument, nullptr, &normIndex);

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
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	auto uv = *static_cast<ExtMeshProp<UV>::Layer *> (userData);

	long uvIndex;
	ply_get_argument_element(argument, nullptr, &uvIndex);

	if (userIndex == 0)
		uv[uvIndex].u =
			static_cast<float>(ply_get_argument_value(argument));
	else if (userIndex == 1)
		uv[uvIndex].v =
			static_cast<float>(ply_get_argument_value(argument));

	return 1;
}

// rply color callback
static int ColorCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	auto c = *static_cast<ExtMeshProp<float>::Layer *> (userData);
	//float *c = *static_cast<float **> (userData);

	long colIndex;
	ply_get_argument_element(argument, nullptr, &colIndex);

	// Check the type of value used
	p_ply_property property = nullptr;
	ply_get_argument_property(argument, &property, nullptr, nullptr);
	e_ply_type dataType;
	ply_get_property_info(property, nullptr, &dataType, nullptr, nullptr);
	if (dataType == PLY_UCHAR) {
		if (userIndex == 0)
			c[colIndex * 3] =
				static_cast<float>(ply_get_argument_value(argument) / 255.0);
		else if (userIndex == 1)
			c[colIndex * 3 + 1] =
				static_cast<float>(ply_get_argument_value(argument) / 255.0);
		else if (userIndex == 2)
			c[colIndex * 3 + 2] =
				static_cast<float>(ply_get_argument_value(argument) / 255.0);
	} else {
		if (userIndex == 0)
			c[colIndex * 3] =
				static_cast<float>(ply_get_argument_value(argument));
		else if (userIndex == 1)
			c[colIndex * 3 + 1] =
				static_cast<float>(ply_get_argument_value(argument));
		else if (userIndex == 2)
			c[colIndex * 3 + 2] =
				static_cast<float>(ply_get_argument_value(argument));
	}

	return 1;
}

// rply vertex callback
static int AlphaCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	auto c = *static_cast<ExtMeshProp<float>::Layer *> (userData);

	long alphaIndex;
	ply_get_argument_element(argument, nullptr, &alphaIndex);

	// Check the type of value used
	p_ply_property property = nullptr;
	ply_get_argument_property(argument, &property, nullptr, nullptr);
	e_ply_type dataType;
	ply_get_property_info(property, nullptr, &dataType, nullptr, nullptr);
	if (dataType == PLY_UCHAR) {
		if (userIndex == 0)
			c[alphaIndex] =
				static_cast<float>(ply_get_argument_value(argument) / 255.0);
	} else {
		if (userIndex == 0)
			c[alphaIndex] =
				static_cast<float>(ply_get_argument_value(argument));		
	}

	return 1;
}

// rply vertex callback
static int VertexAOVCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	auto c = *static_cast<ExtMeshProp<float>::Layer *> (userData);

	long alphaIndex;
	ply_get_argument_element(argument, nullptr, &alphaIndex);

	// Check the type of value used
	p_ply_property property = nullptr;
	ply_get_argument_property(argument, &property, nullptr, nullptr);
	e_ply_type dataType;
	ply_get_property_info(property, nullptr, &dataType, nullptr, nullptr);
	if (dataType == PLY_UCHAR) {
		if (userIndex == 0)
			c[alphaIndex] =
				static_cast<float>(ply_get_argument_value(argument) / 255.0);
	} else {
		if (userIndex == 0)
			c[alphaIndex] =
				static_cast<float>(ply_get_argument_value(argument));		
	}

	return 1;
}

// rply face callback
static int FaceCB(p_ply_argument argument) {
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, nullptr);

	vector<Triangle> *tris = static_cast<vector<Triangle> *> (userData);

	long length, valueIndex;
	ply_get_argument_property(argument, nullptr, &length, &valueIndex);

	if (length == 3) {
		if (valueIndex < 0)
			tris->push_back(Triangle());
		else if (valueIndex < 3)
			tris->back().v[valueIndex] =
					static_cast<u_int> (ply_get_argument_value(argument));
	} else if (length == 4) {
		// I have to split the quad in 2x triangles
		if (valueIndex < 0) {
			tris->push_back(Triangle());
		} else if (valueIndex < 3)
			tris->back().v[valueIndex] =
					static_cast<u_int> (ply_get_argument_value(argument));
		else if (valueIndex == 3) {
			const u_int i0 = tris->back().v[0];
			const u_int i1 = tris->back().v[2];
			const u_int i2 = static_cast<u_int> (ply_get_argument_value(argument));

			tris->push_back(Triangle(i0, i1, i2));
		}
	}

	return 1;
}

// rply uv callback
static int TriAOVCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = nullptr;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	auto triAOV = *static_cast<ExtMeshProp<float>::Layer *> (userData);

	long triAOVIndex;
	ply_get_argument_element(argument, nullptr, &triAOVIndex);

	if (userIndex == 0)
		triAOV[triAOVIndex] =
			static_cast<float>(ply_get_argument_value(argument));

	return 1;
}

ExtTriangleMeshUPtr ExtTriangleMesh::LoadPly(const string &fileName) {
	p_ply plyfile = ply_open(fileName.c_str(), nullptr);
	if (!plyfile) {
		stringstream ss;
		ss << "Unable to read PLY mesh file '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	if (!ply_read_header(plyfile)) {
		stringstream ss;
		ss << "Unable to read PLY header from '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	VertexBuffer p;
	const long plyNbVerts = ply_set_read_cb(plyfile, "vertex", "x", VertexCB, &p, 0);
	ply_set_read_cb(plyfile, "vertex", "y", VertexCB, &p, 1);
	ply_set_read_cb(plyfile, "vertex", "z", VertexCB, &p, 2);
	if (plyNbVerts <= 0) {
		stringstream ss;
		ss << "No vertices found in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	std::vector<Triangle> vi;
	const long plyNbFaces = ply_set_read_cb(plyfile, "face", "vertex_indices", FaceCB, &vi, 0);
	if (plyNbFaces <= 0) {
		stringstream ss;
		ss << "No faces found in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// Check if the file includes triaov information
	ExtMeshProp<float> TriAOVs;
	array<u_int, EXTMESH_MAX_DATA_COUNT> plyNbTriAOVs;
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		const string suffix = (i == 0) ? "" : ToString(i);

		plyNbTriAOVs[i] = ply_set_read_cb(plyfile, ("faceaov" + suffix).c_str(), "triaov", TriAOVCB, &TriAOVs[i], 0);
		if ((plyNbTriAOVs[i] > 0) && (plyNbTriAOVs[i] != plyNbFaces)) {
			stringstream ss;
			ss << "Wrong count of triangle AOV #" << i << " in '" << fileName << "'";
			throw runtime_error(ss.str());
		}
	}

	// Check if the file includes normal information
	NormalBuffer n;
	const long plyNbNormals = ply_set_read_cb(plyfile, "vertex", "nx", NormalCB, &n, 0);
	ply_set_read_cb(plyfile, "vertex", "ny", NormalCB, &n, 1);
	ply_set_read_cb(plyfile, "vertex", "nz", NormalCB, &n, 2);
	if ((plyNbNormals > 0) && (plyNbNormals != plyNbVerts)) {
		stringstream ss;
		ss << "Wrong count of normals in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// This is our own extension to file PLY format in order to support multiple
	// UVs, Colors and Alphas for each vertex

	// Output buffers

	ExtMeshProp<UV> uvs; // Vertex uvs
	ExtMeshProp<Spectrum> cols; // Vertex colors
	ExtMeshProp<float> alphas; // Vertex alphas
	ExtMeshProp<float> vertexAOVs; // Vertex AOV

	array<u_int, EXTMESH_MAX_DATA_COUNT> plyNbUVs;
	array<u_int, EXTMESH_MAX_DATA_COUNT> plyNbColors;
	array<u_int, EXTMESH_MAX_DATA_COUNT> plyNbAlphas;
	array<u_int, EXTMESH_MAX_DATA_COUNT> plyNbVertexAOVs;

	// Get output buffer sizes
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		const string suffix = (i == 0) ? "" : ToString(i);

		// Check if the file includes uv information
		plyNbUVs[i] = ply_set_read_cb(plyfile, "vertex", ("s" + suffix).c_str(), UVCB, &uvs[i], 0);
		ply_set_read_cb(plyfile, "vertex", ("t" + suffix).c_str(), UVCB, &uvs[i], 1);
		if ((plyNbUVs[i] > 0) && (plyNbUVs[i] != plyNbVerts)) {
			stringstream ss;
			ss << "Wrong count of uvs #" << i << " in '" << fileName << "'";
			throw runtime_error(ss.str());
		}

		// Check if the file includes color information
		plyNbColors[i] = ply_set_read_cb(plyfile, "vertex", ("red" + suffix).c_str(), ColorCB, &cols[i], 0);
		ply_set_read_cb(plyfile, "vertex", ("green" + suffix).c_str(), ColorCB, &cols[i], 1);
		ply_set_read_cb(plyfile, "vertex", ("blue" + suffix).c_str(), ColorCB, &cols[i], 2);
		if ((plyNbColors[i] > 0) && (plyNbColors[i] != plyNbVerts)) {
			stringstream ss;
			ss << "Wrong count of colors #" << i << " in '" << fileName << "'";
			throw runtime_error(ss.str());
		}

		// Check if the file includes alpha information
		plyNbAlphas[i] = ply_set_read_cb(plyfile, "vertex", ("alpha" + suffix).c_str(), AlphaCB, &alphas[i], 0);
		if ((plyNbAlphas[i] > 0) && (plyNbAlphas[i] != plyNbVerts)) {
			stringstream ss;
			ss << "Wrong count of alphas #" << i << " in '" << fileName << "'";
			throw runtime_error(ss.str());
		}

		// Check if the file includes vertexAOV information
		plyNbVertexAOVs[i] = ply_set_read_cb(plyfile, "vertex", ("vertaov" + suffix).c_str(), VertexAOVCB, &vertexAOVs[i], 0);
		if ((plyNbVertexAOVs[i] > 0) && (plyNbVertexAOVs[i] != plyNbVerts)) {
			stringstream ss;
			ss << "Wrong count of vertex AOV #" << i << " in '" << fileName << "'";
			throw runtime_error(ss.str());
		}
	}

	// Allocate buffers
	p.Allocate(plyNbVerts);
	if (plyNbNormals == 0)
		n = NormalBuffer();
	else
		n.Allocate(plyNbNormals);

	// Helper
	auto allocProp = [&]<typename V, typename N>(V& values, const N& numbers, u_int i) {
		if (numbers[i] == 0) {
			values[i] = nullptr;
		} else {
			values.AllocateLayer(i, numbers[i]);
		}
	};
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		allocProp(uvs, plyNbUVs, i);
		allocProp(cols, plyNbColors, i);
		allocProp(alphas, plyNbAlphas, i);
		allocProp(vertexAOVs, plyNbVertexAOVs, i);
		allocProp(TriAOVs, plyNbTriAOVs, i);
	}

	// Read data
	if (!ply_read(plyfile)) {
		stringstream ss;
		ss << "Unable to parse PLY file '" << fileName << "'";

		throw runtime_error(ss.str());
	}

	ply_close(plyfile);

	// Copy triangle indices vector
	auto tris = TriangleBuffer(vi);
	//copy(vi.begin(), vi.end(), tris);

	auto mesh = std::make_unique<ExtTriangleMesh>(std::move(p), std::move(tris), std::move(n), uvs, cols, alphas);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		//mesh->SetVertexAOV(i, vertexAOVs[i], plyNbVerts);  TODO
		mesh->SetTriAOV(i, TriAOVs[i], vi.size());
	}
	
	return mesh;
}

//------------------------------------------------------------------------------
// ExtTriangleMesh Load
//------------------------------------------------------------------------------

ExtTriangleMeshUPtr ExtTriangleMesh::Load(const string &fileName) {
	const std::filesystem::path ext = std::filesystem::path(fileName).extension();
	if (ext == ".ply")
		return LoadPly(fileName);
	else if (ext == ".bpy")
		return LoadSerialized(fileName);
	else
		throw runtime_error("Unknown file extension while loading a mesh from: " + fileName);	
}

//------------------------------------------------------------------------------
// ExtTriangleMesh Save
//------------------------------------------------------------------------------

void ExtTriangleMesh::Save(const string &fileName) const {
	const std::filesystem::path ext = std::filesystem::path(fileName).extension();
	if (ext == ".ply")
		SavePly(fileName);
	else if (ext == ".bpy")
		SaveSerialized(fileName);
	else
		throw runtime_error("Unknown file extension while saving a mesh to: " + fileName);
}

void ExtTriangleMesh::SavePly(const string &fileName) const {
	// The use of std::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	std::ofstream plyFile(std::filesystem::path(fileName),
			std::ofstream::out |
			std::ofstream::binary |
			std::ofstream::trunc);
	if(!plyFile.is_open())
		throw runtime_error("Unable to open: " + fileName);

	plyFile.imbue(cLocale);
	auto vertCount = vertices.Count();
	auto triCount = tris.Count();


	// Write the PLY header
	plyFile << "ply\n"
			"format " + string(ply_storage_mode_list[ply_arch_endian()]) + " 1.0\n"
			"comment Created by LuxRays v" LUXRAYS_VERSION "\n"
			"element vertex " << vertCount << "\n"
			"property float x\n"
			"property float y\n"
			"property float z\n";

	if (HasNormals())
		plyFile << "property float nx\n"
				"property float ny\n"
				"property float nz\n";

	// This is our own extension to file PLY format in order to support multiple
	// UVs, Colors, Alphas and Vertex AOVs for each vertex
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		const string suffix = (i == 0) ? "" : ToString(i);

		if (HasUVs(i))
			plyFile << "property float s" << suffix << "\n"
					"property float t" << suffix << "\n";

		if (HasColors(i))
			plyFile << "property float red" << suffix << "\n"
					"property float green" << suffix << "\n"
					"property float blue" << suffix << "\n";

		if (HasAlphas(i))
			plyFile << "property float alpha" << suffix << "\n";	

		if (HasVertexAOV(i))
			plyFile << "property float vertaov" << suffix << "\n";	
	}

	plyFile << "element face " << triCount << "\n"
				"property list uchar uint vertex_indices\n";

	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i) {
		const string suffix = (i == 0) ? "" : ToString(i);

		if (HasTriAOV(i))
			plyFile << "element faceaov" << suffix << " " << triCount << "\n"
					"property float triaov\n";
	}

	plyFile << "end_header\n";

	if (!plyFile.good())
		throw runtime_error("Unable to write PLY header to: " + fileName);

	// Write all vertex data
	for (size_t i = 0; i < vertCount; ++i) {
		plyFile.write((char *)&vertices[i], sizeof(Point));
		if (HasNormals())
			plyFile.write((char *)&normals[i], sizeof(Normal));

		for (size_t j = 0; j < EXTMESH_MAX_DATA_COUNT; ++j) {
			if (HasUVs(j))
				plyFile.write((char *)&uvs[j][i], sizeof(UV));
			if (HasColors(j))
				plyFile.write((char *)&cols[j][i], sizeof(Spectrum));
			if (HasAlphas(j))
				plyFile.write((char *)&alphas[j][i], sizeof(float));
			if (HasVertexAOV(j))
				plyFile.write((char *)&vertAOV[j][i], sizeof(float));
		}
	}

	if (!plyFile.good())
		throw runtime_error("Unable to write PLY vertex data to: " + fileName);

	// Write all face data
	const u_char len = 3;
	for (u_int i = 0; i < triCount; ++i) {
		plyFile.write((char *)&len, 1);
		plyFile.write((char *)&tris[i], sizeof(Triangle));
	}

	for (u_int j = 0; j < EXTMESH_MAX_DATA_COUNT; ++j) {
		if (HasTriAOV(j)) {
			for (u_int i = 0; i < triCount; ++i)
				plyFile.write((char *)&triAOV[j][i], sizeof(float));
		}
	}

	if (!plyFile.good())
		throw runtime_error("Unable to write PLY face data to: " + fileName);

	plyFile.close();
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
