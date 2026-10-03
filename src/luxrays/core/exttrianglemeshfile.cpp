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
#include <cctype>
#include <cstdint>
#include <atomic>

#include <boost/format.hpp>

#include "oneapi/tbb.h"

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

//------------------------------------------------------------------------------
// ExtMesh PLY binary fast path
//------------------------------------------------------------------------------

// The fast path of the binary PLY reading: an element of vertices (any
// fixed size properties, the x, y, z are extracted and the others are
// skipped, exactly like the rply reader below skips the properties
// without a callback) followed by an element of triangular faces (any
// fixed size properties plus the vertex_indices list, the last property
// of the element). The whole file is read at once and the two elements
// are parsed in parallel chunks, directly from the bytes - the rply
// reader walks the file through stdio and invokes one callback per
// value, one value at a time.
//
// Anything else (the ASCII format, the properties the rply path would
// extract: normals, uvs, colors, alphas, AOVs, list properties in the
// vertex element, other or missing elements, quad or polygon faces,
// truncated files) is rejected here and read by the rply path below,
// unchanged: the fast path is a strict subset and the fallback keeps
// every other file exactly as before.
//
// The conversion of every value goes through a double, like rply does
// (ply_get_argument_value returns a double, the callbacks cast it
// back), so the meshes of the fast path are identical to the ones of
// the rply path, value for value (a float32 property makes the round
// trip through the double without any change)

enum class PlyPropType {
	CHAR, UCHAR, SHORT, USHORT, INT, UINT, FLOAT, DOUBLE
};

static size_t PlyPropTypeSize(const PlyPropType t) {
	switch (t) {
		case PlyPropType::CHAR:
		case PlyPropType::UCHAR:
			return 1;
		case PlyPropType::SHORT:
		case PlyPropType::USHORT:
			return 2;
		case PlyPropType::INT:
		case PlyPropType::UINT:
		case PlyPropType::FLOAT:
			return 4;
		case PlyPropType::DOUBLE:
			return 8;
	}

	return 0;
}

// Parse the name of a PLY property type (the format has two spellings
// for several of them)
static bool PlyParsePropType(const string &name, PlyPropType &type) {
	if ((name == "char") || (name == "int8"))
		type = PlyPropType::CHAR;
	else if ((name == "uchar") || (name == "uint8"))
		type = PlyPropType::UCHAR;
	else if ((name == "short") || (name == "int16"))
		type = PlyPropType::SHORT;
	else if ((name == "ushort") || (name == "uint16"))
		type = PlyPropType::USHORT;
	else if ((name == "int") || (name == "int32"))
		type = PlyPropType::INT;
	else if ((name == "uint") || (name == "uint32"))
		type = PlyPropType::UINT;
	else if ((name == "float") || (name == "float32"))
		type = PlyPropType::FLOAT;
	else if ((name == "double") || (name == "float64"))
		type = PlyPropType::DOUBLE;
	else
		return false;

	return true;
}

// Read a little or big endian value from the raw file bytes
template<typename T>
static T PlyReadValue(const unsigned char *p, const bool bigEndian) {
	T v;
	if (bigEndian) {
		unsigned char *dst = reinterpret_cast<unsigned char *>(&v);
		for (size_t i = 0; i < sizeof(T); ++i)
			dst[i] = p[sizeof(T) - 1 - i];
	} else
		memcpy(&v, p, sizeof(T));

	return v;
}

// Read a property value: converted to a double, like
// ply_get_argument_value of rply
static double PlyPropValue(const unsigned char *p, const PlyPropType t,
		const bool bigEndian) {
	switch (t) {
		case PlyPropType::CHAR:
			return PlyReadValue<int8_t>(p, bigEndian);
		case PlyPropType::UCHAR:
			return PlyReadValue<uint8_t>(p, bigEndian);
		case PlyPropType::SHORT:
			return PlyReadValue<int16_t>(p, bigEndian);
		case PlyPropType::USHORT:
			return PlyReadValue<uint16_t>(p, bigEndian);
		case PlyPropType::INT:
			return PlyReadValue<int32_t>(p, bigEndian);
		case PlyPropType::UINT:
			return PlyReadValue<uint32_t>(p, bigEndian);
		case PlyPropType::FLOAT:
			return PlyReadValue<float>(p, bigEndian);
		case PlyPropType::DOUBLE:
			return PlyReadValue<double>(p, bigEndian);
	}

	return 0.0;
}

// A fixed size property of an element
struct PlyProperty {
	string name;
	PlyPropType type = PlyPropType::FLOAT;
	size_t offset = 0;
};

// An element of the header
struct PlyElement {
	string name;
	size_t count = 0;
	vector<PlyProperty> properties;
	bool hasList = false;
	string listName;
	PlyPropType listCountType = PlyPropType::UCHAR;
	PlyPropType listType = PlyPropType::INT;
};

