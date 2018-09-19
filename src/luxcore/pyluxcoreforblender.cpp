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

#ifdef WIN32
// Windows seems to require this #define otherwise VisualC++ looks for
// Boost Python DLL symbols.
// Do not use for Unix(s), it makes some symbol local.
#define BOOST_PYTHON_STATIC_LIB
#define BOOST_NUMPY_STATIC_LIB
#endif

#include <memory>
#include <vector>
#include <algorithm>
#include <boost/foreach.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>
#include <boost/format.hpp>
#include <boost/python.hpp>
#include <boost/python/numpy.hpp>

#include <OpenImageIO/imagebufalgo.h>
#include <OpenImageIO/imagebuf.h>
#include <OpenImageIO/dassert.h>

#include <Python.h>

#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/pyluxcore/pyluxcoreforblender.h"
#include "luxrays/utils/utils.h"

using namespace std;
using namespace luxrays;
using namespace luxcore;
using namespace boost::python;
namespace np = boost::python::numpy;
OIIO_NAMESPACE_USING

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

// At the moment alpha is abused for vertex painting
// and not used for transparency, note that red and blue are swapped
struct MCol {
	u_char a, r, g, b;
};

struct MTFace {
	float uv[4][2];
	void *tpage;
	char flag, transp;
	short mode, tile, unwrap;
};

struct RenderPass {
	struct RenderPass *next, *prev;
	int channels;
	char name[64];
	char chan_id[8];
	float *rect;  // The only thing we are interested in
	int rectx, recty;

	char fullname[64];
	char view[64];
	int view_id;

	int pad;
};

//------------------------------------------------------------------------------
// Utility functions
//------------------------------------------------------------------------------

template<class T>
T FindMaxValue(const T* buffer, const u_int buffersize) {
	T maxValue = 0;
	for (u_int i = 0; i < buffersize; ++i) {
		const T value = buffer[i];
		if (value > maxValue) {
			maxValue = value;
		}
	}
	return maxValue;
}

template<>
float FindMaxValue<float>(const float *buffer, const u_int buffersize) {
	float maxValue = 0;
	for (u_int i = 0; i < buffersize; ++i) {
		const float value = buffer[i];
		if (!isinf(value) && !isnan(value) && (value > maxValue)) {
			maxValue = value;
		}
	}
	return maxValue;
}

template<class T>
void GetOutput(boost::python::object &filmObj, const Film::FilmOutputType outputType,
		const u_int outputIndex, T *pixels, const bool executeImagePipeline) {
	// Convert boost::python::object to C++ class
	luxcore::detail::FilmImpl &filmImpl = extract<luxcore::detail::FilmImpl &>(filmObj);
	Film &film = reinterpret_cast<Film &>(filmImpl);
	film.GetOutput<T>(outputType, pixels, outputIndex, executeImagePipeline);
}

// Safety check
void ThrowIfSizeMismatch(const RenderPass *renderPass, const u_int width, const u_int height) {
	if ((u_int)renderPass->rectx != width || (u_int)renderPass->recty != height) {
		const string rectSize = luxrays::ToString(renderPass->rectx) + "x" + luxrays::ToString(renderPass->recty);
		const string outputSize = luxrays::ToString(width) + "x" + luxrays::ToString(height);
		throw runtime_error("Size mismatch. RenderPass->rect size: " + rectSize + ", passed width x height: " + outputSize);
	}
}

//------------------------------------------------------------------------------
// Film output conversion functions
//------------------------------------------------------------------------------

// For channels like DEPTH
void ConvertFilmChannelOutput_1xFloat_To_1xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 1;
	
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	
	// srcBufferDepth is equal, write directly to the renderPass
	GetOutput(filmObj, outputType, outputIndex, renderPass->rect, executeImagePipeline);
	
	if (normalize) {
		const float maxValue = FindMaxValue(renderPass->rect, width * height * srcBufferDepth);
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
		
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * srcBufferDepth;

			for (u_int x = 0; x < width; ++x) {
				renderPass->rect[srcIndex++] *= k;
			}
		}
	}
}

