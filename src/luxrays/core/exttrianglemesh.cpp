/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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
#include <boost/filesystem.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>

#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/utils/ply/rply.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// ExtMesh
//------------------------------------------------------------------------------

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtMesh)

void ExtMesh::GetDifferentials(const float time, const u_int triIndex, const Normal &shadeNormal,
        Vector *dpdu, Vector *dpdv,
        Normal *dndu, Normal *dndv) const {
    // Compute triangle partial derivatives
    const Triangle &tri = GetTriangles()[triIndex];
    UV uv0, uv1, uv2;
    if (HasUVs()) {
        uv0 = GetUV(tri.v[0]);
        uv1 = GetUV(tri.v[1]);
        uv2 = GetUV(tri.v[2]);
    } else {
		uv0 = UV(.5f, .5f);
		uv1 = UV(.5f, .5f);
		uv2 = UV(.5f, .5f);
	}

    // Compute deltas for triangle partial derivatives
	const float du1 = uv0.u - uv2.u;
	const float du2 = uv1.u - uv2.u;
	const float dv1 = uv0.v - uv2.v;
	const float dv2 = uv1.v - uv2.v;
	const float determinant = du1 * dv2 - dv1 * du2;

	if (determinant == 0.f) {
		// Handle 0 determinant for triangle partial derivative matrix
		CoordinateSystem(Vector(shadeNormal), dpdu, dpdv);
		*dndu = Normal();
		*dndv = Normal();
	} else {
		const float invdet = 1.f / determinant;

		// Using GetVertex() in order to do all computation relative to
		// the global coordinate system.
		const Point p0 = GetVertex(time, tri.v[0]);
		const Point p1 = GetVertex(time, tri.v[1]);
		const Point p2 = GetVertex(time, tri.v[2]);

		const Vector dp1 = p0 - p2;
		const Vector dp2 = p1 - p2;

		const Vector geometryDpDu = ( dv2 * dp1 - dv1 * dp2) * invdet;
		const Vector geometryDpDv = (-du2 * dp1 + du1 * dp2) * invdet;

		*dpdu = Cross(shadeNormal, Cross(geometryDpDu, shadeNormal));
		*dpdv = Cross(shadeNormal, Cross(geometryDpDv, shadeNormal));

		if (HasNormals()) {
			// Using GetShadeNormal() in order to do all computation relative to
			// the global coordinate system.
			const Normal n0 = GetShadeNormal(time, tri.v[0]);
			const Normal n1 = GetShadeNormal(time, tri.v[1]);
			const Normal n2 = GetShadeNormal(time, tri.v[2]);

			const Normal dn1 = n0 - n2;
			const Normal dn2 = n1 - n2;
			*dndu = ( dv2 * dn1 - dv1 * dn2) * invdet;
			*dndv = (-du2 * dn1 + du1 * dn2) * invdet;
		} else {
			*dndu = Normal();
			*dndv = Normal();
		}
	}
}

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

// rply color callback
static int ColorCB(p_ply_argument argument) {
	long userIndex = 0;
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	float *c = *static_cast<float **> (userData);

	long colIndex;
	ply_get_argument_element(argument, NULL, &colIndex);

	// Check the type of value used
	p_ply_property property = NULL;
	ply_get_argument_property(argument, &property, NULL, NULL);
	e_ply_type dataType;
	ply_get_property_info(property, NULL, &dataType, NULL, NULL);
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
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, &userIndex);

	float *c = *static_cast<float **> (userData);

	long alphaIndex;
	ply_get_argument_element(argument, NULL, &alphaIndex);

	// Check the type of value used
	p_ply_property property = NULL;
	ply_get_argument_property(argument, &property, NULL, NULL);
	e_ply_type dataType;
	ply_get_property_info(property, NULL, &dataType, NULL, NULL);
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
	void *userData = NULL;
	ply_get_argument_user_data(argument, &userData, NULL);

	vector<Triangle> *tris = static_cast<vector<Triangle> *> (userData);

	long length, valueIndex;
	ply_get_argument_property(argument, NULL, &length, &valueIndex);

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

ExtTriangleMesh *ExtTriangleMesh::Load(const string &fileName) {
	const boost::filesystem::path ext = boost::filesystem::path(fileName).extension();
	if (ext == ".ply")
		return LoadPly(fileName);
	else if (ext == ".bpy")
		return LoadSerialized(fileName);
	else
		throw runtime_error("Unknown file extension while loading a mesh from: " + fileName);	
}

