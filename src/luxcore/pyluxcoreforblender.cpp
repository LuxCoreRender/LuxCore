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

#ifdef WIN32
// Windows seems to require this #define otherwise VisualC++ looks for
// Boost Python DLL symbols.
// Do not use for Unix(s), it makes some symbol local.
#define BOOST_PYTHON_STATIC_LIB
#define BOOST_NUMPY_STATIC_LIB
// Python 3.8 and older define snprintf as a macro even for VS 2015 and newer,
// where this causes an error - See https://bugs.python.org/issue36020
#if defined(_MSC_VER) && _MSC_VER >= 1900
#define HAVE_SNPRINTF
#endif
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

#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>

#include <Python.h>

#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/pyluxcore/pyluxcoreforblender.h"
#include "luxcore/pyluxcore/blender_types.h"
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
// Blender struct access functions
//------------------------------------------------------------------------------

template<typename CustomData>
static int CustomData_get_active_layer_index(const CustomData *data, int type)
{
	const int layer_index = data->typemap[type];
	return (layer_index != -1) ? layer_index + data->layers[layer_index].active : -1;
}

template<typename CustomData>
static void *CustomData_get_layer(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if (layer_index == -1) {
		return nullptr;
	}

	return data->layers[layer_index].data;
}

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

static Transform ExtractTransformation(const boost::python::object &transformation) {
	if (transformation.is_none()) {
		return Transform();
	}

	extract<boost::python::list> getTransformationList(transformation);
	if (getTransformationList.check()) {
		const boost::python::list &lst = getTransformationList();
		const boost::python::ssize_t size = len(lst);
		if (size != 16) {
			const string objType = extract<string>((transformation.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong number of elements for the list of transformation values: " + objType);
		}

		luxrays::Matrix4x4 mat;
		boost::python::ssize_t index = 0;
		for (u_int j = 0; j < 4; ++j)
			for (u_int i = 0; i < 4; ++i)
				mat.m[i][j] = extract<float>(lst[index++]);

		return Transform(mat);
	}
	else {
		const string objType = extract<string>((transformation.attr("__class__")).attr("__name__"));
		throw runtime_error("Wrong data type for the list of transformation values: " + objType);
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

// Note: This method is used by pyluxcoredemo.py, do not remove.
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
		PyBuffer_Release(&srcView);

		const string objType = extract<string>((objSrc.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}

	if (srcView.len / (3 * 4) != dstView.len / 4) {
		PyBuffer_Release(&srcView);
		PyBuffer_Release(&dstView);
		throw runtime_error("Wrong buffer size in ConvertFilmChannelOutput_3xFloat_To_4xUChar()");
	}

	const float *src = (float *)srcView.buf;
	u_char *dst = (u_char *)dstView.buf;

	if (normalize) {
		const float maxValue = FindMaxValue(src, width * height * 3);
		const float k = (maxValue == 0.f) ? 0.f : (255.f / maxValue);

		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = (height - y - 1) * width * 3;
			u_int dstIndex = y * width * 4;

			for (u_int x = 0; x < width; ++x) {
				dst[dstIndex++] = (u_char)floor((src[srcIndex + 2] * k + .5f));
				dst[dstIndex++] = (u_char)floor((src[srcIndex + 1] * k + .5f));
				dst[dstIndex++] = (u_char)floor((src[srcIndex] * k + .5f));
				dst[dstIndex++] = 0xff;
				srcIndex += 3;
			}
		}
	} else {
		for (u_int y = 0; y < height; ++y) {
			u_int srcIndex = (height - y - 1) * width * 3;
			u_int dstIndex = y * width * 4;

			for (u_int x = 0; x < width; ++x) {
				dst[dstIndex++] = (u_char)floor((src[srcIndex + 2] * 255.f + .5f));
				dst[dstIndex++] = (u_char)floor((src[srcIndex + 1] * 255.f + .5f));
				dst[dstIndex++] = (u_char)floor((src[srcIndex] * 255.f + .5f));
				dst[dstIndex++] = 0xff;
				srcIndex += 3;
			}
		}
	}
	
	PyBuffer_Release(&srcView);
	PyBuffer_Release(&dstView);
}

//------------------------------------------------------------------------------
// General utility functions for the Blender addon
//------------------------------------------------------------------------------

// No safety checks to gain speed, this function is called for each particle, 
// potentially millions of times.
boost::python::list BlenderMatrix4x4ToList(boost::python::object &blenderMatrix) {
	const PyObject *pyObj = blenderMatrix.ptr();
	const MatrixObject *blenderMatrixObj = (MatrixObject *)pyObj;

	boost::python::list result;

	for (int i = 0; i < 16; ++i) {
		result.append(blenderMatrixObj->matrix[i]);
	}

	// Make invertible if necessary
	Matrix4x4 matrix(blenderMatrixObj->matrix);
	if (matrix.Determinant() == 0.f) {
		const float epsilon = 1e-8f;
		result[0] += epsilon;  // [0][0]
		result[5] += epsilon;  // [1][1]
		result[10] += epsilon;  // [2][2]
		result[15] += epsilon;  // [3][3]
	}

	return result;
}

//------------------------------------------------------------------------------
// OpenVDB helper functions
//------------------------------------------------------------------------------
boost::python::list GetOpenVDBGridNames(const string &filePathStr) {
	boost::python::list gridNames;

	openvdb::io::File file(filePathStr);
	file.open();
	for (auto i = file.beginName(); i != file.endName(); ++i)
		gridNames.append(*i);

	file.close();
	return gridNames;
}


boost::python::tuple GetOpenVDBGridInfo(const string &filePathStr, const string &gridName) {
	boost::python::list bBox;
	boost::python::list bBox_w;
	boost::python::list trans_matrix;
	boost::python::list BlenderMetadata;
	openvdb::io::File file(filePathStr);
	
	file.open();
	openvdb::MetaMap::Ptr ovdbMetaMap = file.getMetadata();

	string creator = "";
	try {
		creator = ovdbMetaMap->metaValue<string>("creator");
	} catch (openvdb::LookupError &e) {
		cout << "No creator file meta data found in OpenVDB file " + filePathStr << endl;
	};

	openvdb::GridBase::Ptr ovdbGrid = file.readGridMetadata(gridName);	

	//const openvdb::Vec3i bbox_min = ovdbGrid->metaValue<openvdb::Vec3i>("file_bbox_min");
	//const openvdb::Vec3i bbox_max = ovdbGrid->metaValue<openvdb::Vec3i>("file_bbox_max");

	const openvdb::math::Transform &transform = ovdbGrid->transform();
	openvdb::math::Mat4f matrix = transform.baseMap()->getAffineMap()->getMat4();

	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			trans_matrix.append(matrix(col, row));
		}
	}

	// Read the grid from the file
	ovdbGrid = file.readGrid(gridName);

	openvdb::CoordBBox coordbbox;
	ovdbGrid->baseTree().evalLeafBoundingBox(coordbbox);
	openvdb::BBoxd bbox_world = ovdbGrid->transform().indexToWorld(coordbbox);

	bBox.append(coordbbox.min()[0]);
	bBox.append(coordbbox.min()[1]);
	bBox.append(coordbbox.min()[2]);

	bBox.append(coordbbox.max()[0]);
	bBox.append(coordbbox.max()[1]);
	bBox.append(coordbbox.max()[2]);

	bBox_w.append(bbox_world.min().x());
	bBox_w.append(bbox_world.min().y());
	bBox_w.append(bbox_world.min().z());

	bBox_w.append(bbox_world.max().x());
	bBox_w.append(bbox_world.max().y());
	bBox_w.append(bbox_world.max().z());


	if (creator == "Blender/Smoke") {
		boost::python::list min_bbox_list;
		boost::python::list max_bbox_list;
		boost::python::list res_list;
		boost::python::list minres_list;
		boost::python::list maxres_list;
		boost::python::list baseres_list;
		boost::python::list obmat_list;
		boost::python::list obj_shift_f_list;

		openvdb::Vec3s min_bbox = ovdbMetaMap->metaValue<openvdb::Vec3s>("blender/smoke/min_bbox");
		openvdb::Vec3s max_bbox = ovdbMetaMap->metaValue<openvdb::Vec3s>("blender/smoke/max_bbox");
		openvdb::Vec3i res = ovdbMetaMap->metaValue<openvdb::Vec3i>("blender/smoke/resolution");

		//adaptive domain settings
		openvdb::Vec3i minres = ovdbMetaMap->metaValue<openvdb::Vec3i>("blender/smoke/min_resolution");
		openvdb::Vec3i maxres = ovdbMetaMap->metaValue<openvdb::Vec3i>("blender/smoke/max_resolution");

		openvdb::Vec3i base_res = ovdbMetaMap->metaValue<openvdb::Vec3i>("blender/smoke/base_resolution");

		openvdb::Mat4s obmat = ovdbMetaMap->metaValue<openvdb::Mat4s>("blender/smoke/obmat");
		openvdb::Vec3s obj_shift_f = ovdbMetaMap->metaValue<openvdb::Vec3s>("blender/smoke/obj_shift_f");

		min_bbox_list.append(min_bbox[0]);
		min_bbox_list.append(min_bbox[1]);
		min_bbox_list.append(min_bbox[2]);

		max_bbox_list.append(max_bbox[0]);
		max_bbox_list.append(max_bbox[1]);
		max_bbox_list.append(max_bbox[2]);
		
		res_list.append(res[0]);
		res_list.append(res[1]);
		res_list.append(res[2]);

		minres_list.append(minres[0]);
		minres_list.append(minres[1]);
		minres_list.append(minres[2]);
		
		maxres_list.append(maxres[0]);
		maxres_list.append(maxres[1]);
		maxres_list.append(maxres[2]);

		baseres_list.append(base_res[0]);
		baseres_list.append(base_res[1]);
		baseres_list.append(base_res[2]);

		obmat_list.append(obmat[0][0]);
		obmat_list.append(obmat[0][1]);
		obmat_list.append(obmat[0][2]);
		obmat_list.append(obmat[0][3]);

		obmat_list.append(obmat[1][0]);
		obmat_list.append(obmat[1][1]);
		obmat_list.append(obmat[1][2]);
		obmat_list.append(obmat[1][3]);

		obmat_list.append(obmat[2][0]);
		obmat_list.append(obmat[2][1]);
		obmat_list.append(obmat[2][2]);
		obmat_list.append(obmat[2][3]);

		obmat_list.append(obmat[3][0]);
		obmat_list.append(obmat[3][1]);
		obmat_list.append(obmat[3][2]);
		obmat_list.append(obmat[3][3]);

		obj_shift_f_list.append(obj_shift_f[0]);
		obj_shift_f_list.append(obj_shift_f[1]);
		obj_shift_f_list.append(obj_shift_f[2]);

		BlenderMetadata.append(min_bbox_list);
		BlenderMetadata.append(max_bbox_list);
		BlenderMetadata.append(res_list);
		BlenderMetadata.append(minres_list);
		BlenderMetadata.append(maxres_list);
		BlenderMetadata.append(baseres_list);
		BlenderMetadata.append(obmat_list);
		BlenderMetadata.append(obj_shift_f_list);
	};

	file.close();

	return boost::python::make_tuple(creator, bBox, bBox_w, trans_matrix, ovdbGrid->valueType(), BlenderMetadata);
}

//------------------------------------------------------------------------------
// Mesh conversion functions
//------------------------------------------------------------------------------

static bool Scene_DefineBlenderMesh(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t loopTriCount, const size_t loopTriPtr,
		const size_t loopPtr,
		const size_t vertPtr,
		const size_t normalPtr,
		const size_t polyPtr,
		const boost::python::object &loopUVsPtrList,
		const boost::python::object &loopColsPtrList,
		const size_t meshPtr,
		const short matIndex,
		const luxrays::Transform *trans,
		const boost::python::tuple &blenderVersion,
		const boost::python::object &loopTriCustomNormals) {

	const MLoopTri *loopTris = reinterpret_cast<const MLoopTri *>(loopTriPtr);
	const MLoop *loops = reinterpret_cast<const MLoop *>(loopPtr);
	const MVert *verts = reinterpret_cast<const MVert *>(vertPtr);
	const MPoly *polygons = reinterpret_cast<const MPoly *>(polyPtr);
	const float(*normals)[3] = nullptr;
	normals = reinterpret_cast<const float(*)[3]>(normalPtr);

	extract<boost::python::list> getUVPtrList(loopUVsPtrList);
	extract<boost::python::list> getColPtrList(loopColsPtrList);

    // Check UVs
    if (!getUVPtrList.check()) {
        const string objType = extract<string>((loopUVsPtrList.attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type for the list of UV maps of method Scene.DefineMesh(): " + objType);
    }

    const boost::python::list &UVsList = getUVPtrList();
    const boost::python::ssize_t loopUVsCount = len(UVsList);

    if (loopUVsCount > EXTMESH_MAX_DATA_COUNT) {
        throw runtime_error("Too many UV Maps in list for method Scene.DefineMesh()");
    }

    // Check vertex colors
    if (!getColPtrList.check()) {
        const string objType = extract<string>((loopColsPtrList.attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type for the list of Vertex Color maps of method Scene.DefineMesh(): " + objType);
    }

    const boost::python::list &ColsList = getColPtrList();
    const boost::python::ssize_t loopColsCount = len(ColsList);

    if (loopColsCount > EXTMESH_MAX_DATA_COUNT) {
        throw runtime_error("Too many Vertex Color Maps in list for method Scene.DefineMesh()");
    }

	vector<Normal> customNormals;
	bool hasCustomNormals = false;
	{
		const float(*loopNormals)[3] = nullptr;
		u_int loopCount = 0;
		
		if (len(blenderVersion) != 3) {
			throw runtime_error("Blender version tuple needs to have exactly 3 elements for Scene.DefineMesh()");
		}
		const int blenderVersionMajor = extract<int>(blenderVersion[0]);
		const int blenderVersionMinor = extract<int>(blenderVersion[1]);
		const int blenderVersionSub = extract<int>(blenderVersion[2]);
		
		if (blenderVersionMajor == 2 && blenderVersionMinor == 82 && blenderVersionSub == 7) {
			const blender_2_82::Mesh *mesh = reinterpret_cast<const blender_2_82::Mesh*>(meshPtr);
			loopNormals = static_cast<const float(*)[3]>(CustomData_get_layer(&mesh->ldata, blender_2_82::CD_NORMAL));
			loopCount = mesh->totloop;
		} else if (blenderVersionMajor == 2 && blenderVersionMinor == 83) {
			// Not checking the sub version here, for now we assume that these data structures stay the same across sub releases
			const blender_2_83::Mesh *mesh = reinterpret_cast<const blender_2_83::Mesh*>(meshPtr);
			loopNormals = static_cast<const float(*)[3]>(CustomData_get_layer(&mesh->ldata, blender_2_83::CD_NORMAL));
			loopCount = mesh->totloop;
		}
		
		if (loopNormals) {
			hasCustomNormals = true;
			for (u_int i = 0; i < loopCount; ++i) {
				customNormals.emplace_back(Normal(loopNormals[i][0], loopNormals[i][1], loopNormals[i][2]));
			}
		} else {
			// Fallback to Python-converted custom normals if we don't have support for the mesh layout of this Blender version
			hasCustomNormals = !loopTriCustomNormals.is_none();
			if (hasCustomNormals) {
				extract<boost::python::list> getCustomNormalsList(loopTriCustomNormals);

				if (!getCustomNormalsList.check()) {
					const string objType = extract<string>((loopUVsPtrList.attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type for the list of custom normals of method Scene.DefineMesh(): " + objType);
				}

				const boost::python::list &loopTriCustomNormalsList = getCustomNormalsList();
				const boost::python::ssize_t loopCustomNormalsCount = len(loopTriCustomNormalsList);

				for (int i = 0; i < loopCustomNormalsCount; i += 3) {
					const float x = extract<float>(loopTriCustomNormalsList[i]);
					const float y = extract<float>(loopTriCustomNormalsList[i + 1]);
					const float z = extract<float>(loopTriCustomNormalsList[i + 2]);
					customNormals.emplace_back(Normal(x, y, z));
				}
			}
		}
	}

	vector<const MLoopUV *> loopUVsList;
	vector<const MLoopCol *> loopColsList;
	vector<Point> tmpMeshVerts;
	vector<Normal> tmpMeshNorms;
	vector<vector<UV>> tmpMeshUVs;
	vector<vector<Spectrum>> tmpMeshCols;
	vector<Triangle> tmpMeshTris;

	for (u_int i = 0; i < loopUVsCount; ++i) {
		const size_t UVListPtr = extract<size_t>(UVsList[i]);
		loopUVsList.push_back(reinterpret_cast<const MLoopUV *>(UVListPtr));

		vector<UV> temp;
		tmpMeshUVs.push_back(temp);
	}

	for (u_int i = 0; i < loopColsCount; ++i) {
		const size_t ColListPtr = extract<size_t>(ColsList[i]);
		loopColsList.push_back(reinterpret_cast<const MLoopCol *>(ColListPtr));

		vector<Spectrum> temp;
		tmpMeshCols.push_back(temp);
	}

	u_int vertFreeIndex = 0;
	boost::unordered_map<u_int, u_int> vertexMap;

	const float normalScale = 1.f / 32767.f;
	const float rgbScale = 1.f / 255.f;

	for (u_int loopTriIndex = 0; loopTriIndex < loopTriCount; ++loopTriIndex) {
		const MLoopTri &loopTri = loopTris[loopTriIndex];
		const MPoly &poly = polygons[loopTri.poly];

		if (poly.mat_nr != matIndex)
			continue;

		u_int vertIndices[3];

		if ((poly.flag & ME_SMOOTH) || hasCustomNormals) {
			// Smooth shaded, use the Blender vertex normal
			for (u_int i = 0; i < 3; ++i) {
				const u_int tri = loopTri.tri[i];
				const u_int index = loops[tri].v;

				// Check if it has been already defined

				bool alreadyDefined = (vertexMap.find(index) != vertexMap.end());
				if (alreadyDefined) {
					const u_int mappedIndex = vertexMap[index];

					if (hasCustomNormals && (customNormals[tri] != tmpMeshNorms[mappedIndex]))
						alreadyDefined = false;
					
					for (u_int uvLayerIndex = 0; uvLayerIndex < loopUVsList.size() && alreadyDefined; ++uvLayerIndex) {
						const MLoopUV *loopUVs = loopUVsList[uvLayerIndex];

						if (loopUVs) {
							const MLoopUV &loopUV = loopUVs[tri];
							// Check if the already defined vertex has the right UV coordinates
							if ((loopUV.uv[0] != tmpMeshUVs[uvLayerIndex][mappedIndex].u) ||
								(loopUV.uv[1] != tmpMeshUVs[uvLayerIndex][mappedIndex].v)) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}
					}
					for (u_int colLayerIndex = 0; colLayerIndex < loopColsList.size() && alreadyDefined; ++colLayerIndex) {
						const MLoopCol *loopCols = loopColsList[colLayerIndex];

						if (loopCols) {
							const MLoopCol &loopCol = loopCols[tri];
							// Check if the already defined vertex has the right color
							if (((loopCol.r * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[0]) ||
								((loopCol.g * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[1]) ||
								((loopCol.b * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[2])) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}
					}
				}

				if (alreadyDefined)
					vertIndices[i] = vertexMap[index];
				else {
					const MVert &vertex = verts[index];

					// Add the vertex
					tmpMeshVerts.emplace_back(Point(vertex.co));

					// Add the normal
					if (hasCustomNormals) {
						tmpMeshNorms.push_back(customNormals[tri]);
					} else {
						tmpMeshNorms.push_back(Normalize(Normal(
							normals[index][0] * normalScale,
							normals[index][1] * normalScale,
							normals[index][2] * normalScale)));
					}
					
					// Add the UV
					for (u_int uvLayerIndex = 0; uvLayerIndex < loopUVsList.size(); ++uvLayerIndex) {
						const MLoopUV *loopUVs = loopUVsList[uvLayerIndex];
						if (loopUVs) {
							const MLoopUV &loopUV = loopUVs[tri];
							tmpMeshUVs[uvLayerIndex].push_back(UV(loopUV.uv));
						}
					}
					// Add the color
					for (u_int colLayerIndex = 0; colLayerIndex < loopColsList.size(); ++colLayerIndex) {
						const MLoopCol *loopCols = loopColsList[colLayerIndex];
						if (loopCols) {
							const MLoopCol &loopCol = loopCols[tri];
							tmpMeshCols[colLayerIndex].push_back(Spectrum(
								loopCol.r * rgbScale,
								loopCol.g * rgbScale,
								loopCol.b * rgbScale));
						}
					}
					// Add the vertex mapping
					const u_int vertIndex = vertFreeIndex++;
					vertexMap[index] = vertIndex;
					vertIndices[i] = vertIndex;
				}
			}
		} else {
			// Flat shaded, use the Blender face normal
			const MVert &v0 = verts[loops[loopTri.tri[0]].v];
			const MVert &v1 = verts[loops[loopTri.tri[1]].v];
			const MVert &v2 = verts[loops[loopTri.tri[2]].v];

			const Point p0(v0.co);
			const Point p1(v1.co);
			const Point p2(v2.co);

			const Vector e1 = p1 - p0;
			const Vector e2 = p2 - p0;
			Normal faceNormal(Cross(e1, e2));

			if ((faceNormal.x != 0.f) || (faceNormal.y != 0.f) || (faceNormal.z != 0.f))
				faceNormal /= faceNormal.Length();

			for (u_int i = 0; i < 3; ++i) {
				const u_int tri = loopTri.tri[i];
				const u_int index = loops[tri].v;

				// Check if it has been already defined

				bool alreadyDefined = (vertexMap.find(index) != vertexMap.end());
				if (alreadyDefined) {
					const u_int mappedIndex = vertexMap[index];

					// In order to have flat shading, we need to duplicate vertices with differing normals
					if (faceNormal != tmpMeshNorms[mappedIndex])
						alreadyDefined = false;
					
					for (u_int uvLayerIndex = 0; uvLayerIndex < loopUVsList.size() && alreadyDefined; ++uvLayerIndex) {
						const MLoopUV * loopUVs = loopUVsList[uvLayerIndex];

						if (loopUVs) {
							const MLoopUV &loopUV = loopUVs[tri];
							// Check if the already defined vertex has the right UV coordinates
							if ((loopUV.uv[0] != tmpMeshUVs[uvLayerIndex][mappedIndex].u) ||
								(loopUV.uv[1] != tmpMeshUVs[uvLayerIndex][mappedIndex].v)) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}
					}
					for (u_int colLayerIndex = 0; colLayerIndex < loopColsList.size() && alreadyDefined; ++colLayerIndex) {
						const MLoopCol * loopCols = loopColsList[colLayerIndex];

						if (loopCols) {
							const MLoopCol &loopCol = loopCols[tri];
							// Check if the already defined vertex has the right color
							if (((loopCol.r * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[0]) ||
								((loopCol.g * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[1]) ||
								((loopCol.b * rgbScale) != tmpMeshCols[colLayerIndex][mappedIndex].c[2])) {
								// I have to create a new vertex
								alreadyDefined = false;
							}
						}
					}
				}

				if (alreadyDefined)
					vertIndices[i] = vertexMap[index];
				else {
					const MVert &vertex = verts[index];

					// Add the vertex
					tmpMeshVerts.emplace_back(Point(vertex.co));
					// Add the normal (same for all vertices of this face, to have flat shading)
					tmpMeshNorms.push_back(faceNormal);

					// Add the UV
					for (u_int uvLayerIndex = 0; uvLayerIndex < loopUVsList.size(); ++uvLayerIndex) {
						const MLoopUV * loopUVs = loopUVsList[uvLayerIndex];
						if (loopUVs) {
							const MLoopUV &loopUV = loopUVs[tri];
							tmpMeshUVs[uvLayerIndex].push_back(UV(loopUV.uv));
						}
					}
					// Add the color
					for (u_int colLayerIndex = 0; colLayerIndex < loopColsList.size(); ++colLayerIndex) {
						const MLoopCol * loopCols = loopColsList[colLayerIndex];
						if (loopCols) {
							const MLoopCol &loopCol = loopCols[tri];
							tmpMeshCols[colLayerIndex].push_back(Spectrum(
								loopCol.r * rgbScale,
								loopCol.g * rgbScale,
								loopCol.b * rgbScale));
						}
					}
					// Add the vertex mapping
					const u_int vertIndex = vertFreeIndex++;
					vertexMap[index] = vertIndex;
					vertIndices[i] = vertIndex;
				}
			}
		}

		tmpMeshTris.emplace_back(Triangle(vertIndices[0], vertIndices[1], vertIndices[2]));
	}

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

	array<UV *, EXTMESH_MAX_DATA_COUNT> meshUVs;
	array<Spectrum *, EXTMESH_MAX_DATA_COUNT> meshCols;
	
	fill(meshUVs.begin(), meshUVs.end(), nullptr);
	fill(meshCols.begin(), meshCols.end(), nullptr);
	
	for (u_int i = 0; i < loopUVsList.size(); ++i) {
		const MLoopUV * loopUVs = loopUVsList[i];
		if (loopUVs) {
			meshUVs[i] = new UV[tmpMeshVerts.size()];
			copy(tmpMeshUVs[i].begin(), tmpMeshUVs[i].end(), meshUVs[i]);
		}
	}
	for (u_int i = 0; i < loopColsList.size(); ++i) {
		const MLoopCol * loopCols = loopColsList[i];
		if (loopCols) {
			meshCols[i] = new Spectrum[tmpMeshVerts.size()];
			copy(tmpMeshCols[i].begin(), tmpMeshCols[i].end(), meshCols[i]);
		}
	}

	luxrays::ExtTriangleMesh *mesh = new luxrays::ExtTriangleMesh(tmpMeshVerts.size(),
		tmpMeshTris.size(), meshVerts, meshTris,
		meshNorms, &meshUVs, &meshCols, NULL);

	// Apply the transformation if required
	if (trans)
		mesh->ApplyTransform(*trans);

	mesh->SetName(name);
	scene->DefineMesh(mesh);

	return true;
}

boost::python::list Scene_DefineBlenderMesh1(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t loopTriCount, const size_t loopTriPtr,
		const size_t loopPtr,
		const size_t vertPtr,
		const size_t normalPtr,
		const size_t polyPtr,
		const boost::python::object &loopUVsPtrList,
		const boost::python::object &loopColsPtrList,
		const size_t meshPtr,
		const u_int materialCount,
		const boost::python::object &transformation,
		const boost::python::tuple &blenderVersion,
		const boost::python::object &loopTriCustomNormals) {
	
	// Get the transformation if required
	bool hasTransformation = false;
	Transform trans;
	if (!transformation.is_none()) {
		trans = ExtractTransformation(transformation);
		hasTransformation = true;
	}

	boost::python::list result;
	for (u_int matIndex = 0; matIndex < materialCount; ++matIndex) {
		const string meshName = (boost::format(name + "%03d") % matIndex).str();

		if (Scene_DefineBlenderMesh(scene, meshName, loopTriCount, loopTriPtr,
				loopPtr, vertPtr, normalPtr, polyPtr,
				loopUVsPtrList, loopColsPtrList, 
				meshPtr,
				matIndex,
				hasTransformation ? &trans : NULL,
				blenderVersion,
				loopTriCustomNormals)) {
			boost::python::list meshInfo;
			meshInfo.append(meshName);
			meshInfo.append(matIndex);
			result.append(meshInfo);
		}
	}

	return result;
}

boost::python::list Scene_DefineBlenderMesh2(luxcore::detail::SceneImpl *scene, const string &name,
		const size_t loopTriCount, const size_t loopTriPtr,
		const size_t loopPtr,
		const size_t vertPtr,
		const size_t normalPtr,	
		const size_t polyPtr,
		const boost::python::object &loopUVsPtrList,
		const boost::python::object &loopColsPtrList,
		const size_t meshPtr,
		const u_int materialCount,
		const boost::python::tuple &blenderVersion,
		const boost::python::object &loopTriCustomNormals) {
	return Scene_DefineBlenderMesh1(scene, name, loopTriCount, loopTriPtr,
		loopPtr, vertPtr, normalPtr, polyPtr, loopUVsPtrList, loopColsPtrList,
		meshPtr, materialCount, boost::python::object(), blenderVersion, 
		loopTriCustomNormals);
}

//------------------------------------------------------------------------------
// Hair/strands conversion functions
//------------------------------------------------------------------------------

bool nearlyEqual(const float a, const float b, const float epsilon) {
	return fabs(a - b) < epsilon;
}

Spectrum getColorFromImage(const vector<float> &imageData, const float gamma,
	                       const u_int width, const u_int height, const u_int channelCount,
	                       const float u, const float v) {
	assert (width > 0);
	assert (height > 0);
	
 	const u_int x = u * (width - 1);
	// The pixels coming from OIIO are flipped in y direction, so we flip v
	const u_int y = (1.f - v) * (height - 1);
	assert (x >= 0);
	assert (x < width);
	assert (y >= 0);
	assert (y < height);
	
	const u_int index = (width * y + x) * channelCount;
	
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
		const boost::python::object &transformation,
		const float strandDiameter,
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
	
	// UVs
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

	// Transformation
	bool hasTransformation = false;
	Transform trans;
	if (!transformation.is_none()) {
		trans = ExtractTransformation(transformation);
		hasTransformation = true;
	}
	
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
		unique_ptr<ImageInput> in(ImageInput::open(imageFilename, &config));
		
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
			// Bring u and v into range 0..1
			u -= floor(u);
			v -= floor(v);
		}
		if (useVertexCols) {
			r = *colorPtr++;
			g = *colorPtr++;
			b = *colorPtr++;
		}
		
		Point currPoint = Point(pointPtr);
		if (hasTransformation)
			currPoint *= trans;
		pointPtr += pointStride;
		Point lastPoint;
		
		// Iterate over the strand. We can skip step == 0.
		for (u_int step = 1; step < pointsPerStrand; ++step) {
			lastPoint = currPoint;
			currPoint = Point(pointPtr);
			if (hasTransformation)
				currPoint *= trans;
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