// For the UV channel.
// We need to pad the UV pass to 3 elements (Blender can't handle 2 elements).
// The third channel is a mask that is 1 where a UV map exists and 0 otherwise.
void ConvertFilmChannelOutput_UV_to_Blender_UV(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 2;
	const u_int dstBufferDepth = 3;

	unique_ptr<float[]> src(new float[width * height * srcBufferDepth]);
	GetOutput(filmObj, outputType, outputIndex, src.get(), executeImagePipeline);
	
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	// Here we can't simply copy the values
	
	float k = 1.f;
	if (normalize) {
		const float maxValue = FindMaxValue(src.get(), width * height);
		k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
	}
		
	for (u_int y = 0; y < height; ++y) {
		u_int srcIndex = y * width * srcBufferDepth;
		u_int dstIndex = y * width * dstBufferDepth;

		for (u_int x = 0; x < width; ++x) {
			const float u = src[srcIndex] * k;
			const float v = src[srcIndex + 1] * k;
			
			renderPass->rect[dstIndex] = u;
			renderPass->rect[dstIndex + 1] = v;
			// The third channel is a mask that is 1 where a UV map exists and 0 otherwise.
			renderPass->rect[dstIndex + 2] = (u || v) ? 1.f : 0.f;
			
			srcIndex += srcBufferDepth;
			dstIndex += dstBufferDepth;
		}
	}
}

void ConvertFilmChannelOutput_1xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 1;
	const u_int dstBufferDepth = 4;
	
	unique_ptr<float[]> src(new float[width * height * srcBufferDepth]);
	GetOutput(filmObj, outputType, outputIndex, src.get(), executeImagePipeline);
	
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	// Here we can't simply copy the values
	
	float k = 1.f;
	if (normalize) {
		const float maxValue = FindMaxValue(src.get(), width * height);
		k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
	}
		
	for (u_int y = 0; y < height; ++y) {
		u_int srcIndex = y * width * srcBufferDepth;
		u_int dstIndex = y * width * dstBufferDepth;

		for (u_int x = 0; x < width; ++x) {
			const float val = src[srcIndex] * k;
			renderPass->rect[dstIndex] = val;
			renderPass->rect[dstIndex + 1] = val;
			renderPass->rect[dstIndex + 2] = val;
			renderPass->rect[dstIndex + 3] = 1.f;  // Alpha
			
			srcIndex += srcBufferDepth;
			dstIndex += dstBufferDepth;
		}
	}
}

void ConvertFilmChannelOutput_3xFloat_To_3xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 3;

	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	
	// srcBufferDepth is equal, write directly to the renderPass
	GetOutput(filmObj, outputType, outputIndex, renderPass->rect, executeImagePipeline);
	
	if (normalize) {
		const float maxValue = FindMaxValue(renderPass->rect, width * height);
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
		
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * srcBufferDepth;

			for (u_int x = 0; x < width; ++x) {
				renderPass->rect[srcIndex] *= k;
				renderPass->rect[srcIndex + 1] *= k;
				renderPass->rect[srcIndex + 2] *= k;
				srcIndex += srcBufferDepth;
			}
		}
	}
}

void ConvertFilmChannelOutput_3xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 3;
	const u_int dstBufferDepth = 4;
	
	unique_ptr<float[]> src(new float[width * height * srcBufferDepth]);
	GetOutput(filmObj, outputType, outputIndex, src.get(), executeImagePipeline);
		
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	// Here we can't simply copy the values
	
	float k = 1.f;
	if (normalize) {
		const float maxValue = FindMaxValue(src.get(), width * height);
		k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
	}
		
	for (u_int y = 0; y < height; ++y) {
		u_int srcIndex = y * width * srcBufferDepth;
		u_int dstIndex = y * width * dstBufferDepth;

		for (u_int x = 0; x < width; ++x) {
			renderPass->rect[dstIndex] = src[srcIndex] * k;
			renderPass->rect[dstIndex + 1] = src[srcIndex + 1] * k;
			renderPass->rect[dstIndex + 2] = src[srcIndex + 2] * k;
			renderPass->rect[dstIndex + 3] = 1.f;  // Alpha
			
			srcIndex += srcBufferDepth;
			dstIndex += dstBufferDepth;
		}
	}
}

