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

#ifdef WIN32
// Windows seems to require this #define otherwise VisualC++ looks for
// Boost Python DLL symbols.
// Do not use for Unix(s), it makes some symbol local.
#define BOOST_PYTHON_STATIC_LIB
#endif

#include <vector>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/format.hpp>
#include <boost/python.hpp>

#include <Python.h>

#include <luxcore/luxcore.h>
#include <luxcore/pyluxcore/pyluxcoreforblender.h>

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace boost::python;

namespace luxcore {
namespace blender {

//------------------------------------------------------------------------------
// Blender definitions and structures
//------------------------------------------------------------------------------

static const int ME_SMOOTH = 1;

struct MFace {
	u_int v[4];
	short mat_nr;
	char edcode, flag;
};

struct MVert {
	float co[3];
	short no[3];
	char flag, bweight;
};

struct MCol {
	u_char a, r, g, b;
};

struct MTFace {
	float uv[4][2];
	void *tpage;
	char flag, transp;
	short mode, tile, unwrap;
};

//------------------------------------------------------------------------------

void ConvertFilmChannelOutput_3xFloat_To_4xUChar(const u_int width, const u_int height,
		boost::python::object &objSrc, boost::python::object &objDst, const bool normalize) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}
	if (!PyObject_CheckBuffer(objDst.ptr())) {
		const string objType = extract<string>((objDst.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in destination object of ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}
	
	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}	
	Py_buffer dstView;
	if (PyObject_GetBuffer(objDst.ptr(), &dstView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}

	if (srcView.len / (3 * 4) != dstView.len / 4)
		throw runtime_error("Wrong buffer size in ConvertFilmChannelOutput_3xFloat_To_4xUChar()");

	const float *src = (float *)srcView.buf;
	unsigned char *dst = (unsigned char *)dstView.buf;

	if (normalize) {
		// Look for the max. in source buffer

		float maxValue = 0.f;
		for (u_int i = 0; i < width * height * 3; ++i) {
			const float value = src[i];
			if (!isinf(value) && !isnan(value) && (value > maxValue))
				maxValue = value;
		}
		const float k = (maxValue == 0.f) ? 0.f : (255.f / maxValue);

		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = (height - y - 1) * width * 3;
			u_int dstIndex = y * width * 4;

			for (u_int x = 0; x < width; ++x) {
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex + 2] * k + .5f));
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex + 1] * k + .5f));
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex] * k + .5f));
				dst[dstIndex++] = 0xff;
				srcIndex += 3;
			}
		}
	} else {
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = (height - y - 1) * width * 3;
			u_int dstIndex = y * width * 4;

			for (u_int x = 0; x < width; ++x) {
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex + 2] * 255.f + .5f));
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex + 1] * 255.f + .5f));
				dst[dstIndex++] = (unsigned char)floor((src[srcIndex] * 255.f + .5f));
				dst[dstIndex++] = 0xff;
				srcIndex += 3;
			}
		}
	}
}

boost::python::list ConvertFilmChannelOutput_3xFloat_To_3xFloatList(const u_int width, const u_int height,
		boost::python::object &objSrc) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_3xFloat_To_3xFloatList(): " + objType);
	}
	
	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_3xFloatList(): " + objType);
	}	

	const float *src = (float *)srcView.buf;
	boost::python::list l;

	for (u_int y = 0; y < height; ++y) {
		u_int srcIndex = y * width * 3;

		for (u_int x = 0; x < width; ++x) {
			l.append(boost::python::make_tuple(src[srcIndex], src[srcIndex + 1], src[srcIndex + 2], 1.f));
			srcIndex += 3;
		}
	}

	return l;
}

boost::python::list ConvertFilmChannelOutput_1xFloat_To_4xFloatList(const u_int width, const u_int height,
		boost::python::object &objSrc, const bool normalize) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_1xFloat_To_4xFloatList(): " + objType);
	}
	
	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_1xFloat_To_4xFloatList(): " + objType);
	}	

	const float *src = (float *)srcView.buf;
	boost::python::list l;

	if (normalize) {
		// Look for the max. in source buffer

		float maxValue = 0.f;
		for (u_int i = 0; i < width * height; ++i) {
			const float value = src[i];
			if (!isinf(value) && !isnan(value) && (value > maxValue))
				maxValue = value;
		}
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);

		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width;

			for (u_int x = 0; x < width; ++x) {
				const float val = src[srcIndex++] * k;
				l.append(val);
				l.append(val);
				l.append(val);
				l.append(1.f);
			}
		}
	} else {
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width;

			for (u_int x = 0; x < width; ++x) {
				const float val = src[srcIndex++];
				l.append(val);
				l.append(val);
				l.append(val);
				l.append(1.f);
			}
		}
	}

	return l;
}

