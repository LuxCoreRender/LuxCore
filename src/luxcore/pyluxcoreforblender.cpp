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

void Scene_DefineBlenderStrands(luxcore::detail::SceneImpl *scene,
		const string &shapeName,
		// const int strandsCount, // remove (compute here)
		const int pointsPerStrand,
		const boost::python::object &points,
		// const boost::python::object &segments, // remove (compute here)
		// const boost::python::object &thickness, // remove (compute here)
		// const boost::python::object &transparency, // remove (not needed)
		const boost::python::object &colors,
		const boost::python::object &uvs,
		const string &tessellationTypeStr,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	// TODO remove debug print
	cout << "Scene_DefineBlenderStrands" << endl;
	const double startTime = WallClockTime();
	
	extract<np::ndarray> getPointsArray(points);
	if (!getPointsArray.check()) {
		// TODO better error message
		throw runtime_error("points error");
	}
	
	const np::ndarray &arr = getPointsArray();
	if (arr.get_dtype() != np::dtype::get_builtin<float>())
		throw runtime_error("Wrong ndarray dtype");
	if (arr.get_nd() != 1)
		throw runtime_error("Wrong number of dimensions");
	
	const float *pointsPtr = reinterpret_cast<const float*>(arr.get_data());
	const int pointsSize = arr.shape(0);
	const int pointStride = 3;
	
	// There can be invalid points, so we have to filter them
	const float epsilon = 0.000001f;
	const int maxPointCount = pointsSize / pointStride;
	vector<u_short> segments;
	segments.reserve(maxPointCount / pointsPerStrand);
	// We save the filtered points as floats so we can easily move later
	vector<float> filteredPoints;
	filteredPoints.reserve(pointsSize);
	int strandsCount = 0;
	
	printf("Checking %d points, %d points per strand\n", maxPointCount, pointsPerStrand);
	
	for (const float *p = pointsPtr; p < (pointsPtr + pointsSize); ) {
		u_short validSegmentsCount = 0;
		Point currPoint(p[0], p[1], p[2]);
		p += pointStride;
		Point lastPoint;
		
		// Iterate over the strand. We can skip step == 0.
		for (int step = 1; step < pointsPerStrand; ++step, p += pointStride) {
			lastPoint = currPoint;
			currPoint = Point(p[0], p[1], p[2]);
			const float segmentLengthSqr = DistanceSquared(currPoint, lastPoint);
			
			if (segmentLengthSqr > epsilon) {
				validSegmentsCount++;
				if (step == 1) {
					filteredPoints.push_back(lastPoint.x);
					filteredPoints.push_back(lastPoint.y);
					filteredPoints.push_back(lastPoint.z);
				}
				filteredPoints.push_back(currPoint.x);
				filteredPoints.push_back(currPoint.y);
				filteredPoints.push_back(currPoint.z);
			}
		}
		
		if (validSegmentsCount > 0) {
			segments.push_back(validSegmentsCount);
			strandsCount++;
		}
		
		if ((p + pointStride) == (pointsPtr + pointsSize)) {
			printf("Reached the end\n");
		}
	}
	
	if (segments.empty()) {
		cout << "Aborting strand definition: Could not find valid segments!"
			 << " (" << maxPointCount << " input points)" << endl;
		return;
	}
	
	printf("Got %ld filtered points, making up %d strands\n", (filteredPoints.size() / pointStride), strandsCount);
	
	const bool allSegmentsEqual = std::adjacent_find(segments.begin(), segments.end(),
													 std::not_equal_to<u_short>()) == segments.end();
	cout << "all segments equal: " << allSegmentsEqual << endl;
	
	// Create hair file and copy/move the data
	luxrays::cyHairFile strands;
	strands.SetHairCount(strandsCount);
	strands.SetPointCount(filteredPoints.size());
	
	// ------------
	int flags = CY_HAIR_FILE_POINTS_BIT;

	if (allSegmentsEqual)
		strands.SetDefaultSegmentCount(segments.at(0));
	else
		flags |= CY_HAIR_FILE_SEGMENTS_BIT;

	// boost::python::extract<float> defaultThicknessValue(thickness);
	// if (defaultThicknessValue.check())
	// 	strands.SetDefaultThickness(defaultThicknessValue());
	// else
	// 	flags |= CY_HAIR_FILE_THICKNESS_BIT;
	strands.SetDefaultThickness(0.01f);

	// boost::python::extract<float> defaultTransparencyValue(transparency);
	// if (defaultTransparencyValue.check())
	// 	strands.SetDefaultTransparency(defaultTransparencyValue());
	// else
	// 	flags |= CY_HAIR_FILE_TRANSPARENCY_BIT;
	strands.SetDefaultTransparency(0.f);

	// boost::python::extract<boost::python::tuple> defaultColorsValue(colors);
	// if (defaultColorsValue.check()) {
	// 	const boost::python::tuple &t = defaultColorsValue();
	//
	// 	strands.SetDefaultColor(
	// 		extract<float>(t[0]),
	// 		extract<float>(t[1]),
	// 		extract<float>(t[2]));
	// } else
	// 	flags |= CY_HAIR_FILE_COLORS_BIT;
	strands.SetDefaultColor(1.f, 1.f, 1.f);

	// if (!uvs.is_none())
	// 	flags |= CY_HAIR_FILE_UVS_BIT;

	strands.SetArrays(flags);
	// ------------
	
	if (!allSegmentsEqual) {
		move(segments.begin(), segments.end(), strands.GetSegmentsArray());
	}
	
	move(filteredPoints.begin(), filteredPoints.end(), strands.GetPointsArray());
	
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
		throw runtime_error("Tessellation type unknown in method Scene.DefineStrands(): " + tessellationTypeStr);
	
	const double endTime = WallClockTime();
	printf("Preparing strands took %.3f s\n", (endTime - startTime));

	scene->DefineStrands(shapeName, strands,
			tessellationType, adaptiveMaxDepth, adaptiveError,
			solidSideCount, solidCapBottom, solidCapTop,
			useCameraPosition);
}

}  // namespace blender
}  // namespace luxcore