void ConvertFilmChannelOutput_4xFloat_To_4xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 4;
	
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	
	// srcBufferDepth is equal, write directly to the renderPass
	GetOutput(filmObj, outputType, outputIndex, renderPass->rect, executeImagePipeline);
	
	if (normalize) {
		// Look for the max. in source buffer (only among RGB values, not Alpha)
		float maxValue = 0.f;
		for (u_int i = 0; i < width * height * 4; ++i) {
			const float value = renderPass->rect[i];
			// Leave out every multiple of 4 (alpha values)
			if ((i % 4 != 0) && !isinf(value) && !isnan(value) && (value > maxValue))
				maxValue = value;
		}
		const float k = (maxValue == 0.f) ? 0.f : (1.f / maxValue);
		
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = y * width * srcBufferDepth;

			for (u_int x = 0; x < width; ++x) {
				renderPass->rect[srcIndex] *= k;
				renderPass->rect[srcIndex + 1] *= k;
				renderPass->rect[srcIndex + 2] *= k;
				// Note: we do not normalize the alpha channel
				srcIndex += srcBufferDepth;
			}
		}
	}
}

// This function is for channels like the material index, object index or samplecount
void ConvertFilmChannelOutput_1xUInt_To_1xFloatList(boost::python::object &filmObj,
		const Film::FilmOutputType outputType, const u_int outputIndex, const u_int width, const u_int height,
		const size_t renderPassPtr, const bool normalize, const bool executeImagePipeline) {
	const u_int srcBufferDepth = 1;

	// Note that objSrc is unsigned int here
	unique_ptr<u_int[]> src(new u_int[width * height * srcBufferDepth]);
	GetOutput(filmObj, outputType, outputIndex, src.get(), executeImagePipeline);
	
	RenderPass *renderPass = reinterpret_cast<RenderPass *>(renderPassPtr);
	ThrowIfSizeMismatch(renderPass, width, height);
	// Here we can't simply copy the values
	
	float k = 1.f;
	if (normalize) {
		u_int maxValue = FindMaxValue(src.get(), width * height);
		k = (maxValue == 0) ? 0.f : (1.f / maxValue);
	}
		
	for (u_int y = 0; y < height; ++y) {
		u_int srcIndex = y * width * srcBufferDepth;

		for (u_int x = 0; x < width; ++x) {
			// u_int is converted to float here
			renderPass->rect[srcIndex] = src[srcIndex] * k;
			srcIndex += srcBufferDepth;
		}
	}
}

//------------------------------------------------------------------------------
// Mesh conversion functions
//------------------------------------------------------------------------------

static bool Scene_DefineBlenderMesh(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr, const short matIndex,
		const luxrays::Transform *trans) {
	const MFace *blenderFaces = reinterpret_cast<const MFace *>(blenderFacesPtr);
	const MVert *blenderFVertices = reinterpret_cast<const MVert *>(blenderVerticesPtr);
	const MTFace *blenderUVs = reinterpret_cast<const MTFace *>(blenderUVsPtr);
	const MCol *blenderCols = reinterpret_cast<const MCol *>(blenderColsPtr);

	const float normalScale = 1.f / 32767.f;
	const float rgbScale = 1.f / 255.f;
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
					
					bool alreadyDefined = (vertexMap.find(index) != vertexMap.end());
					if (alreadyDefined) {
						const u_int mappedIndex = vertexMap[index];

						if (blenderUVs) {
							// Check if the already defined vertex has the right UV coordinates
							if ((blenderUVs[faceIndex].uv[j][0] != tmpMeshUVs[mappedIndex].u) ||
									(blenderUVs[faceIndex].uv[j][1] != tmpMeshUVs[mappedIndex].v)) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}

						if (blenderCols) {
							// Check if the already defined vertex has the right color
							if (((blenderCols[faceIndex * 4 + j].b * rgbScale) != tmpMeshCols[mappedIndex].c[0]) ||
									((blenderCols[faceIndex * 4 + j].g * rgbScale) != tmpMeshCols[mappedIndex].c[1]) ||
									((blenderCols[faceIndex * 4 + j].r * rgbScale) != tmpMeshCols[mappedIndex].c[2])) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}
					}

					if (alreadyDefined)
						vertIndices[j] = vertexMap[index];
					else {
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
						if (blenderCols) {
							tmpMeshCols.push_back(Spectrum(
								blenderCols[faceIndex * 4 + j].b * rgbScale,
								blenderCols[faceIndex * 4 + j].g * rgbScale,
								blenderCols[faceIndex * 4 + j].r * rgbScale));
						}

						// Add the vertex mapping
						const u_int vertIndex = vertFreeIndex++;
						vertexMap[index] = vertIndex;
						vertIndices[j] = vertIndex;
					}
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
					if (blenderCols) {
						tmpMeshCols.push_back(Spectrum(
							blenderCols[faceIndex * 4 + j].b * rgbScale,
							blenderCols[faceIndex * 4 + j].g * rgbScale,
							blenderCols[faceIndex * 4 + j].r * rgbScale));
					}

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
		return false;

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
	luxrays::ExtTriangleMesh *mesh = new luxrays::ExtTriangleMesh(tmpMeshVerts.size(),
			tmpMeshTris.size(), meshVerts, meshTris,
			meshNorms, meshUVs, meshCols, NULL);
	
	// Apply the transformation if required
	if (trans)
		mesh->ApplyTransform(*trans);
	
	mesh->SetName(name);
	scene->DefineMesh(mesh);

	return true;
}