boost::python::list ConvertFilmChannelOutput_2xFloat_To_4xFloatList(const u_int width, const u_int height,
		boost::python::object &objSrc, const bool normalize) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_2xFloat_To_4xFloatList(): " + objType);
	}
	
	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_2xFloat_To_4xFloatList(): " + objType);
	}	

	const float *src = (float *)srcView.buf;
	boost::python::list l;

	if (normalize) {
		// Look for the max. in source buffer

		float maxValue = 0.f;
		for (u_int i = 0; i < width * height * 2; ++i) {
			const float value = src[i];
			if (!isinf(value) && !isnan(value) && (value > maxValue))
				maxValue = value;
		}
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);

		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * 2;

			for (u_int x = 0; x < width; ++x) {
				l.append(src[srcIndex++] * k);
				l.append(src[srcIndex++] * k);
				l.append(0.f);
				l.append(1.f);
			}
		}
	} else {
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * 2;

			for (u_int x = 0; x < width; ++x) {
				l.append(src[srcIndex++]);
				l.append(src[srcIndex++]);
				l.append(0.f);
				l.append(1.f);
			}
		}
	}

	return l;
}

boost::python::list ConvertFilmChannelOutput_3xFloat_To_4xFloatList(const u_int width, const u_int height,
		boost::python::object &objSrc, const bool normalize) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_3xFloat_To_4xFloatList(): " + objType);
	}
	
	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xFloatList(): " + objType);
	}	

	const float *src = (float *)srcView.buf;
	boost::python::list l;

	if (normalize) {
		// Look for the max. in source buffer

		float maxValue = 0.f;
		for (u_int i = 0; i < width * height * 3; ++i) {
			const float value = src[i];
			if (!isinf(value) && !isnan(value) && (value > maxValue))
				maxValue = value;
		}
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);

		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * 3;

			for (u_int x = 0; x < width; ++x) {
				l.append(src[srcIndex++] * k);
				l.append(src[srcIndex++] * k);
				l.append(src[srcIndex++] * k);
				l.append(1.f);
			}
		}
	} else {
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * 3;

			for (u_int x = 0; x < width; ++x) {
				l.append(src[srcIndex++]);
				l.append(src[srcIndex++]);
				l.append(src[srcIndex++]);
				l.append(1.f);
			}
		}
	}

	return l;
}