ExtTriangleMesh *ExtTriangleMesh::LoadPly(const string &fileName) {
	p_ply plyfile = ply_open(fileName.c_str(), NULL);
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

	Point *p;
	const long plyNbVerts = ply_set_read_cb(plyfile, "vertex", "x", VertexCB, &p, 0);
	ply_set_read_cb(plyfile, "vertex", "y", VertexCB, &p, 1);
	ply_set_read_cb(plyfile, "vertex", "z", VertexCB, &p, 2);
	if (plyNbVerts <= 0) {
		stringstream ss;
		ss << "No vertices found in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	vector<Triangle> vi;
	const long plyNbFaces = ply_set_read_cb(plyfile, "face", "vertex_indices", FaceCB, &vi, 0);
	if (plyNbFaces <= 0) {
		stringstream ss;
		ss << "No faces found in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// Check if the file includes normal informations
	Normal *n;
	const long plyNbNormals = ply_set_read_cb(plyfile, "vertex", "nx", NormalCB, &n, 0);
	ply_set_read_cb(plyfile, "vertex", "ny", NormalCB, &n, 1);
	ply_set_read_cb(plyfile, "vertex", "nz", NormalCB, &n, 2);
	if ((plyNbNormals > 0) && (plyNbNormals != plyNbVerts)) {
		stringstream ss;
		ss << "Wrong count of normals in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// Check if the file includes uv informations
	UV *uv;
	const long plyNbUVs = ply_set_read_cb(plyfile, "vertex", "s", UVCB, &uv, 0);
	ply_set_read_cb(plyfile, "vertex", "t", UVCB, &uv, 1);
	if ((plyNbUVs > 0) && (plyNbUVs != plyNbVerts)) {
		stringstream ss;
		ss << "Wrong count of uvs in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// Check if the file includes color informations
	Spectrum *cols;
	const long plyNbColors = ply_set_read_cb(plyfile, "vertex", "red", ColorCB, &cols, 0);
	ply_set_read_cb(plyfile, "vertex", "green", ColorCB, &cols, 1);
	ply_set_read_cb(plyfile, "vertex", "blue", ColorCB, &cols, 2);
	if ((plyNbColors > 0) && (plyNbColors != plyNbVerts)) {
		stringstream ss;
		ss << "Wrong count of colors in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	// Check if the file includes alpha informations
	float *alphas;
	const long plyNbAlphas = ply_set_read_cb(plyfile, "vertex", "alpha", AlphaCB, &alphas, 0);
	if ((plyNbAlphas > 0) && (plyNbAlphas != plyNbVerts)) {
		stringstream ss;
		ss << "Wrong count of alphas in '" << fileName << "'";
		throw runtime_error(ss.str());
	}

	p = TriangleMesh::AllocVerticesBuffer(plyNbVerts);
	if (plyNbNormals == 0)
		n = NULL;
	else
		n = new Normal[plyNbNormals];
	if (plyNbUVs == 0)
		uv = NULL;
	else
		uv = new UV[plyNbUVs];
	if (plyNbColors == 0)
		cols = NULL;
	else
		cols = new Spectrum[plyNbColors];
	if (plyNbAlphas == 0)
		alphas = NULL;
	else
		alphas = new float[plyNbAlphas];

	if (!ply_read(plyfile)) {
		stringstream ss;
		ss << "Unable to parse PLY file '" << fileName << "'";

		delete[] p;
		delete[] n;
		delete[] uv;
		delete[] cols;
		delete[] alphas;

		throw runtime_error(ss.str());
	}

	ply_close(plyfile);

	// Copy triangle indices vector
	Triangle *tris = TriangleMesh::AllocTrianglesBuffer(vi.size());
	copy(vi.begin(), vi.end(), tris);

	return new ExtTriangleMesh(plyNbVerts, vi.size(), p, tris, n, uv, cols, alphas);
}

ExtTriangleMesh *ExtTriangleMesh::LoadSerialized(const string &fileName) {
	BOOST_IFSTREAM inFile;
	inFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	inFile.open(fileName.c_str(), BOOST_IFSTREAM::binary);

	// Create an input filtering stream
	boost::iostreams::filtering_istream inStream;

	// Enable compression
	inStream.push(boost::iostreams::gzip_decompressor());
	inStream.push(inFile);

	// Use portable archive
	eos::polymorphic_portable_iarchive inArchive(inStream);
	//boost::archive::binary_iarchive inArchive(inStream);

	ExtTriangleMesh *mesh;
	inArchive >> mesh;

	if (!inStream.good())
		throw runtime_error("Error while loading serialized scene: " + fileName);

	return mesh;
}

//------------------------------------------------------------------------------
// ExtTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::TriangleMesh, luxrays::ExtTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtTriangleMesh)

ExtTriangleMesh::ExtTriangleMesh(const u_int meshVertCount, const u_int meshTriCount,
		Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV,
			Spectrum *meshCols, float *meshAlpha) :
		TriangleMesh(meshVertCount, meshTriCount, meshVertices, meshTris) {
	normals = meshNormals;
	uvs = meshUV;
	cols = meshCols;
	alphas = meshAlpha;

	triNormals = new Normal[triCount];
	Preprocess();
}

void ExtTriangleMesh::Preprocess() {
	// Compute all triangle normals and mesh area
	area = 0.f;
	for (u_int i = 0; i < triCount; ++i) {
		triNormals[i] = tris[i].GetGeometryNormal(vertices);
		area += tris[i].Area(vertices);
	}
}

Normal *ExtTriangleMesh::ComputeNormals() {
	bool allocated;
	if (!normals) {
		allocated = true;
		normals = new Normal[vertCount];
	} else
		allocated = false;

	for (u_int i = 0; i < vertCount; ++i)
		normals[i] = Normal(0.f, 0.f, 0.f);
	for (u_int i = 0; i < triCount; ++i) {
		const Vector e1 = vertices[tris[i].v[1]] - vertices[tris[i].v[0]];
		const Vector e2 = vertices[tris[i].v[2]] - vertices[tris[i].v[0]];
		const Normal N = Normal(Normalize(Cross(e1, e2)));
		normals[tris[i].v[0]] += N;
		normals[tris[i].v[1]] += N;
		normals[tris[i].v[2]] += N;
	}
	//int printedWarning = 0;
	for (u_int i = 0; i < vertCount; ++i) {
		normals[i] = Normalize(normals[i]);
		// Check for degenerate triangles/normals, they can freeze the GPU
		if (isnan(normals[i].x) || isnan(normals[i].y) || isnan(normals[i].z)) {
			/*if (printedWarning < 15) {
				SDL_LOG("The model contains a degenerate normal (index " << i << ")");
				++printedWarning;
			} else if (printedWarning == 15) {
				SDL_LOG("The model contains more degenerate normals");
				++printedWarning;
			}*/
			normals[i] = Normal(0.f, 0.f, 1.f);
		}
	}

	return allocated ? normals : NULL;
}

void ExtTriangleMesh::ApplyTransform(const Transform &trans) {
	TriangleMesh::ApplyTransform(trans);

	if (normals) {
		for (u_int i = 0; i < vertCount; ++i) {
			normals[i] *= trans;
			normals[i] = Normalize(normals[i]);
		}
	}

	Preprocess();
}

void ExtTriangleMesh::Save(const string &fileName) const {
	const boost::filesystem::path ext = boost::filesystem::path(fileName).extension();
	if (ext == ".ply")
		SavePly(fileName);
	else if (ext == ".bpy")
		SaveSerialized(fileName);
	else
		throw runtime_error("Unknown file extension while saving a mesh to: " + fileName);
}

void ExtTriangleMesh::SavePly(const string &fileName) const {
	BOOST_OFSTREAM plyFile(fileName.c_str(), ofstream::out | ofstream::binary | ofstream::trunc);
	if(!plyFile.is_open())
		throw runtime_error("Unable to open: " + fileName);

	// Write the PLY header
	plyFile << "ply\n"
			"format " + string(ply_storage_mode_list[ply_arch_endian()]) + " 1.0\n"
			"comment Created by LuxRays v" LUXRAYS_VERSION_MAJOR "." LUXRAYS_VERSION_MINOR "\n"
			"element vertex " + boost::lexical_cast<string>(vertCount) + "\n"
			"property float x\n"
			"property float y\n"
			"property float z\n";

	if (HasNormals())
		plyFile << "property float nx\n"
				"property float ny\n"
				"property float nz\n";

	if (HasUVs())
		plyFile << "property float s\n"
				"property float t\n";

	if (HasColors())
		plyFile << "property float red\n"
				"property float green\n"
				"property float blue\n";

	if (HasAlphas())
		plyFile << "property float alpha\n";

	plyFile << "element face " + boost::lexical_cast<string>(triCount) + "\n"
				"property list uchar uint vertex_indices\n"
				"end_header\n";

	if (!plyFile.good())
		throw runtime_error("Unable to write PLY header to: " + fileName);

	// Write all vertex data
	for (u_int i = 0; i < vertCount; ++i) {
		plyFile.write((char *)&vertices[i], sizeof(Point));
		if (HasNormals())
			plyFile.write((char *)&normals[i], sizeof(Normal));
		if (HasUVs())
			plyFile.write((char *)&uvs[i], sizeof(UV));
		if (HasColors())
			plyFile.write((char *)&cols[i], sizeof(Spectrum));
		if (HasAlphas())
			plyFile.write((char *)&alphas[i], sizeof(float));
	}
	if (!plyFile.good())
		throw runtime_error("Unable to write PLY vertex data to: " + fileName);

	// Write all face data
	const u_char len = 3;
	for (u_int i = 0; i < triCount; ++i) {
		plyFile.write((char *)&len, 1);
		plyFile.write((char *)&tris[i], sizeof(Triangle));
	}
	if (!plyFile.good())
		throw runtime_error("Unable to write PLY face data to: " + fileName);

	plyFile.close();
}

void ExtTriangleMesh::SaveSerialized(const string &fileName) const {
	// Serialize the mesh
	BOOST_OFSTREAM outFile;
	outFile.exceptions(ofstream::failbit | ofstream::badbit | ofstream::eofbit);
	outFile.open(fileName.c_str(), BOOST_OFSTREAM::binary);

	// Enable compression
	boost::iostreams::filtering_ostream outStream;
	outStream.push(boost::iostreams::gzip_compressor(4));
	outStream.push(outFile);

	// Use portable archive
	eos::polymorphic_portable_oarchive outArchive(outStream);
	//boost::archive::binary_oarchive outArchive(outStream);

	outArchive << this;

	if (!outStream.good())
		throw runtime_error("Error while saving serialized mesh: " + fileName);

	flush(outStream);
}

ExtTriangleMesh *ExtTriangleMesh::Copy(Point *meshVertices, Triangle *meshTris, Normal *meshNormals, UV *meshUV,
			Spectrum *meshCols, float *meshAlpha) const {
	Point *vs = meshVertices;
	if (!vs) {
		vs = AllocVerticesBuffer(vertCount);
		copy(vertices, vertices + vertCount, vs);
	}

	Triangle *ts = meshTris;
	if (!ts) {
		ts = AllocTrianglesBuffer(triCount);
		copy(tris, tris + triCount, ts);
	}

	Normal *ns = meshNormals;
	if (!ns && HasNormals()) {
		ns = new Normal[vertCount];
		copy(normals, normals + vertCount, ns);
	}

	UV *us = meshUV;
	if (!us && HasUVs()) {
		us = new UV[vertCount];
		copy(uvs, uvs + vertCount, us);
	}

	Spectrum *cs = meshCols;
	if (!cs && HasColors()) {
		cs = new Spectrum[vertCount];
		copy(cols, cols + vertCount, cs);
	}
	
	float *as = meshAlpha;
	if (!as && HasAlphas()) {
		as = new float[vertCount];
		copy(alphas, alphas + vertCount, as);
	}

	return new ExtTriangleMesh(vertCount, triCount, vs, ts, ns, us, cs, as);
}

//------------------------------------------------------------------------------
// ExtInstanceTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::InstanceTriangleMesh, luxrays::ExtInstanceTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtInstanceTriangleMesh)

//------------------------------------------------------------------------------
// ExtMotionTriangleMesh
//------------------------------------------------------------------------------

// This is a workaround to a GCC bug described here:
//  https://svn.boost.org/trac10/ticket/3730
//  https://marc.info/?l=boost&m=126496738227673&w=2
namespace boost{
template<>
struct is_virtual_base_of<luxrays::MotionTriangleMesh, luxrays::ExtMotionTriangleMesh>: public mpl::true_ {};
}

BOOST_CLASS_EXPORT_IMPLEMENT(luxrays::ExtMotionTriangleMesh)