boost::python::list Scene_DefineBlenderMesh1(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr,
		const boost::python::object &transformation) {
	// Get the transformation if required
	bool hasTransformation = false;
	Transform trans;
	if (!transformation.is_none()) {
		extract<boost::python::list> getTransformationList(transformation);
		if (getTransformationList.check()) {
			const boost::python::list &l = getTransformationList();
			const boost::python::ssize_t size = len(l);
			if (size != 16) {
				const string objType = extract<string>((transformation.attr("__class__")).attr("__name__"));
				throw runtime_error("Wrong number of elements for the list of transformation values of method Scene.DefineMesh(): " + objType);
			}

			luxrays::Matrix4x4 mat;
			boost::python::ssize_t index = 0;
			for (u_int j = 0; j < 4; ++j)
				for (u_int i = 0; i < 4; ++i)
					mat.m[i][j] = extract<float>(l[index++]);

			trans = luxrays::Transform(mat);
			hasTransformation = true;
		} else {
			const string objType = extract<string>((transformation.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of transformation values of method Scene.DefineMesh(): " + objType);
		}
	}

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

		if (Scene_DefineBlenderMesh(scene, "Mesh-" + objName, blenderFaceCount, blenderFacesPtr,
				blenderVertCount, blenderVerticesPtr, blenderUVsPtr, blenderColsPtr, matIndex,
				hasTransformation ? &trans : NULL)) {
			boost::python::list meshInfo;
			meshInfo.append(objName);
			meshInfo.append(matIndex);
			result.append(meshInfo);
		}
	}

	return result;
}

boost::python::list Scene_DefineBlenderMesh2(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t blenderFaceCount, const size_t blenderFacesPtr,
		const size_t blenderVertCount, const size_t blenderVerticesPtr,
		const size_t blenderUVsPtr, const size_t blenderColsPtr) {
	return Scene_DefineBlenderMesh1(scene, name, blenderFaceCount, blenderFacesPtr,
			blenderVertCount, blenderVerticesPtr, blenderUVsPtr, blenderColsPtr,
			boost::python::object());
}

//------------------------------------------------------------------------------
// Hair/strands conversion functions
//------------------------------------------------------------------------------

Point makePoint(const float *arrayPos, const float worldscale) {
	return Point(arrayPos[0] * worldscale,
				 arrayPos[1] * worldscale,
				 arrayPos[2] * worldscale);
}

bool nearlyEqual(const float a, const float b, const float epsilon) {
	return fabs(a - b) < epsilon;
}

Spectrum getColorFromImage(const vector<float> &imageData, const float gamma,
	                       const u_int width, const u_int height, const u_int channelCount,
	                       const float u, const float v) {
	assert (width > 0);
	assert (height > 0);
	
 	const int x = u * (width - 1);
	// The pixels coming from OIIO are flipped in y direction, so we flip v
	const int y = (1.f - v) * (height - 1);
	assert (x >= 0);
	assert (x < width);
	assert (y >= 0);
	assert (y < height);
	
	const int index = (width * y + x) * channelCount;
	
	if (channelCount == 1) {
		return Spectrum(powf(imageData[index], gamma));
	} else {
		// In case of channelCount == 4, we just ignore the alpha channel
		return Spectrum(powf(imageData[index], gamma),
		                powf(imageData[index + 1], gamma),
						powf(imageData[index + 2], gamma));
	}
}

// Returns true if the shape could be defined successfully, false otherwise.
// root_width, tip_width and width_offset are percentages (range 0..1).
bool Scene_DefineBlenderStrands(luxcore::detail::SceneImpl *scene,
		const string &shapeName,
		const u_int pointsPerStrand,
		const boost::python::object &points,
		const boost::python::object &colors,
		const boost::python::object &uvs,
		const string &imageFilename,
		const float imageGamma,
		const bool copyUVs,
		const float worldscale,
		const float strandDiameter, // already multiplied with worldscale
		const float rootWidth,
		const float tipWidth,
		const float widthOffset,
		const string &tessellationTypeStr,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const boost::python::list &rootColor,
		const boost::python::list &tipColor) {
	//--------------------------------------------------------------------------
	// Extract arguments (e.g. numpy arrays)
	//--------------------------------------------------------------------------
	
	if (pointsPerStrand == 0)
		throw runtime_error("pointsPerStrand needs to be greater than 0");
	
	// Points
	extract<np::ndarray> getPointsArray(points);
	if (!getPointsArray.check())
		throw runtime_error("Points: not a numpy ndarray");
	
	const np::ndarray &arrPoints = getPointsArray();
	if (arrPoints.get_dtype() != np::dtype::get_builtin<float>())
		throw runtime_error("Points: Wrong ndarray dtype (required: float32)");
	if (arrPoints.get_nd() != 1)
		throw runtime_error("Points: Wrong number of dimensions (required: 1)");
	
	const float * const pointsStartPtr = reinterpret_cast<const float*>(arrPoints.get_data());
	const int pointArraySize = arrPoints.shape(0);
	const int pointStride = 3;
	const size_t inputPointCount = pointArraySize / pointStride;
	
	// Colors
	extract<np::ndarray> getColorsArray(colors);
	if (!getColorsArray.check())
		throw runtime_error("Colors: not a numpy ndarray");
	
	const np::ndarray &arrColors = getColorsArray();
	if (arrColors.get_dtype() != np::dtype::get_builtin<float>())
		throw runtime_error("Colors: Wrong ndarray dtype (required: float32)");
	if (arrColors.get_nd() != 1)
		throw runtime_error("Colors: Wrong number of dimensions (required: 1)");
	
	const float * const colorsStartPtr = reinterpret_cast<const float*>(arrColors.get_data());
	const int colorArraySize = arrColors.shape(0);
	const int colorStride = 3;
	// const size_t inputColorCount = colorArraySize / colorStride;
	const bool useVertexCols = colorArraySize > 0;
	
	// Root/tip colors
	if (len(rootColor) != 3)
		throw runtime_error("rootColor list has wrong length (required: 3)");
	if (len(tipColor) != 3)
		throw runtime_error("tipColor list has wrong length (required: 3)");
	const float rootColorR = extract<float>(rootColor[0]);
	const float rootColorG = extract<float>(rootColor[1]);
	const float rootColorB = extract<float>(rootColor[2]);
	const float tipColorR = extract<float>(tipColor[0]);
	const float tipColorG = extract<float>(tipColor[1]);
	const float tipColorB = extract<float>(tipColor[2]);
	const Spectrum rootCol(rootColorR, rootColorG, rootColorB);
   	const Spectrum tipCol(tipColorR, tipColorG, tipColorB);
	const Spectrum white(1.f);
	// Since root and tip colors are multipliers, we don't need them if both are white
	const bool useRootTipColors = rootCol != white || tipCol != white;
	
	// UVs (note: only needed for getting colors from an image, not used as strands UVs)
	extract<np::ndarray> getUVsArray(uvs);
	if (!getUVsArray.check())
		throw runtime_error("UVs: not a numpy ndarray");
	
	const np::ndarray &arrUVs = getUVsArray();
	if (arrUVs.get_dtype() != np::dtype::get_builtin<float>())
		throw runtime_error("UVs: Wrong ndarray dtype (required: float32)");
	if (arrUVs.get_nd() != 1)
		throw runtime_error("UVs: Wrong number of dimensions (required: 1)");
	
	const float * const uvsStartPtr = reinterpret_cast<const float*>(arrUVs.get_data());
	const int uvArraySize = arrUVs.shape(0);
	const int uvStride = 2;
	
	// If UVs are used, we expect one UV coord per strand (not per point)
	const int inputUVCount = uvArraySize / uvStride;
	const int inputStrandCount = inputPointCount / pointsPerStrand;
	if (uvArraySize > 0 && inputUVCount != inputStrandCount)
		throw runtime_error("UV array size is " + to_string(inputUVCount)
                            + " (expected: " + to_string(inputStrandCount) + ")");
	
	if (copyUVs && uvArraySize == 0)
		throw runtime_error("Can not copy UVs without UV array");
	
	// Tessellation type
	Scene::StrandsTessellationType tessellationType;
	if (tessellationTypeStr == "ribbon")
		tessellationType = Scene::TESSEL_RIBBON;
	else if (tessellationTypeStr == "ribbonadaptive")
		tessellationType = Scene::TESSEL_RIBBON_ADAPTIVE;
	else if (tessellationTypeStr == "solid")
		tessellationType = Scene::TESSEL_SOLID;
	else if (tessellationTypeStr == "solidadaptive")
		tessellationType = Scene::TESSEL_SOLID_ADAPTIVE;
	else
		throw runtime_error("Unknown tessellation type: " + tessellationTypeStr);
	
	//--------------------------------------------------------------------------
	// Load image if required
	//--------------------------------------------------------------------------
	
	vector<float> imageData;
	u_int width = 0;
	u_int height = 0;
	u_int channelCount = 0;
	
	if (uvArraySize > 0 && !imageFilename.empty()) {
		ImageSpec config;
		config.attribute ("oiio:UnassociatedAlpha", 1);
		auto_ptr<ImageInput> in(ImageInput::open(imageFilename, &config));
		
		if (!in.get()) {
			throw runtime_error("Error opening image file : " + imageFilename +
					            "\n" + geterror());
		}
		
		const ImageSpec &spec = in->spec();

		width = spec.width;
		height = spec.height;
		channelCount = spec.nchannels;
		
		if (channelCount != 1 && channelCount != 3 && channelCount != 4) {
			throw runtime_error("Unsupported number of channels (" + to_string(channelCount)
			                    + ") in image file: " + imageFilename
							    + " (supported: 1, 3, or 4 channels)");
		}
		
		imageData.resize(width * height * channelCount);
		in->read_image(TypeDesc::FLOAT, &imageData[0]);
		in->close();
		in.reset();
	}
	
	if (!imageFilename.empty() && uvArraySize == 0)
		throw runtime_error("Image provided, but no UV data");
		
	const bool colorsFromImage = uvArraySize > 0 && !imageFilename.empty();
	if (useVertexCols && colorsFromImage)
		throw runtime_error("Can't copy colors from both image and color array");
	
	//--------------------------------------------------------------------------
	// Remove invalid points, create other arrays (segments, thickness etc.)
	//--------------------------------------------------------------------------
	
	// There can be invalid points, so we have to filter them
	const float epsilon = 0.000000001f;
	const Point invalidPoint(0.f, 0.f, 0.f);
	
	vector<u_short> segments;
	segments.reserve(inputPointCount / pointsPerStrand);
	
	// We save the filtered points as raw floats so we can easily move later
	vector<float> filteredPoints;
	filteredPoints.reserve(pointArraySize);
	
	// We only need the thickness array if rootWidth and tipWidth are not equal.
	// Also, if the widthOffset is 1, there is no thickness variation.
	const bool useThicknessArray = !nearlyEqual(rootWidth, tipWidth, epsilon)
	                               && !nearlyEqual(widthOffset, 1.f, epsilon);
	vector<float> thickness;
	if (useThicknessArray) {
		thickness.reserve(inputPointCount);
	} else {
		thickness.push_back(strandDiameter * rootWidth);
	}
	
	const bool useColorsArray = colorsFromImage || useVertexCols || useRootTipColors;
	vector<float> filteredColors;
	if (useColorsArray) {
		filteredColors.reserve(inputPointCount * colorStride);
	}
	
	const bool useUVsArray = inputUVCount > 0 && copyUVs;
	vector<float> filteredUVs;
	if (useUVsArray) {
		filteredUVs.reserve(inputPointCount * uvStride);
	}
	
	const float *pointPtr = pointsStartPtr;
	const float *uvPtr = uvsStartPtr;
	const float *colorPtr = colorsStartPtr;
	
	while (pointPtr < (pointsStartPtr + pointArraySize)) {
		u_short validPointCount = 0;
		
		// We only have uv and color information for the first point of each strand
		float u = 0.f, v = 0.f, r = 1.f, g = 1.f, b = 1.f;
		if (useUVsArray || colorsFromImage) {
			u = *uvPtr++;
			v = *uvPtr++;
			// u and v might be out of range 0..1
			u -= floor(u);
			v -= floor(v);
		}
		if (useVertexCols) {
			r = *colorPtr++;
			g = *colorPtr++;
			b = *colorPtr++;
		}
		
		Point currPoint = makePoint(pointPtr, worldscale);
		pointPtr += pointStride;
		Point lastPoint;
		
		// Iterate over the strand. We can skip step == 0.
		for (u_int step = 1; step < pointsPerStrand; ++step) {
			lastPoint = currPoint;
			currPoint = makePoint(pointPtr, worldscale);
			pointPtr += pointStride;
			
			if (lastPoint == invalidPoint || currPoint == invalidPoint) {
				// Blender sometimes creates points that are all zeros, e.g. if
				// hair length is textured and an area is black (length == 0)
				continue;
			}
			
			const float segmentLengthSqr = DistanceSquared(currPoint, lastPoint);
			if (segmentLengthSqr < epsilon) {
				continue;
			}
			
			if (step == 1) {
				filteredPoints.push_back(lastPoint.x);
				filteredPoints.push_back(lastPoint.y);
				filteredPoints.push_back(lastPoint.z);
				validPointCount++;
			
				// The root point of a strand always uses the rootWidth
				if (useThicknessArray) {
					thickness.push_back(rootWidth * strandDiameter);
				}
				
				if (useUVsArray) {
					filteredUVs.push_back(u);
					filteredUVs.push_back(v);
				}
				
				Spectrum colPoint(1.f);
				
				if (colorsFromImage) {
					colPoint = getColorFromImage(imageData, imageGamma,
			                                     width, height,
		                                         channelCount, u, v);
				}
				
				if (useVertexCols) {
					colPoint = Spectrum(r, g, b);
				}
				
				if (useColorsArray) {
					if (useRootTipColors) {
						// We are in the root, no need to interpolate
						colPoint *= rootCol;
					}
					
					filteredColors.push_back(colPoint.c[0]);
					filteredColors.push_back(colPoint.c[1]);
					filteredColors.push_back(colPoint.c[2]);
				}
			}
			
			filteredPoints.push_back(currPoint.x);
			filteredPoints.push_back(currPoint.y);
			filteredPoints.push_back(currPoint.z);
			validPointCount++;
			
			if (useThicknessArray) {
				const float widthOffsetSteps = widthOffset * (pointsPerStrand - 1);
				
				if (step < widthOffsetSteps) {
					// We are still in the root part
					thickness.push_back(rootWidth * strandDiameter);
				} else {
					// We are above the root, interpolate thickness
					const float normalizedPosition = ((float)step - widthOffsetSteps)
													 / (pointsPerStrand - 1 - widthOffsetSteps);
					const float relativeThick = Lerp(normalizedPosition, rootWidth, tipWidth);
					thickness.push_back(relativeThick * strandDiameter);
				}
			}
			
			if (useUVsArray) {
				filteredUVs.push_back(u);
				filteredUVs.push_back(v);
			}
			
			Spectrum colPoint(1.f);
			
			if (colorsFromImage) {
				colPoint = getColorFromImage(imageData, imageGamma,
					                         width, height,
											 channelCount, u, v);
			}
			
			if (useVertexCols) {
				colPoint = Spectrum(r, g, b);
			}
			
			if (useColorsArray) {
				if (useRootTipColors) {
					if (step == pointsPerStrand - 1) {
						// We are in the root, no need to interpolate
						colPoint *= tipCol;
					} else {
						const float normalizedPosition = (float)step / (pointsPerStrand - 1);
						colPoint *= Lerp(normalizedPosition, rootCol, tipCol);
					}
				}
					
				filteredColors.push_back(colPoint.c[0]);
				filteredColors.push_back(colPoint.c[1]);
				filteredColors.push_back(colPoint.c[2]);
			}
		}
		
		if (validPointCount == 1) {
			// Can't make a segment with only one point, rollback
			for (int i = 0; i < pointStride; ++i)
				filteredPoints.pop_back();
			
			if (useThicknessArray)
				thickness.pop_back();
			
			if (useColorsArray) {
				for (int i = 0; i < colorStride; ++i)
					filteredColors.pop_back();
			}
			
			if (useUVsArray) {
				for (int i = 0; i < uvStride; ++i)
					filteredUVs.pop_back();
			}
		} else if (validPointCount > 1) {
			segments.push_back(validPointCount - 1);
		}
	}
	
	if (segments.empty()) {
		SLG_LOG("Aborting strand definition: Could not find valid segments!");
		return false;
	}
	
	const size_t pointCount = filteredPoints.size() / pointStride;
	
	if (pointCount != inputPointCount) {
		SLG_LOG("Removed " << (inputPointCount - pointCount) << " invalid points");
	}
	
	const bool allSegmentsEqual = std::adjacent_find(segments.begin(), segments.end(),
													 std::not_equal_to<u_short>()) == segments.end();
		
	//--------------------------------------------------------------------------
	// Create hair file header
	//--------------------------------------------------------------------------
	
	luxrays::cyHairFile strands;
	strands.SetHairCount(segments.size());
	strands.SetPointCount(pointCount);
	
	int flags = CY_HAIR_FILE_POINTS_BIT;

	if (allSegmentsEqual) {
		strands.SetDefaultSegmentCount(segments.at(0));
	}
	else {
		flags |= CY_HAIR_FILE_SEGMENTS_BIT;
	}

	if (useThicknessArray)
		flags |= CY_HAIR_FILE_THICKNESS_BIT;
	else
		strands.SetDefaultThickness(thickness.at(0));

	// We don't need/support vertex alpha at the moment
	strands.SetDefaultTransparency(0.f);
	
	if (useColorsArray)
		flags |= CY_HAIR_FILE_COLORS_BIT;
	else
		strands.SetDefaultColor(1.f, 1.f, 1.f);

	// if (!uvs.is_none())
	// 	flags |= CY_HAIR_FILE_UVS_BIT;
	if (useUVsArray)
		flags |= CY_HAIR_FILE_UVS_BIT;

	strands.SetArrays(flags);
	
	//--------------------------------------------------------------------------
	// Copy/move data into hair file
	//--------------------------------------------------------------------------
	
	if (!allSegmentsEqual) {
		move(segments.begin(), segments.end(), strands.GetSegmentsArray());
	}
	
	if (useThicknessArray) {
		move(thickness.begin(), thickness.end(), strands.GetThicknessArray());
	}
	
	if (useColorsArray) {
		move(filteredColors.begin(), filteredColors.end(), strands.GetColorsArray());
	}
	
	if (useUVsArray) {
		move(filteredUVs.begin(), filteredUVs.end(), strands.GetUVsArray());
	}
	
	move(filteredPoints.begin(), filteredPoints.end(), strands.GetPointsArray());

	const bool useCameraPosition = true;
	scene->DefineStrands(shapeName, strands,
			tessellationType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
	return true;
}

}  // namespace blender
}  // namespace luxcore