// The name without the optional numeric suffix of the multi layer
// properties (s1, t1, red2, alpha3, ...)
static string PlyBaseName(const string &name) {
	size_t last = name.size();
	while ((last > 0) && isdigit(name[last - 1]))
		--last;

	return name.substr(0, last);
}

// Read the plain binary PLY meshes. Returns null when the file is
// anything the fast path does not support (the caller falls back to
// the rply reader)
static ExtTriangleMeshUPtr ReadPlyBinary(const string &fileName) {
	// Read the whole file at once: the header is parsed from the
	// buffer and the data below is read directly from the bytes
	FILE *file = fopen(fileName.c_str(), "rb");
	if (!file)
		return nullptr;

	fseek(file, 0, SEEK_END);
	const long fileSize = ftell(file);
	if (fileSize < 4) {
		fclose(file);
		return nullptr;
	}
	rewind(file);

	vector<unsigned char> data(fileSize);
	if (fread(data.data(), 1, fileSize, file) != size_t(fileSize)) {
		fclose(file);
		return nullptr;
	}
	fclose(file);

	if (memcmp(data.data(), "ply\n", 4) != 0)
		return nullptr;

	// Parse the header: the elements with their properties, and the
	// binary format. The comments and every other line are skipped,
	// like the rply reader ignores them
	bool bigEndian = false;
	vector<PlyElement> elements;
	size_t dataStart = data.size();
	{
		size_t pos = 4;
		bool binary = false;
		bool headerDone = false;
		while (!headerDone) {
			if (pos >= data.size())
				return nullptr;

			const unsigned char *lineEnd = static_cast<const unsigned char *>(
					memchr(&data[pos], '\n', data.size() - pos));
			if (!lineEnd)
				return nullptr;

			const string line(reinterpret_cast<const char *>(&data[pos]),
					size_t(lineEnd - &data[pos]));
			pos = size_t(lineEnd - data.data()) + 1;

			istringstream ss(line);
			string keyword;
			ss >> keyword;

			if (keyword == "format") {
				string format, version;
				ss >> format >> version;
				if (format == "binary_big_endian") {
					binary = true;
					bigEndian = true;
				} else if (format == "binary_little_endian")
					binary = true;
				else
					return nullptr;
			} else if (keyword == "element") {
				PlyElement element;
				string name, count;
				ss >> name >> count;
				element.name = name;
				element.count = strtoull(count.c_str(), nullptr, 10);
				elements.push_back(std::move(element));
			} else if (keyword == "property") {
				if (elements.empty())
					return nullptr;

				PlyElement &element = elements.back();
				string type1;
				ss >> type1;
				if (type1 == "list") {
					string countType, itemType, name;
					ss >> countType >> itemType >> name;
					if (!ss || element.hasList ||
							!PlyParsePropType(countType, element.listCountType) ||
							!PlyParsePropType(itemType, element.listType))
						return nullptr;
					element.hasList = true;
					element.listName = name;
				} else {
					string name;
					ss >> name;
					PlyProperty property;
					if (!ss || !PlyParsePropType(type1, property.type))
						return nullptr;
					property.name = name;
					for (const PlyProperty &p : element.properties)
						property.offset += PlyPropTypeSize(p.type);
					element.properties.push_back(std::move(property));
				}
			} else if (keyword == "end_header") {
				headerDone = true;
				dataStart = pos;
			}
		}

		if (!binary)
			return nullptr;
	}

	// The fast path supports only the plain meshes: one vertex element
	// followed by one face element, and nothing the rply path would
	// extract beyond the positions and the triangle indices
	if ((elements.size() != 2) ||
			(elements[0].name != "vertex") || elements[0].hasList || (elements[0].count == 0) ||
			(elements[1].name != "face") || (elements[1].count == 0) ||
			!elements[1].hasList || (elements[1].listName != "vertex_indices"))
		return nullptr;

	const PlyElement &vertexElement = elements[0];
	const PlyElement &faceElement = elements[1];

	// The vertex element must not carry anything the rply path would
	// load: the fast path reads only the positions
	for (const PlyProperty &property : vertexElement.properties) {
		const string base = PlyBaseName(property.name);
		if ((base == "nx") || (base == "ny") || (base == "nz") ||
				(base == "s") || (base == "t") ||
				(base == "red") || (base == "green") || (base == "blue") ||
				(base == "alpha") || (base == "vertaov"))
			return nullptr;
	}

	// The face element must not carry the triangle AOVs either
	for (const PlyProperty &property : faceElement.properties) {
		if (PlyBaseName(property.name) == "triaov")
			return nullptr;
	}

	// The vertex positions
	size_t xOffset = SIZE_MAX, yOffset = SIZE_MAX, zOffset = SIZE_MAX;
	PlyPropType xType = PlyPropType::FLOAT, yType = PlyPropType::FLOAT, zType = PlyPropType::FLOAT;
	for (const PlyProperty &property : vertexElement.properties) {
		if (property.name == "x") {
			xOffset = property.offset;
			xType = property.type;
		} else if (property.name == "y") {
			yOffset = property.offset;
			yType = property.type;
		} else if (property.name == "z") {
			zOffset = property.offset;
			zType = property.type;
		}
	}
	if ((xOffset == SIZE_MAX) || (yOffset == SIZE_MAX) || (zOffset == SIZE_MAX))
		return nullptr;

	// The record sizes
	size_t vertexStride = 0;
	for (const PlyProperty &property : vertexElement.properties)
		vertexStride += PlyPropTypeSize(property.type);
	size_t facePrefix = 0;
	for (const PlyProperty &property : faceElement.properties)
		facePrefix += PlyPropTypeSize(property.type);
	const size_t faceCountSize = PlyPropTypeSize(faceElement.listCountType);
	const size_t faceItemSize = PlyPropTypeSize(faceElement.listType);

	// The face records are uniform only when every face is a triangle
	// (verified while reading: a different count aborts the fast path
	// and the rply path below reads the file)
	const size_t faceStride = facePrefix + faceCountSize + 3 * faceItemSize;

	// The data must fit in the file
	const size_t vertexBytes = vertexElement.count * vertexStride;
	const size_t faceBytes = faceElement.count * faceStride;
	if ((dataStart >= data.size()) ||
			(data.size() - dataStart < vertexBytes + faceBytes))
		return nullptr;

	// Read the vertices: one parallel pass over the records
	VertexBuffer p;
	p.Allocate(vertexElement.count);
	{
		const std::span<Point> points = p.GetObjects();
		const unsigned char * const vertexData = data.data() + dataStart;
		tbb::parallel_for(tbb::blocked_range<size_t>(0, vertexElement.count, 16384),
			[&](const tbb::blocked_range<size_t> &r) {
				for (size_t i = r.begin(); i < r.end(); ++i) {
					const unsigned char * const record = vertexData + i * vertexStride;
					points[i] = Point(
						static_cast<float>(PlyPropValue(record + xOffset, xType, bigEndian)),
						static_cast<float>(PlyPropValue(record + yOffset, yType, bigEndian)),
						static_cast<float>(PlyPropValue(record + zOffset, zType, bigEndian)));
				}
			});
	}

	// Read the faces: one parallel pass with the triangle assumption
	// verified per record
	TriangleBuffer tris(faceElement.count);
	std::atomic<bool> notTriangles(false);
	{
		const std::span<Triangle> triangles = tris.GetObjects();
		const unsigned char * const faceData = data.data() + dataStart + vertexBytes;
		tbb::parallel_for(tbb::blocked_range<size_t>(0, faceElement.count, 16384),
			[&](const tbb::blocked_range<size_t> &r) {
				if (notTriangles.load(std::memory_order_relaxed))
					return;

				for (size_t i = r.begin(); i < r.end(); ++i) {
					const unsigned char * const record = faceData + i * faceStride;
					if (PlyPropValue(record + facePrefix, faceElement.listCountType, bigEndian) != 3.0) {
						notTriangles.store(true, std::memory_order_relaxed);
						return;
					}

					const unsigned char * const indices = record + facePrefix + faceCountSize;
					triangles[i] = Triangle(
						static_cast<u_int>(PlyPropValue(indices, faceElement.listType, bigEndian)),
						static_cast<u_int>(PlyPropValue(indices + faceItemSize, faceElement.listType, bigEndian)),
						static_cast<u_int>(PlyPropValue(indices + 2 * faceItemSize, faceElement.listType, bigEndian)));
				}
			});

		if (notTriangles.load(std::memory_order_relaxed))
			return nullptr;
	}

	// The mesh of the fast path carries no normals, uvs, colors,
	// alphas or AOVs: the header checks above rejected the files
	// carrying them
	NormalBuffer n;
	ExtMeshProp<UV> uvs;
	ExtMeshProp<Spectrum> cols;
	ExtMeshProp<float> alphas;
	ExtMeshProp<float> triAOVs;
	auto mesh = std::make_unique<ExtTriangleMesh>(std::move(p), std::move(tris),
			std::move(n), uvs, cols, alphas);
	for (u_int i = 0; i < EXTMESH_MAX_DATA_COUNT; ++i)
		mesh->SetTriAOV(i, triAOVs[i], faceElement.count);

	return mesh;
}

ExtTriangleMeshUPtr ExtTriangleMesh::LoadPly(const string &fileName) {
	// The fast path of the plain binary PLY meshes (see ReadPlyBinary):
	// anything it does not support returns null and the rply reader
	// below handles the file, unchanged
	if (ExtTriangleMeshUPtr fastMesh = ReadPlyBinary(fileName))
		return fastMesh;

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