static void Scene_DefineBlenderMesh(Scene *scene, const string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr, const short matIndex) {
	MFace *blenderFaces = reinterpret_cast<MFace *>(blenderFacesPtr);
	MVert *blenderFVertices = reinterpret_cast<MVert *>(blenderVerticesPtr);
	MTFace *blenderUVs = reinterpret_cast<MTFace *>(blenderUVsPtr);
	MCol *blenderCols = reinterpret_cast<MCol *>(blenderColsPtr);

	const float normalScale = 1.f / 32767.f;
	const float rgbScale = 1.0f / 255.0f;
	u_int vertFreeIndex = 0;
	boost::unordered_map<u_int, u_int> vertexMap;
	vector<Point> tmpMeshVerts;
	vector<Normal> tmpMeshNorms;
	vector<UV> tmpMeshUVs;
	vector<Spectrum> tmpMeshCols;
	vector<Triangle> tmpMeshTris;
	for (u_int faceIndex = 0; faceIndex < blenderFaceCount; ++faceIndex) {
		const MFace &face = blenderFaces[faceIndex];

		// Is a face with the selected material ?
		if (face.mat_nr == matIndex) {
			const bool triangle = (face.v[3] == 0);
			const u_int nVertices = triangle ? 3 : 4;

			u_int vertIndices[4];
			if (face.flag & ME_SMOOTH) {
				//--------------------------------------------------------------
				// Use the Blender vertex normal
				//--------------------------------------------------------------

				for (u_int j = 0; j < nVertices; ++j) {
					const u_int index = face.v[j];
					const MVert &vertex = blenderFVertices[index];

					// Check if it has been already defined
					if (vertexMap.find(index) == vertexMap.end()) {
						// Add the vertex
						tmpMeshVerts.push_back(Point(vertex.co[0], vertex.co[1], vertex.co[2]));
						// Add the normal
						tmpMeshNorms.push_back(Normalize(Normal(
							vertex.no[0] * normalScale,
							vertex.no[1] * normalScale,
							vertex.no[2] * normalScale)));
						// Add the UV
						if (blenderUVs)
							tmpMeshUVs.push_back(UV(blenderUVs[faceIndex].uv[j]));
						// Add the color
						if (blenderCols)
							tmpMeshCols.push_back(Spectrum(
								blenderCols[faceIndex].r * rgbScale,
								blenderCols[faceIndex].g * rgbScale,
								blenderCols[faceIndex].b * rgbScale));

						// Add the vertex mapping
						const u_int vertIndex = vertFreeIndex++;
						vertexMap[index] = vertIndex;
						vertIndices[j] = vertIndex;
					} else
						vertIndices[j] = vertexMap[index];
				}
			} else {
				//--------------------------------------------------------------
				// Use the Blender face normal
				//--------------------------------------------------------------

				const Point p0(blenderFVertices[face.v[0]].co[0], blenderFVertices[face.v[0]].co[1], blenderFVertices[face.v[0]].co[2]);
				const Point p1(blenderFVertices[face.v[1]].co[0], blenderFVertices[face.v[1]].co[1], blenderFVertices[face.v[1]].co[2]);
				const Point p2(blenderFVertices[face.v[2]].co[0], blenderFVertices[face.v[2]].co[1], blenderFVertices[face.v[2]].co[2]);

				const Vector e1 = p1 - p0;
				const Vector e2 = p2 - p0;
				Normal faceNormal(Cross(e1, e2));

				if ((faceNormal.x != 0.f) || (faceNormal.y != 0.f) || (faceNormal.z != 0.f))
					faceNormal /= faceNormal.Length();

				for (u_int j = 0; j < nVertices; ++j) {
					const u_int index = face.v[j];
					const MVert &vertex = blenderFVertices[index];

					// Add the vertex
					tmpMeshVerts.push_back(Point(vertex.co[0], vertex.co[1], vertex.co[2]));
					// Add the normal
					tmpMeshNorms.push_back(faceNormal);
					// Add UV
					if (blenderUVs)
						tmpMeshUVs.push_back(UV(blenderUVs[faceIndex].uv[j]));
					// Add the color
					if (blenderCols)
						tmpMeshCols.push_back(Spectrum(
							blenderCols[faceIndex].r * rgbScale,
							blenderCols[faceIndex].g * rgbScale,
							blenderCols[faceIndex].b * rgbScale));

					vertIndices[j] = vertFreeIndex++;
				}
			}

			tmpMeshTris.push_back(Triangle(vertIndices[0], vertIndices[1], vertIndices[2]));
			if (!triangle)
				tmpMeshTris.push_back(Triangle(vertIndices[0], vertIndices[2], vertIndices[3]));
		}
	}

	//cout << "meshTriCount = " << tmpMeshTris.size() << "\n";
	//cout << "meshVertCount = " << tmpMeshVerts.size() << "\n";

	// Check if there wasn't any triangles with matIndex
	if (tmpMeshTris.size() == 0)
		return;

	// Allocate memory for LuxCore mesh data
	Triangle *meshTris = TriangleMesh::AllocTrianglesBuffer(tmpMeshTris.size());
	copy(tmpMeshTris.begin(), tmpMeshTris.end(), meshTris);

	Point *meshVerts = TriangleMesh::AllocVerticesBuffer(tmpMeshVerts.size());
	copy(tmpMeshVerts.begin(), tmpMeshVerts.end(), meshVerts);

	Normal *meshNorms = new Normal[tmpMeshVerts.size()];
	copy(tmpMeshNorms.begin(), tmpMeshNorms.end(), meshNorms);

	UV *meshUVs = NULL;
	if (blenderUVs) {
		meshUVs = new UV[tmpMeshVerts.size()];
		copy(tmpMeshUVs.begin(), tmpMeshUVs.end(), meshUVs);
	}

	Spectrum *meshCols = NULL;
	if (blenderCols) {
		meshCols = new Spectrum[tmpMeshVerts.size()];
		copy(tmpMeshCols.begin(), tmpMeshCols.end(), meshCols);
	}

	//cout << "Defining mesh: " << name << "\n";
	scene->DefineMesh(name, tmpMeshVerts.size(), tmpMeshTris.size(), meshVerts, meshTris,
			meshNorms, meshUVs, meshCols, NULL);
}

boost::python::list Scene_DefineBlenderMesh(Scene *scene, const string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr) {
	MFace *blenderFaces = reinterpret_cast<MFace *>(blenderFacesPtr);

	// Build the list of mesh material indices
	boost::unordered_set<u_int> matSet;
	for (u_int faceIndex = 0; faceIndex < blenderFaceCount; ++faceIndex) {
		const MFace &face = blenderFaces[faceIndex];

		matSet.insert(face.mat_nr);
	}

	boost::python::list result;
	BOOST_FOREACH(u_int matIndex, matSet) {		
		const string objName = (boost::format(name + "%03d") % matIndex).str();

		Scene_DefineBlenderMesh(scene, "Mesh-" + objName, blenderFaceCount, blenderFacesPtr,
				blenderVertCount, blenderVerticesPtr, blenderUVsPtr, blenderColsPtr, matIndex);
		
		boost::python::list meshInfo;
		meshInfo.append(objName);
		meshInfo.append(matIndex);
		result.append(meshInfo);
	}

	return result;
}

}
}
