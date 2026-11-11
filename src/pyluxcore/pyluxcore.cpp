/***************************************************************************
 * Copyright 1998-2025 by authors (see AUTHORS.txt)            *
 *                                     *
 *   This file is part of LuxCoreRender.                   *
 *                                     *
 * Licensed under the Apache License, Version 2.0 (the "License");     *
 * you may not use this file except in compliance with the License.    *
 * You may obtain a copy of the License at                 *
 *                                     *
 *   http://www.apache.org/licenses/LICENSE-2.0              *
 *                                     *
 * Unless required by applicable law or agreed to in writing, software   *
 * distributed under the License is distributed on an "AS IS" BASIS,     *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.*
 * See the License for the specific language governing permissions and   *
 * limitations under the License.                      *
 ***************************************************************************/
#include "luxrays/core/exttrianglemesh.h"
#include "luxrays/core/trianglemesh.h"
#include "luxrays/utils/properties.h"
#include <iterator>
#include <pybind11/detail/common.h>
#include <pybind11/detail/using_smart_holder.h>
#include <string_view>
#define PYBIND11_DETAILED_ERROR_MESSAGES

#ifdef WIN32
// Python 3.8 and older define snprintf as a macro even for VS 2015 and newer
// where this causes an error - See https://bugs.python.org/issue36020
#if defined(_MSC_VER) && _MSC_VER >= 1900
#define HAVE_SNPRINTF
#endif
#endif

#include <locale>
#include <memory>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include <pybind11/operators.h>
#include <pybind11/typing.h>

#include <openvdb/openvdb.h>
#include <openvdb/io/File.h>


#include "luxrays/luxrays.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/pyluxcore/pyluxcoreforblender.h"
#include "slg/utils/group_by_equivalence.h"
#include "luxcore/pyluxcore/pyluxcoreutils.h"
#include "oneapi/tbb/parallel_sort.h"
#include <utility>

using namespace luxcore;
using namespace luxcore::detail;
namespace py = pybind11;

using PropertyUPtr = std::unique_ptr<luxrays::Property>;
using PropertyRPtr = const std::unique_ptr<luxrays::Property> &;
using SceneImplPtr = std::unique_ptr<luxcore::detail::SceneImpl>;
using PropertiesUPtr = std::unique_ptr<luxrays::Properties>;


namespace luxcore {

//------------------------------------------------------------------------------
// Module functions
//------------------------------------------------------------------------------

static std::mutex luxCoreInitMutex;
static py::object luxCoreLogHandler;

#if (PY_VERSION_HEX >= 0x03040000)
static void PythonDebugHandler(const char *msg) {
  // PyGILState_Check() is available since Python 3.4
  if (PyGILState_Check())
    luxCoreLogHandler(std::string(msg));
  else {
    // The following code is supposed to work ... but it doesn't (it never
    // returns). So I'm just avoiding to call Python without the GIL and
    // I silently discard the message.

    //PyGILState_STATE state = PyGILState_Ensure();
    //luxCoreLogHandler(std::string(msg));
    //PyGILState_Release(state);
  }
}
#else
static int PyGILState_Check2(void) {
  PyThreadState *tstate = _PyThreadState_Current;
  return tstate && (tstate == PyGILState_GetThisThreadState());
}

static void PythonDebugHandler(const char *msg) {
  if (PyGILState_Check2())
    luxCoreLogHandler(std::string(msg));
}
#endif

static void LuxCore_Init() {
  std::unique_lock<std::mutex> lock(luxCoreInitMutex);
  Init();
}

static void LuxCore_InitDefaultHandler(py::object &logHandler) {
  std::unique_lock<std::mutex> lock(luxCoreInitMutex);
  // I wonder if I should increase the reference count for Python
  luxCoreLogHandler = logHandler;

  Init(&PythonDebugHandler);
}

static void LuxCore_SetLogHandler(py::object &logHandler) {
  luxCoreLogHandler = logHandler;

  if (logHandler.is_none())
    SetLogHandler(NULL);
}

static const char *LuxCoreVersion() {
  static const char *luxCoreVersion = LUXCORE_VERSION;
  return luxCoreVersion;
}

// Union-find over [0, num_elements) given pairs of equivalent elements
// (parallel C++ implementation, slg::equiv::GroupByEquivalence).
// Returns an int32 array with the component label of each element.
static py::array_t<int> union_find_labels(
	const size_t num_elements,
	const py::array_t<uint32_t, py::array::c_style> &pairs
) {
	if (pairs.ndim() != 2 || pairs.shape(1) != 2)
		throw std::runtime_error("union_find: pairs must be a [n, 2] uint32 array");
	if (num_elements > std::numeric_limits<uint32_t>::max())
		throw std::runtime_error("union_find: num_elements too large");

	std::vector<slg::equiv::Relation> rel;
	rel.reserve(pairs.shape(0));
	auto p = pairs.unchecked<2>();
	for (size_t i = 0; i < (size_t)pairs.shape(0); ++i)
		rel.emplace_back(p(i, 0), p(i, 1));

	py::array_t<int> out(num_elements);
	auto l = out.mutable_unchecked<1>();
	{
		// The computation is pure C++ (and internally TBB-parallel):
		// release the GIL so other Python threads can run meanwhile
		Py_BEGIN_ALLOW_THREADS
		// Flat per-element labels directly (no Classes structure)
		std::vector<uint32_t> labels = slg::equiv::GroupByEquivalenceLabels(
			num_elements, std::span<const slg::equiv::Relation>(rel));
		for (size_t i = 0; i < num_elements; ++i)
			l[i] = static_cast<int>(labels[i]);
		Py_END_ALLOW_THREADS
	}
	return out;
}

// Parallel argsort of a 1-D array (tbb::parallel_sort over (key, index)
// pairs; ties broken by index, so the result is deterministic). Returns
// the int64 indices that would sort the array. The GIL is released during
// the sort. numpy's argsort holds the GIL, so this allows Python threads
// to overlap sorting work.
template <typename T>
static py::array_t<int64_t> ParallelArgsortImpl(const T *keys, const size_t n) {
	py::array_t<int64_t> out(n);
	auto o = out.mutable_unchecked<1>();
	std::vector<std::pair<T, int64_t>> pairs(n);
	for (size_t i = 0; i < n; ++i)
		pairs[i] = std::make_pair(keys[i], static_cast<int64_t>(i));
	{
		Py_BEGIN_ALLOW_THREADS
		tbb::parallel_sort(pairs.begin(), pairs.end());
		for (size_t i = 0; i < n; ++i)
			o(i) = pairs[i].second;
		Py_END_ALLOW_THREADS
	}
	return out;
}

// Two-key variant: lexicographic (keys, secondary) order, ties broken by
// index (deterministic). Same GIL release as ParallelArgsortImpl.
template <typename T, typename U>
static py::array_t<int64_t> ParallelArgsort2Impl(
	const T *keys, const U *secondary, const size_t n
) {
	py::array_t<int64_t> out(n);
	auto o = out.mutable_unchecked<1>();
	std::vector<std::tuple<T, U, int64_t>> items(n);
	for (size_t i = 0; i < n; ++i)
		items[i] = std::make_tuple(keys[i], secondary[i], static_cast<int64_t>(i));
	{
		Py_BEGIN_ALLOW_THREADS
		tbb::parallel_sort(items.begin(), items.end());
		for (size_t i = 0; i < n; ++i)
			o(i) = std::get<2>(items[i]);
		Py_END_ALLOW_THREADS
	}
	return out;
}

template <typename T>
static py::array_t<int64_t> ParallelArgsortSecondaryDispatch(
	const T *keys, const py::array &secondary
) {
	const size_t n = secondary.shape(0);
	const char kind = secondary.dtype().kind();
	const size_t itemsize = secondary.dtype().itemsize();
	const void *data = secondary.data();
	if (kind == 'u' && itemsize == 8)
		return ParallelArgsort2Impl<T, uint64_t>(keys, static_cast<const uint64_t *>(data), n);
	if (kind == 'i' && itemsize == 8)
		return ParallelArgsort2Impl<T, int64_t>(keys, static_cast<const int64_t *>(data), n);
	if (kind == 'u' && itemsize == 4)
		return ParallelArgsort2Impl<T, uint32_t>(keys, static_cast<const uint32_t *>(data), n);
	if (kind == 'i' && itemsize == 4)
		return ParallelArgsort2Impl<T, int32_t>(keys, static_cast<const int32_t *>(data), n);
	if (kind == 'f' && itemsize == 4)
		return ParallelArgsort2Impl<T, float>(keys, static_cast<const float *>(data), n);
	if (kind == 'f' && itemsize == 8)
		return ParallelArgsort2Impl<T, double>(keys, static_cast<const double *>(data), n);
	throw std::runtime_error(
		"parallel_argsort: unsupported secondary dtype (need 4/8-byte int or float)");
}

static py::array_t<int64_t> parallel_argsort(
	const py::array &keys, const py::object &secondary
) {
	if (keys.ndim() != 1)
		throw std::runtime_error("parallel_argsort: keys must be a 1-D array");
	const size_t n = keys.shape(0);
	const char kind = keys.dtype().kind();
	const size_t itemsize = keys.dtype().itemsize();
	const void *data = keys.data();
	if (!secondary.is_none()) {
		const py::array sec = py::cast<py::array>(secondary);
		if (sec.ndim() != 1 || sec.shape(0) != n)
			throw std::runtime_error("parallel_argsort: secondary must be a 1-D array of the same length");
		if (kind == 'u' && itemsize == 8)
			return ParallelArgsortSecondaryDispatch<uint64_t>(static_cast<const uint64_t *>(data), sec);
		if (kind == 'i' && itemsize == 8)
			return ParallelArgsortSecondaryDispatch<int64_t>(static_cast<const int64_t *>(data), sec);
		throw std::runtime_error(
			"parallel_argsort: unsupported keys dtype for two-key sort (need 8-byte int)");
	}
	if (kind == 'u' && itemsize == 8)
		return ParallelArgsortImpl<uint64_t>(static_cast<const uint64_t *>(data), n);
	if (kind == 'i' && itemsize == 8)
		return ParallelArgsortImpl<int64_t>(static_cast<const int64_t *>(data), n);
	if (kind == 'f' && itemsize == 8)
		return ParallelArgsortImpl<double>(static_cast<const double *>(data), n);
	if (kind == 'f' && itemsize == 4)
		return ParallelArgsortImpl<float>(static_cast<const float *>(data), n);
	throw std::runtime_error(
		"parallel_argsort: unsupported dtype (need 4/8-byte int or float)");
}

static py::list GetOpenCLDeviceList() {
  luxrays::Context ctx;
  auto deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

  // Select only OpenCL devices
  luxrays::DeviceDescription::Filter(
		(luxrays::DeviceType)(luxrays::DEVICE_TYPE_OPENCL_ALL | luxrays::DEVICE_TYPE_CUDA_ALL),
		deviceDescriptions
	);

  // Add all device information to the list
  py::list l;
  for (luxrays::DeviceDescriptionRef desc : deviceDescriptions) {
    l.append(py::make_tuple(
        desc.GetName(),
        luxrays::DeviceDescription::GetDeviceType(desc.GetType()),
        desc.GetComputeUnits(),
        desc.GetNativeVectorWidthFloat(),
        desc.GetMaxMemory(),
        desc.GetMaxMemoryAllocSize()));
  }

  return l;
}

static void LuxCore_KernelCacheFill1() {
  KernelCacheFill(std::make_unique<Properties>());
}

static void LuxCore_KernelCacheFill2(PropertiesRPtr config) {
  KernelCacheFill(config);
}

//------------------------------------------------------------------------------
// OpenVDB helper functions
//------------------------------------------------------------------------------
py::list GetOpenVDBGridNames(const std::string &filePathStr) {
  py::list gridNames;

  openvdb::io::File file(filePathStr);
  file.open();
  for (auto i = file.beginName(); i != file.endName(); ++i)
    gridNames.append(*i);

  file.close();
  return gridNames;
}


py::tuple GetOpenVDBGridInfo(const std::string &filePathStr, const std::string &gridName) {
  py::list bBox;
  py::list bBox_w;
  py::list trans_matrix;
  py::list BlenderMetadata;
  openvdb::io::File file(filePathStr);
  
  file.open();
  openvdb::MetaMap::Ptr ovdbMetaMap = file.getMetadata();

  std::string creator = "";
  try {
    creator = ovdbMetaMap->metaValue<std::string>("creator");
  } catch (openvdb::LookupError &e) {
    std::cout << "No creator file meta data found in OpenVDB file " + filePathStr << std::endl;
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
    py::list min_bbox_list;
    py::list max_bbox_list;
    py::list res_list;
    py::list minres_list;
    py::list maxres_list;
    py::list baseres_list;
    py::list obmat_list;
    py::list obj_shift_f_list;

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

  return py::make_tuple(creator, bBox, bBox_w, trans_matrix, ovdbGrid->valueType(), BlenderMetadata);
}

//------------------------------------------------------------------------------
// Glue for Property class
//------------------------------------------------------------------------------

static py::list Property_GetBlobByIndex(PropertyRPtr prop, const size_t i) {
  const luxrays::Blob &blob = prop->Get<const luxrays::Blob &>(i);
  const char *data = blob.GetData();
  const size_t size = blob.GetSize();

  py::list l;
  for (size_t i = 0; i < size; ++i)
    l.append((int)data[i]);

  return l;
}

static py::list Property_Get(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i) {
    const auto dataType = prop->GetValueType(i);

    switch (dataType) {
      case luxrays::PropertyValue::BOOL_VAL:
        l.append(prop->Get<bool>(i));
        break;
      case luxrays::PropertyValue::INT_VAL:
      case luxrays::PropertyValue::LONGLONG_VAL:
        l.append(prop->Get<long long>(i));
        break;
      case luxrays::PropertyValue::DOUBLE_VAL:
        l.append(prop->Get<double>(i));
        break;
      case luxrays::PropertyValue::STRING_VAL:
        l.append(prop->Get<std::string>(i));
        break;
      case luxrays::PropertyValue::BLOB_VAL:
        l.append(Property_GetBlobByIndex(prop, i));
        break;
      default:
        throw std::runtime_error("Unsupported data type in list extraction of a Property: " + prop->GetName());
    }
  }

  return l;
}

static py::list Property_GetBools(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<bool>(i));
  return l;
}

static py::list Property_GetInts(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<long long>(i));
  return l;
}

static py::list Property_GetFloats(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<double>(i));
  return l;
}

static py::list Property_GetStrings(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<std::string>(i));
  return l;
}

static py::list Property_GetBlobs(PropertyRPtr prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(Property_GetBlobByIndex(prop, i));
  return l;
}

static bool Property_GetBool(PropertyRPtr prop) {
  return prop->Get<bool>(0);
}

static long long Property_GetInt(PropertyRPtr prop) {
  return prop->Get<long long>(0);
}

static unsigned long long Property_GetUnsignedLongLong(PropertyRPtr prop) {
  return prop->Get<unsigned long long>(0);
}

static double Property_GetFloat(PropertyRPtr prop) {
  return prop->Get<double>(0);
}

static std::string Property_GetString(PropertyRPtr prop) {
  return prop->Get<std::string>(0);
}

static py::list Property_GetBlob(PropertyRPtr prop) {
  return Property_GetBlobByIndex(prop, 0);
}

static PropertyRPtr Property_Add(PropertyRPtr prop, const py::list &l) {
  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
    const py::object obj = l[i];

    if (objType == "bool") {
      const bool v = py::cast<bool>(obj);
      prop->Add(v);
    } else if (objType == "int") {
      const long long v = py::cast<long long>(obj);
      prop->Add(v);
    } else if (objType == "float") {
      const double v = py::cast<double>(obj);
      prop->Add(v);
    } else if (objType == "str") {
      const std::string v = py::cast<std::string>(obj);
      prop->Add(v);
    } else if (objType == "list") {
      const py::list ol = py::cast<py::list>(obj);

      const py::ssize_t os = len(ol);

      std::vector<char> data(os);
      for (py::ssize_t i = 0; i < os; ++i)
        data[i] = py::cast<int>(ol[i]);

      prop->Add(std::make_shared<luxrays::Blob>(&data[0], os));
    } else if (PyObject_CheckBuffer(obj.ptr())) {
      Py_buffer view;
      if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
        const char *buffer = (char *)view.buf;
        const size_t size = (size_t)view.len;

        auto blob = std::make_shared<luxrays::Blob>(buffer, size);
        prop->Add(blob);

        PyBuffer_Release(&view);
      } else
        throw std::runtime_error("Unable to get a data view in Property.Add() method: " + objType);
    } else
      throw std::runtime_error("Unsupported data type included in Property.Add() method list: " + objType);
  }

  return prop;
}

static PropertyRPtr Property_AddAllBool(PropertyRPtr prop,
    const py::object &obj) {
  std::vector<bool> v;
  GetArray<bool>(obj, v);

  for (auto e : v)
    prop->Add<bool>(e);

  return prop;
}

static PropertyRPtr Property_AddAllInt(PropertyRPtr prop,
    const py::object &obj) {
  std::vector<long long> v;
  GetArray<long long>(obj, v);

  for (auto e : v)
    prop->Add<long long>(e);

  return prop;
}

static PropertyRPtr Property_AddAllUnsignedLongLong(PropertyRPtr prop,
    const py::object &obj) {
  std::vector<unsigned long long> v;
  GetArray<unsigned long long>(obj, v);

  for (auto e : v)
    prop->Add<unsigned long long>(e);

  return prop;
}

static PropertyRPtr Property_AddAllFloat(PropertyRPtr prop,
    const py::object &obj) {
  std::vector<float> v;
  GetArray<float>(obj, v);

  for (auto e : v)
    prop->Add<float>(e);

  return prop;
}

static PropertyRPtr Property_AddAllBoolStride(PropertyRPtr prop,
    const py::object &obj, const size_t width, const size_t stride) {
  std::vector<bool> v;
  GetArray<bool>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<bool>(e);

  return prop;
}

static PropertyRPtr Property_AddAllIntStride(PropertyRPtr prop,
    const py::object &obj, const size_t width, const size_t stride) {
  std::vector<long long> v;
  GetArray<long long>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<long long>(e);

  return prop;
}

static PropertyRPtr Property_AddAllUnsignedLongLongStride(PropertyRPtr prop,
    const py::object &obj, const size_t width, const size_t stride) {
  std::vector<unsigned long long> v;
  GetArray<unsigned long long>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<unsigned long long>(e);

  return prop;
}

static PropertyRPtr Property_AddAllFloatStride(PropertyRPtr prop,
    const py::object &obj, const size_t width, const size_t stride) {
  std::vector<float> v;
  GetArray<float>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<float>(e);

  return prop;
}

static PropertyRPtr Property_Set(PropertyRPtr prop, const size_t i,
    const py::object &obj) {
  const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));

  if (objType == "bool") {
    const bool v = py::cast<bool>(obj);
    prop->Set(i, v);
  } else if (objType == "int") {
    const long long v = py::cast<long long>(obj);
    prop->Set(i, v);
  } else if (objType == "float") {
    const double v = py::cast<double>(obj);
    prop->Set(i, v);
  } else if (objType == "str") {
    const std::string v = py::cast<std::string>(obj);
    prop->Set(i, v);
  } else if (objType == "list") {
    const py::list ol = py::cast<py::list>(obj);

    const py::ssize_t os = len(ol);

    std::vector<char> data(os);
    for (py::ssize_t i = 0; i < os; ++i)
      data[i] = py::cast<int>(ol[i]);

    prop->Set(i, std::make_shared<luxrays::Blob>(&data[0], os));
  } else if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      const char *buffer = (char *)view.buf;
      const size_t size = (size_t)view.len;

      auto blob = std::make_shared<luxrays::Blob>(buffer, size);
      prop->Set(i, blob);

      PyBuffer_Release(&view);
    } else
      throw std::runtime_error("Unable to get a data view in Property->Set() method: " + objType);
  } else
    throw std::runtime_error("Unsupported data type used for Property->Set() method: " + objType);

  return prop;
}

static PropertyRPtr Property_Set(PropertyRPtr prop, const py::list &l) {
  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const py::object obj = l[i];
    Property_Set(prop, i, obj);
  }

  return prop;
}

static PropertyUPtr Property_InitWithList(const py::str &name, const py::list &l) {
  auto prop = std::make_unique<luxrays::Property>(py::cast<std::string>(name));

  Property_Add(prop, l);

  return prop;
}

//------------------------------------------------------------------------------
// Glue for Properties class
//------------------------------------------------------------------------------


static luxrays::Property Properties_GetWithDefaultValues(luxrays::PropertiesRPtr props,
    const std::string &name, const py::list &l) {
  luxrays::PropertyValues values;

  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "bool") {
      const bool v = py::cast<bool>(l[i]);
      values.push_back(v);
    } else if (objType == "int") {
      const long long v = py::cast<long long>(l[i]);
      values.push_back(v);
    } else if (objType == "float") {
      const double v = py::cast<double>(l[i]);
      values.push_back(v);
    } else if (objType == "str") {
      const std::string v = py::cast<std::string>(l[i]);
      values.push_back(v);
    } else
      throw std::runtime_error("Unsupported data type included in Properties Get with default method: " + objType);
  }

  return props->Get(luxrays::Property(name, values));
}


//------------------------------------------------------------------------------
// Glue for Film class
//------------------------------------------------------------------------------

// Blender bgl.Buffer definition

typedef struct {
  PyObject_VAR_HEAD
  PyObject *parent;

  int type;    /* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
  int ndimensions;
  int *dimensions;

  union {
    char *asbyte;
    short *asshort;
    int *asint;
    float *asfloat;
    double *asdouble;

    void *asvoid;
  } buf;
} BGLBuffer;

//------------------------------------------------------------------------------
// File GetOutput() related functions
//------------------------------------------------------------------------------

static void Film_GetOutputFloat1(
	luxcore::detail::FilmImpl& film,
	const Film::FilmOutputType type,
    py::object &obj,
	const size_t index,
	const bool executeImagePipeline
) {
  const size_t outputSize = film.GetOutputSize(type) * sizeof(float);

  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= outputSize) {
        if(!film.HasOutput(type)) {
          const std::string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw std::runtime_error(errorMsg);
        }

        float *buffer = (float *)view.buf;

        film.GetOutput<float>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const std::string errorMsg = "Not enough space in the buffer of Film.GetOutputFloat() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(outputSize);
        PyBuffer_Release(&view);

        throw std::runtime_error(errorMsg);
      }
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Film.GetOutputFloat() method: " + objType);
    }
  } else {
    const PyObject *pyObj = obj.ptr();
    const PyTypeObject *pyTypeObj = Py_TYPE(pyObj);

    // Check if it is a Blender bgl.Buffer object
    if (strcmp(pyTypeObj->tp_name, "bgl.Buffer") == 0) {
      // This is a special path for optimizing Blender preview
      const BGLBuffer *bglBuffer = (BGLBuffer *)pyObj;

      // A safety check for buffer type and size
      // 0x1406 is the value of GL_FLOAT
      if (bglBuffer->type == 0x1406) {
        if (bglBuffer->ndimensions == 1) {
          if (bglBuffer->dimensions[0] * sizeof(float) >= outputSize) {
            if(!film.HasOutput(type)) {
              throw std::runtime_error("Film Output not available: " + luxrays::ToString(type));
            }

            film.GetOutput<float>(type, bglBuffer->buf.asfloat, index, executeImagePipeline);
          } else
            throw std::runtime_error("Not enough space in the Blender bgl.Buffer of Film.GetOutputFloat() method: " +
                luxrays::ToString(bglBuffer->dimensions[0] * sizeof(float)) + " instead of " + luxrays::ToString(outputSize));
        } else
          throw std::runtime_error("A Blender bgl.Buffer has the wrong dimension in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->ndimensions));
      } else
        throw std::runtime_error("A Blender bgl.Buffer has the wrong type in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->type));
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
    }
  }
}

static void Film_GetOutputFloat2(
	FilmImpl & film,
	const Film::FilmOutputType type,
    py::object &obj
) {
  Film_GetOutputFloat1(film, type, obj, 0, true);
}

static void Film_GetOutputFloat3(
    FilmImpl & film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index
) {
  Film_GetOutputFloat1(film, type, obj, index, true);
}

static void Film_GetOutputUInt1(
    FilmImpl & film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index,
    const bool executeImagePipeline) {
  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= film.GetOutputSize(type) * sizeof(u_int)) {
        if(!film.HasOutput(type)) {
          const std::string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw std::runtime_error(errorMsg);
        }

        u_int *buffer = (u_int *)view.buf;

        film.GetOutput<unsigned int>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const std::string errorMsg = "Not enough space in the buffer of Film.GetOutputUInt() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(film.GetOutputSize(type) * sizeof(u_int));
        PyBuffer_Release(&view);

        throw std::runtime_error(errorMsg);
      }
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Film.GetOutputUInt() method: " + objType);
    }
  } else {
    const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unsupported data type in Film.GetOutputUInt() method: " + objType);
  }
}

static void Film_GetOutputUInt2(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_GetOutputUInt1(film, type, obj, 0, true);
}

static void Film_GetOutputUInt3(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_GetOutputUInt1(film, type, obj, index, true);
}

//------------------------------------------------------------------------------
// File UpdateOutput() related functions
//------------------------------------------------------------------------------

static void Film_UpdateOutputFloat1(
    FilmImpl & film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index,
    const bool executeImagePipeline) {
  const size_t outputSize = film.GetOutputSize(type) * sizeof(float);

  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= outputSize) {
        if(!film.HasOutput(type)) {
          const std::string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw std::runtime_error(errorMsg);
        }

        float *buffer = (float *)view.buf;

        film.UpdateOutput<float>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const std::string errorMsg = "Not enough space in the buffer of Film.UpdateOutputFloat() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(outputSize);
        PyBuffer_Release(&view);

        throw std::runtime_error(errorMsg);
      }
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Film.UpdateOutputFloat() method: " + objType);
    }
  } else {
    const PyObject *pyObj = obj.ptr();
    const PyTypeObject *pyTypeObj = Py_TYPE(pyObj);

    // Check if it is a Blender bgl.Buffer object
    if (strcmp(pyTypeObj->tp_name, "bgl.Buffer") == 0) {
      // This is a special path for optimizing Blender preview
      const BGLBuffer *bglBuffer = (BGLBuffer *)pyObj;

      // A safety check for buffer type and size
      // 0x1406 is the value of GL_FLOAT
      if (bglBuffer->type == 0x1406) {
        if (bglBuffer->ndimensions == 1) {
          if (bglBuffer->dimensions[0] * sizeof(float) >= outputSize) {
            if(!film.HasOutput(type)) {
              throw std::runtime_error("Film Output not available: " + luxrays::ToString(type));
            }

            film.UpdateOutput<float>(type, bglBuffer->buf.asfloat, index, executeImagePipeline);
          } else
            throw std::runtime_error("Not enough space in the Blender bgl.Buffer of Film.UpdateOutputFloat() method: " +
                luxrays::ToString(bglBuffer->dimensions[0] * sizeof(float)) + " instead of " + luxrays::ToString(outputSize));
        } else
          throw std::runtime_error("A Blender bgl.Buffer has the wrong dimension in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->ndimensions));
      } else
        throw std::runtime_error("A Blender bgl.Buffer has the wrong type in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->type));
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
    }
  }
}

static void Film_UpdateOutputFloat2(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_UpdateOutputFloat1(film, type, obj, 0, false);
}

static void Film_UpdateOutputFloat3(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_UpdateOutputFloat1(film, type, obj, index, false);
}

static void Film_UpdateOutputUInt1(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj, const size_t index, const bool executeImagePipeline) {
  throw std::runtime_error("Film Output not available: " + luxrays::ToString(type));
}

static void Film_UpdateOutputUInt2(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_UpdateOutputUInt1(film, type, obj, 0, false);
}

static void Film_UpdateOutputUInt3(FilmImpl & film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_UpdateOutputUInt1(film, type, obj, index, false);
}

//------------------------------------------------------------------------------

static void Film_AddFilm1(
	std::unique_ptr<luxcore::detail::FilmImpl> film,
	std::unique_ptr<luxcore::detail::FilmImpl> srcFilm
) {
  film->AddFilm(*srcFilm);
}

static void Film_AddFilm2(
	std::unique_ptr<luxcore::detail::FilmImpl> film,
	std::unique_ptr<luxcore::detail::FilmImpl> srcFilm,
    const size_t srcOffsetX, const size_t srcOffsetY,
    const size_t srcWidth, const size_t srcHeight,
    const size_t dstOffsetX, const size_t dstOffsetY
) {
	film->AddFilm(
		*srcFilm, srcOffsetX, srcOffsetY, srcWidth,  srcHeight, dstOffsetX,  dstOffsetY
	);
}

static float Film_GetFilmY1(FilmImpl & film) {
  return film.GetFilmY();
}

static float Film_GetFilmY2(FilmImpl & film, const size_t imagePipelineIndex) {
  return film.GetFilmY(imagePipelineIndex);
}

static void Film_ApplyOIDN(
	FilmImpl & film,
	const size_t imagePipelineIndex
) {
	film.ApplyOIDN(imagePipelineIndex);
}

//------------------------------------------------------------------------------
// Glue for Camera class
//------------------------------------------------------------------------------

static void Camera_Translate(luxcore::detail::CameraImpl *camera, const py::tuple t) {
  camera->Translate(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
}

static void Camera_Rotate(luxcore::detail::CameraImpl *camera, const float angle, const py::tuple axis) {
  camera->Rotate(angle, py::cast<float>(axis[0]), py::cast<float>(axis[1]), py::cast<float>(axis[2]));
}

//------------------------------------------------------------------------------
// Glue for Scene class
//------------------------------------------------------------------------------

static luxcore::detail::CameraImpl & Scene_GetCamera(const SceneImplPtr & scene) {
  return dynamic_cast<luxcore::detail::CameraImpl &>(scene->GetCamera());
}


static void Scene_DefineImageMap(
	const SceneImplPtr & scene,
	const std::string &imgMapName,
    py::object &obj,
	const float gamma,
    const size_t channels,
	const size_t width,
	const size_t height
) {
  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= width * height * channels * sizeof(float)) {
        float *buffer = (float *)view.buf;
        scene->DefineImageMap(imgMapName, buffer, gamma, channels,
            width, height, Scene::DEFAULT, Scene::REPEAT);

        PyBuffer_Release(&view);
      } else {
        const std::string errorMsg = "Not enough space in the buffer of Scene.DefineImageMap() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(width * height * channels * sizeof(float));
        PyBuffer_Release(&view);

        throw std::runtime_error(errorMsg);
      }
    } else {
      const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Scene.DefineImageMap() method: " + objType);
    }
  }  else {
    const std::string objType = py::cast<std::string>((obj.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unsupported data type Scene.DefineImageMap() method: " + objType);
  }
}

// Helpers for mesh property translation
template<typename T, size_t N>
static luxrays::ExtMeshProp<T>::Layer translateLayer(
	const py::object& obj,
	const std::string objname
) {
	// Require non-empty input
	if (obj.is_none()) return nullptr;

	// Check input type
	if (not py::isinstance<py::list>(obj)) {
		const std::string objType =
			py::cast<std::string>((obj.attr("__class__")).attr("__name__"));

		throw std::runtime_error(
			"Wrong data type for the list of "
			+ objname
			+ " of method Scene.DefineMesh(): "
			+ objType
		);
	}

	const py::list &l = py::cast<py::list>(obj);
	const py::ssize_t size = len(l);

	// Process
	auto out = std::make_shared<T[]>(size);
	for (py::ssize_t i = 0; i < size; ++i) {

		// Check list member consistency
		if (not py::isinstance<py::tuple>(l[i])) {
		  const std::string objType =
			py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
		  throw std::runtime_error(
			"Wrong data type in the list of "
			+ objname
			+ "UVs of method Scene.DefineMesh() at position "
			+ luxrays::ToString(i) + ": " + objType
		  );
		}

		// Process element
		const py::tuple &t = py::cast<py::tuple>(l[i]);
		constexpr auto cast = []<size_t... Is>(
		  const py::tuple& t, size_t i, std::index_sequence<Is...>
		) {
		  return T{py::cast<float>(t[Is])...};
		};
		out[i] = cast(t, i, std::make_index_sequence<N>{});
	}

	return out;

}  // translateLayer


template<typename T, size_t N>
static std::optional<luxrays::ExtMeshProp<T>> translateProp(
	const py::object& obj,
	const std::string objname
) {
	// Require non-empty input
	if (obj.is_none()) return std::nullopt;


    if (not py::isinstance<py::list>(obj)) {
		const std::string objType =
			py::cast<std::string>((obj.attr("__class__")).attr("__name__"));

		throw std::runtime_error(
			"Wrong data type for the list of "
			+ objname
			+ " of method Scene.DefineMesh(): "
			+ objType
		);
	}

	// Cast input
	const py::list& layers = py::cast<py::list>(obj);
	const py::ssize_t size = std::min(len(layers), size_t(LC_MESH_MAX_DATA_COUNT));;

	// Declare output
	luxrays::ExtMeshProp<T> out;

	// Process layers
    for (py::ssize_t j=0; j < size; ++j) {
		auto layer = layers[j];

		if (layer.is_none()) continue;

		// Cast layer to list
        if (not py::isinstance<py::list>(layer)) {
			const std::string objType =
				py::cast<std::string>((layer.attr("__class__")).attr("__name__"));
			throw std::runtime_error(
				"Wrong data type for the list of " + objname
				+ " in the layer " + std::to_string(j)
				+ " of method Scene.DefineMeshExt(): " + objType
			);
		}
		const py::list &l = py::cast<py::list>(layer);

		// Process layer
		const py::ssize_t size = len(l);
		out[j] = std::make_shared<T[]>(size);
		for (py::ssize_t i = 0; i < size; ++i) {
			// Require element to be a tuple
			if(not py::isinstance<py::tuple>(l[i])) {
                const std::string objType =
					py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
                throw std::runtime_error(
					"Wrong data type in the list of " + objname + " colors of method "
					+ "Scene.DefineMeshExt() at position " + luxrays::ToString(i)
					+": " + objType
				);
			}

			// Process element
			const py::tuple &t = py::cast<py::tuple>(l[i]);
			constexpr auto cast = []<size_t... Is>(
				const py::tuple& t, size_t i, std::index_sequence<Is...>
			) {
				return T{py::cast<float>(t[Is])...};
			};
			out[j][i] = cast(t, i, std::make_index_sequence<N>{});
		}
	}

	return out;
}


static void Scene_DefineMesh1(
	const SceneImplPtr & scene,
	const std::string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas,
    const py::object &transformation) {
  // NOTE: I would like to use boost::scoped_array but
  // some guy has decided that boost::scoped_array must not have
  // a release() method for some ideological reason...

  // Translate all vertices
  long plyNbVerts;
  luxrays::VertexBuffer points;
  if (py::isinstance<py::list>(p)) {
    const py::list &l = py::cast<py::list>(p);
    const py::ssize_t size = len(l);
    plyNbVerts = size;

    points.Allocate(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if (py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        points[i] = luxrays::Point(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
      } else {
        const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
        throw std::runtime_error("Wrong data type in the list of vertices of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const std::string objType = py::cast<std::string>((p.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Wrong data type for the list of vertices of method Scene.DefineMesh(): " + objType);
  }

  // Translate all triangles
  // TODO shoud be optimized
  luxrays::TriangleBuffer tris;
  if (py::isinstance<py::list>(vi)) {
    const py::list &l = py::cast<py::list>(vi);
    const py::ssize_t size = len(l);

	tris.Allocate(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if (py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        tris[i] = luxrays::Triangle(
			py::cast<luxrays::Triangle::subtype_t>(t[0]),
			py::cast<luxrays::Triangle::subtype_t>(t[1]),
			py::cast<luxrays::Triangle::subtype_t>(t[2]));
      } else {
        const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
        throw std::runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const std::string objType = py::cast<std::string>((vi.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
  }

  // Translate all normals
  luxrays::NormalBuffer normals;
  if (!n.is_none()) {
    if(py::isinstance<py::list>(n)) {
      const py::list &l = py::cast<py::list>(n);
      const py::ssize_t size = len(l);

      normals.Allocate(size);
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          normals[i] = luxrays::Normal(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
        } else {
          const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
          throw std::runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const std::string objType = py::cast<std::string>((n.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
    }
  }



  // Translate all UVs, Colors, Alphas
  auto uvs = translateLayer<luxrays::UV, 2>(uv, "UVs");
  auto colors = translateLayer<luxrays::Spectrum, 3>(cols, "colors");
  auto as = translateLayer<float, 1>(alphas, "alphas");


  auto mesh = std::make_unique<luxrays::ExtTriangleMesh>(
  	std::move(points), std::move(tris), std::move(normals), uvs, colors, as
  );

  // Apply the transformation if required
  if (!transformation.is_none()) {
  float mat[16];
  GetMatrix4x4(transformation, mat);
  mesh->ApplyTransform(luxrays::Transform(luxrays::Matrix4x4(mat).Transpose()));
  }

  mesh->SetName(meshName);
  scene->DefineMesh(std::move(mesh));
}

static void Scene_DefineMesh2(
	const SceneImplPtr & scene,
	const std::string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas
) {
  Scene_DefineMesh1(scene, meshName, p, vi, n, uv, cols, alphas, py::none());
}

static void Scene_DefineMeshExt1(
	const SceneImplPtr & scene,
	const std::string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas,
    const py::object &transformation
) {

  // Translate all vertices
  long plyNbVerts;
  luxrays::VertexBuffer points;
  if(py::isinstance<py::list>(p)) {
    const py::list &l = py::cast<py::list>(p);
    const py::ssize_t size = len(l);
    plyNbVerts = size;

    points.Allocate(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if(py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        points[i] = luxrays::Point(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
      } else {
        const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
        throw std::runtime_error("Wrong data type in the list of vertices of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const std::string objType = py::cast<std::string>((p.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Wrong data type for the list of vertices of method Scene.DefineMeshExt(): " + objType);
  }

  // Translate all triangles
  long plyNbTris;
  luxrays::TriangleBuffer tris;
  if(py::isinstance<py::list>(vi)) {
    const py::list &l = py::cast<py::list>(vi);
    const py::ssize_t size = len(l);
    plyNbTris = size;

	tris.Allocate(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if(py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        tris[i] = luxrays::Triangle(py::cast<u_int>(t[0]), py::cast<u_int>(t[1]), py::cast<u_int>(t[2]));
      } else {
        const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
        throw std::runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const std::string objType = py::cast<std::string>((vi.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
  }

  // Translate all normals
  luxrays::NormalBuffer normals;
  if (!n.is_none()) {
    if(py::isinstance<py::list>(n)) {
      const py::list &l = py::cast<py::list>(n);
      const py::ssize_t size = len(l);

      normals.Allocate(size);
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          normals[i] = luxrays::Normal(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
        } else {
          const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
          throw std::runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const std::string objType = py::cast<std::string>((n.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
    }
  }

  // Translate UVs, colors, alphas
  auto uvs = translateProp<luxrays::UV, 2>(uv, "UVs");
  auto colors = translateProp<luxrays::Spectrum, 3>(cols, "colors");
  auto as = translateProp<float, 1>(alphas, "alphas");

	auto mesh = std::make_unique<luxrays::ExtTriangleMesh>(
		std::move(points), std::move(tris), std::move(normals), uvs, colors, as
	);

	// Apply the transformation if required
	if (!transformation.is_none()) {
		float mat[16];
		GetMatrix4x4(transformation, mat);
		mesh->ApplyTransform(luxrays::Transform(luxrays::Matrix4x4(mat).Transpose()));
	}

  mesh->SetName(meshName);
  scene->DefineMesh(std::move(mesh));
}

static void Scene_DefineMeshExt2(
	const SceneImplPtr & scene,
	const std::string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas) {
  Scene_DefineMeshExt1(scene, meshName, p, vi, n, uv, cols, alphas, py::none());
}


// TODO make template and move to anonymous namespace
luxrays::Normal* AllocNormalsBuffer(unsigned int n) {
	return (new luxrays::Normal[n]);
}
luxrays::UV* AllocUVBuffer(unsigned int n) {
	return (new luxrays::UV[n]);
}
luxrays::Spectrum* AllocRGBBuffer(unsigned int n) {
	return (new luxrays::Spectrum[n]);
}
float* AllocAlphaBuffer(unsigned int n) {
	return (new float[n]);
}
template<typename T>
T* AllocBuffer(unsigned n) {
	return (new T[n]);
}

// Helper for DefineMeshExt3
// Allocate internal data structure and copy array inside
template<
	typename S,  // Source type
	typename D,  // Destination type
	D* (*Allocator)(unsigned int),
	size_t stride=3
>
std::tuple<std::unique_ptr<D[]>, u_int> dataCopy(
	const py::array_t<S, py::array::c_style> &src,
	const std::string& meshName,
	const std::string& propertyName
) {
	auto src_stride = src.shape(1);

	// Remark:
	// All buffers (triangles, points, normals...) are enclosed in smart pointers
	// in order to guarantee that memory is released in case of exception arising
	// during whole process: please keep it so.
	using this_array_t = py::array_t< S, py::array::c_style | py::array::forcecast >;
	auto direct_src = src.template unchecked<2>();

	// Check
	if (src_stride != stride) {
		std::string errorMsg = std::string("Scene.DefineMeshExt: Error - ")
			+ "Mesh '" + meshName + "' / "
			+ "Property '" + std::string(propertyName) + "' - "
			+ "Shape must be [N,"
			+ std::to_string(stride)
			+ "]";
		throw std::runtime_error(errorMsg);
	}

	// Allocate & copy
	u_int count = src.shape(0);
	if (!count) return std::tuple(nullptr, 0);
    auto dest = std::unique_ptr<D[]>(Allocator(count));
	std::memcpy(dest.get(), direct_src.data(0, 0), direct_src.nbytes());

	return std::tuple(std::move(dest), count);
}

// Variant of dataCopy for vertex buffer
// S: Source underlying type; OUT: Output type type
template< typename S,  size_t stride, typename O > 
O dataCopyBuffer(
	const py::array_t<S, py::array::c_style> &src,
	const std::string& meshName,
	const std::string& propertyName
) {
	auto src_stride = src.shape(1);

	using this_array_t = py::array_t< S, py::array::c_style | py::array::forcecast >;
	auto direct_src = src.template unchecked<2>();

	// Check
	if (src_stride != stride) {
		std::string errorMsg = std::string("Scene.DefineMeshExt: Error - ")
			+ "Mesh '" + meshName + "' / "
			+ "Property '" + std::string(propertyName) + "' - "
			+ "Shape must be [N,"
			+ std::to_string(stride)
			+ "]";
		throw std::runtime_error(errorMsg);
	}

	// Allocate & copy
	if (!src.shape(0)) return O();

	auto count = direct_src.nbytes() / sizeof(S);
	assert(direct_src.nbytes() % sizeof(S) == 0);
	auto in_span = std::span<const S>(direct_src.data(0, 0), count);
	O buf(in_span);

	return buf;
}


// Variant of DataCopy for optional arguments
template<
	typename S,  // Source type
	typename D,  // Destination type
	D* (*Allocator)(unsigned int),
	size_t stride=3
>
std::tuple<std::unique_ptr<D[]>, u_int> dataCopyOptional(
	std::optional<py::array_t<S, py::array::c_style>> src,
	const std::string& meshName,
	const std::string& propertyName
) {
	if (src.has_value()) {
		return dataCopy<S, D, Allocator, stride>(src.value(), meshName, propertyName);
	} else {
		return std::tuple(nullptr, 0);
	}
}


using triangle_underlying_type = std::remove_all_extents<decltype(luxrays::Triangle::v)>::type;

using py_float_array= py::array_t<float, py::array::c_style>;

template<typename T, size_t N>
static std::optional<luxrays::ExtMeshProp<T>> propCopy(
	const std::optional<std::vector<py_float_array>>& layers,
	const std::string objname,
	const std::string meshName
) {
	if (not layers or layers->empty()) return std::nullopt;

	auto layer_count = layers->size();
	luxrays::ExtMeshProp<T> out;
	if (layer_count > out.GetMaxLayerNumber()) {
		throw std::runtime_error(
			"Too many layers of " + objname + " for method Scene.DefineMesh()"
		);
	}

	for (size_t i = 0; i < layer_count; ++i) {
		const py::array_t<float, py::array::c_style> &layer = layers.value()[i];

		constexpr auto allocator = [](size_t n) { return std::make_shared<T[]>(n); };
		if (N == 1 && layer.ndim() == 1) {
			// Alpha layers are passed as a 1-D (N,) array: copy directly,
			// without reshape() (which would need the GIL)
			auto u = layer.unchecked<1>();
			const size_t count = u.shape(0);
			auto data = allocator(count);
			if (count)
				std::memcpy(data.get(), u.data(0), count * sizeof(float));
			out.SetLayer(i, std::shared_ptr<T[]>(std::move(data)), count);
			continue;
		}
		auto [data, numData] = dataCopy<float, T, &AllocBuffer, N>(
			layer, meshName, objname
		);
		out.SetLayer(i, std::shared_ptr<T[]>(std::move(data)), numData);
	}
	return out;

}


// Define Mesh from Numpy arrays
static void Scene_DefineMeshExt3(
	  const SceneImplPtr & scene,
	  const std::string &meshName,
    const py::object &p,  // points ndarray, or the name of an existing mesh to share the points from
	  const py::array_t<triangle_underlying_type, py::array::c_style > tri,
    const std::optional<py_float_array> n,
    const std::optional<std::vector<py_float_array>> uv_layers,
    const std::optional<std::vector<py_float_array>> color_layers,
    const std::optional<std::vector<py_float_array>> alpha_layers,
    const std::optional<py_float_array> transformation,
    const std::optional<py::object> crease_pairs,
    const std::optional<py::object> crease_weights
) {
	// Points: either a new buffer, or shared from an existing mesh (given by
	// name). A Buffer copy shares the storage, so building the mesh with the
	// sharing ctor on *sharedPoints never duplicates the point data.
	// NOTE: the GIL is released for the buffer copies / mesh build / scene
	// insertion below, so callers must not invoke scene methods concurrently
	// from multiple threads.
	luxrays::VertexBuffer points;
	const luxrays::VertexBuffer *sharedPoints = &points;
	py_float_array pointsArr;
	if (py::isinstance<py::str>(p)) {
		const std::string srcMeshName = py::cast<std::string>(p);
		auto &srcMesh = scene->GetSlgScene().GetExtMeshes().GetExtMesh(srcMeshName);
		auto *srcTriMesh = dynamic_cast<luxrays::ExtTriangleMesh *>(&srcMesh);
		if (!srcTriMesh)
			throw std::runtime_error("Scene.DefineMeshExt: mesh '" + srcMeshName +
				"' used as points source is not a triangle mesh");
		sharedPoints = &srcTriMesh->GetVertexBuffer();
	} else {
		pointsArr = py::cast<py_float_array>(p);
	}

	// Optional imported creases: flat (v0, v1) vertex index pairs with one
	// sharpness weight per pair, in OpenSubdiv units (>= 10 = infinite)
	std::vector<u_int> upairs;
	std::vector<float> weights;
	if (crease_pairs.has_value() && !crease_pairs->is_none()) {
		std::vector<int> pairs;
		GetArray<int>(*crease_pairs, pairs, 2);
		if (crease_weights.has_value() && !crease_weights->is_none())
			GetArray<float>(*crease_weights, weights);
		if (weights.size() * 2 != pairs.size()) {
			if (weights.empty() && !pairs.empty())
				weights.assign(pairs.size() / 2, 10.f);  // default: infinite crease
			else
				throw std::runtime_error(
					"Scene.DefineMeshExt: crease_pairs and crease_weights size mismatch");
		}
		upairs.assign(pairs.begin(), pairs.end());
	}

	std::optional<luxrays::Transform> luxtrans;
	if (transformation.has_value())
		luxtrans = luxrays::Transform(transformation.value().data());

	luxrays::TriangleBuffer triangles;
	luxrays::NormalBuffer normals;
	auto meshUVs = propCopy<luxrays::UV, 2>(uv_layers, "UVs", meshName);
	auto meshCols = propCopy<luxrays::Spectrum, 3>(color_layers, "colors", meshName);
	auto meshAlphas = propCopy<float, 1>(alpha_layers, "alphas", meshName);
	{
		Py_BEGIN_ALLOW_THREADS
		if (sharedPoints == &points)
			points = dataCopyBuffer< float, 3, luxrays::VertexBuffer > (
				pointsArr, meshName, "Points"
			);

		// Triangles
		triangles = dataCopyBuffer< luxrays::Triangle::subtype_t, 3, luxrays::TriangleBuffer> (
			tri, meshName, "Triangles"
		);

		// Normals
		if (n)
			normals = dataCopyBuffer< float, 3, luxrays::NormalBuffer > (n.value(), meshName, "Normals");

		assert(not bool(meshUVs) or meshUVs->GetLayerSize() == sharedPoints->Count());
		assert(not bool(meshCols) or meshCols->GetLayerSize() == sharedPoints->Count());
		assert(not bool(meshAlphas) or meshAlphas->GetLayerSize() == sharedPoints->Count());

		// Create Mesh (sharing ctor: the vertex buffer storage is shared, not copied)
		auto newMesh = std::make_unique<luxrays::ExtTriangleMesh>(
			*sharedPoints,
			std::move(triangles),
			std::move(normals),
			meshUVs,
			meshCols,
			meshAlphas
		);
		newMesh->SetName(meshName);
		if (!upairs.empty())
			newMesh->SetCreases(std::move(upairs), std::move(weights));
		if (luxtrans.has_value())
			newMesh->ApplyTransform(luxtrans.value());

		// Insert mesh into the scene
		scene->DefineMesh(std::move(newMesh));
		Py_END_ALLOW_THREADS
	}
}

// Number of imported creases (edge sharpness values) of a mesh
static size_t Scene_GetMeshCreaseCount(
	const SceneImplPtr & scene, const std::string &meshName
) {
	auto &mesh = scene->GetSlgScene().GetExtMeshes().GetExtMesh(meshName);
	auto *triMesh = dynamic_cast<luxrays::ExtTriangleMesh *>(&mesh);
	if (!triMesh)
		throw std::runtime_error("Scene.GetMeshCreaseCount: mesh '" + meshName +
			"' is not a triangle mesh");
	return triMesh->GetCreaseWeights().size();
}

static void Scene_SetMeshVertexAOV(
	const SceneImplPtr & scene, const std::string &meshName,
    const size_t index, const py::object &data
) {
	std::vector<float> v;
	GetArray<float>(data, v);

	Py_BEGIN_ALLOW_THREADS
	scene->SetMeshVertexAOV(meshName, index, v);
	Py_END_ALLOW_THREADS
}

static void Scene_SetMeshTriangleAOV(
    const SceneImplPtr & scene,
    const std::string &meshName,
    const size_t index,
    const py::object &data
) {
  std::vector<float> t;
  GetArray<float>(data, t);

  Py_BEGIN_ALLOW_THREADS
  scene->SetMeshTriangleAOV(meshName, index, t);
  Py_END_ALLOW_THREADS
}

static void Scene_SetMeshAppliedTransformation(
    const SceneImplPtr & scene,
    const std::string &meshName,
    const py::object &transformation) {
  float mat[16];
  GetMatrix4x4(transformation, mat);
  scene->SetMeshAppliedTransformation(meshName, mat);
}

static void Scene_DefineStrands(
    const SceneImplPtr & scene,
    const std::string &shapeName,
    const py::int_ strandsCount,
    const py::int_ pointsCount,
    const py::object &points,
    const py::object &segments,
    const py::object &thickness,
    const py::object &transparency,
    const py::object &colors,
    const py::object &uvs,
    const std::string &tessellationTypeStr,
    const py::int_ adaptiveMaxDepth,
    const float adaptiveError,
    const py::int_ solidSideCount,
    const bool solidCapBottom,
    const bool solidCapTop,
    const bool useCameraPosition) {
  // Initialize the cyHairFile object
  luxrays::cyHairFile strands;
  strands.SetHairCount(strandsCount);
  strands.SetPointCount(pointsCount);

  // Set defaults if available
  int flags = CY_HAIR_FILE_POINTS_BIT;

  if (py::isinstance<py::int_>(segments))
    strands.SetDefaultSegmentCount(py::cast<int>(segments));
  else
    flags |= CY_HAIR_FILE_SEGMENTS_BIT;

  if (py::isinstance<py::float_>(thickness))
    strands.SetDefaultThickness(py::cast<float>(thickness));
  else
    flags |= CY_HAIR_FILE_THICKNESS_BIT;

  if (py::isinstance<py::float_>(transparency))
    strands.SetDefaultTransparency(py::cast<float>(transparency));
  else
    flags |= CY_HAIR_FILE_TRANSPARENCY_BIT;

  if (py::isinstance<py::tuple>(colors)) {
    const py::tuple &t = py::cast<py::tuple>(colors);

    strands.SetDefaultColor(
      py::cast<float>(t[0]),
      py::cast<float>(t[1]),
      py::cast<float>(t[2]));
  } else
    flags |= CY_HAIR_FILE_COLORS_BIT;

  if (!uvs.is_none())
    flags |= CY_HAIR_FILE_UVS_BIT;

  strands.SetArrays(flags);

  // Translate all segments
  if (!py::isinstance<py::int_>(segments)) {
    if(py::isinstance<py::list>(segments)) {
      const py::list &l = py::cast<py::list>(segments);
      const py::ssize_t size = len(l);

      u_short *s = strands.GetSegmentsArray();
      for (py::ssize_t i = 0; i < size; ++i)
        s[i] = py::cast<u_short>(l[i]);
    } else {
      const std::string objType = py::cast<std::string>((segments.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of segments of method Scene.DefineStrands(): " + objType);
    }
  }


  // Translate all points
  if (!points.is_none()) {
    if(py::isinstance<py::list>(points)) {
      const py::list &l = py::cast<py::list>(points);
      const py::ssize_t size = len(l);

      float *p = strands.GetPointsArray();
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          p[i * 3] = py::cast<float>(t[0]);
          p[i * 3 + 1] = py::cast<float>(t[1]);
          p[i * 3 + 2] = py::cast<float>(t[2]);
        } else {
          const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
          throw std::runtime_error("Wrong data type in the list of points of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const std::string objType = py::cast<std::string>((points.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of points of method Scene.DefineStrands(): " + objType);
    }
  } else
    throw std::runtime_error("Points list can not be None in method Scene.DefineStrands()");

  // Translate all thickness
  if (!py::isinstance<py::float_>(thickness)) {
    if(py::isinstance<py::list>(thickness)) {
      const py::list &l = py::cast<py::list>(thickness);
      const py::ssize_t size = len(l);

      float *t = strands.GetThicknessArray();
      for (py::ssize_t i = 0; i < size; ++i)
        t[i] = py::cast<float>(l[i]);
    } else {
      const std::string objType = py::cast<std::string>((thickness.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of thickness of method Scene.DefineStrands(): " + objType);
    }
  }

  // Translate all transparency
  if (!py::isinstance<py::float_>(transparency)) {
    if(py::isinstance<py::list>(transparency)) {
      const py::list &l = py::cast<py::list>(transparency);
      const py::ssize_t size = len(l);

      float *t = strands.GetTransparencyArray();
      for (py::ssize_t i = 0; i < size; ++i)
        t[i] = py::cast<float>(l[i]);
    } else {
      const std::string objType = py::cast<std::string>((transparency.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of transparency of method Scene.DefineStrands(): " + objType);
    }
  }

  // Translate all colors
  if (!py::isinstance<py::tuple>(colors)) {
    if(py::isinstance<py::list>(colors)) {
      const py::list &l = py::cast<py::list>(colors);
      const py::ssize_t size = len(l);

      float *c = strands.GetColorsArray();
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          c[i * 3] = py::cast<float>(t[0]);
          c[i * 3 + 1] = py::cast<float>(t[1]);
          c[i * 3 + 2] = py::cast<float>(t[2]);
        } else {
          const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
          throw std::runtime_error("Wrong data type in the list of colors of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const std::string objType = py::cast<std::string>((colors.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of colors of method Scene.DefineStrands(): " + objType);
    }
  }

  // Translate all UVs
  if (!uvs.is_none()) {
    if(py::isinstance<py::list>(uvs)) {
      const py::list &l = py::cast<py::list>(uvs);
      const py::ssize_t size = len(l);

      float *uv = strands.GetUVsArray();
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          uv[i * 2] = py::cast<float>(t[0]);
          uv[i * 2 + 1] = py::cast<float>(t[1]);
        } else {
          const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));
          throw std::runtime_error("Wrong data type in the list of UVs of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const std::string objType = py::cast<std::string>((uvs.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Wrong data type for the list of UVs of method Scene.DefineStrands(): " + objType);
    }
  }

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
    throw std::runtime_error("Tessellation type unknown in method Scene.DefineStrands(): " + tessellationTypeStr);

  scene->DefineStrands(shapeName, strands,
      tessellationType, adaptiveMaxDepth, adaptiveError,
      solidSideCount, solidCapBottom, solidCapTop,
      useCameraPosition);
}

static void Scene_DuplicateObject(const SceneImplPtr & scene,
    const std::string &srcObjName,
    const std::string &dstObjName,
    const py::object &transformation,
    const size_t objectID) {
  float mat[16];
  GetMatrix4x4(transformation, mat);

  scene->DuplicateObject(srcObjName, dstObjName, mat, objectID);
}

static void Scene_DuplicateObjectMulti(const SceneImplPtr & scene,
    const std::string &srcObjName,
    const std::string &dstObjNamePrefix,
    const unsigned int count,
    const py::object &transformations,
    const py::object &objectIDs) {
  if (!PyObject_CheckBuffer(objectIDs.ptr())){
    const std::string objType = py::cast<std::string>((objectIDs.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
  }
  if (!PyObject_CheckBuffer(transformations.ptr())) {
    const std::string objType = py::cast<std::string>((transformations.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
  }

  Py_buffer transformationsView;
  if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
    const std::string objType = py::cast<std::string>((transformations.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
  }
  Py_buffer objectIDsView;
  if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
    PyBuffer_Release(&transformationsView);

    const std::string objType = py::cast<std::string>((transformations.attr("__class__")).attr("__name__"));
    throw std::runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
  }

  const size_t objectIDsBufferSize = sizeof(u_int) * count;
  if ((size_t)objectIDsView.len < objectIDsBufferSize) {
    const std::string errorMsg = "Not enough objectIDs in the buffer of Scene.DuplicateObject() method: " +
        luxrays::ToString(objectIDsView.len) + " instead of " + luxrays::ToString(objectIDsBufferSize);

    PyBuffer_Release(&objectIDsView);
    PyBuffer_Release(&transformationsView);

    throw std::runtime_error(errorMsg);
  }
  const size_t transformationsBufferSize = sizeof(float) * 16 * count;
  if ((size_t)transformationsView.len < transformationsBufferSize) {
    const std::string errorMsg = "Not enough matrices in the buffer of Scene.DuplicateObject() method: " +
        luxrays::ToString(transformationsView.len) + " instead of " + luxrays::ToString(transformationsBufferSize);

    PyBuffer_Release(&objectIDsView);
    PyBuffer_Release(&transformationsView);

    throw std::runtime_error(errorMsg);
  }

  float *transformationsBuffer = (float *)transformationsView.buf;
  u_int *objectIDsBuffer = (u_int *)objectIDsView.buf;

  scene->DuplicateObject(srcObjName, dstObjNamePrefix, count,
      transformationsBuffer, objectIDsBuffer);

  PyBuffer_Release(&transformationsView);
  PyBuffer_Release(&objectIDsView);
}

static void Scene_DuplicateMotionObject(const SceneImplPtr & scene,
    const std::string &srcObjName,
    const std::string &dstObjName,
    const size_t steps,
    const py::object &times,
    const py::object &transformations,
    const size_t objectID) {
  if (!times.is_none() && !transformations.is_none()) {
    if(py::isinstance<py::list>(times) && py::isinstance<py::list>(transformations)) {
      const py::list &timesList = py::cast<py::list>(times);
      const py::list &transformationsList = py::cast<py::list>(transformations);

      if ((len(timesList) != steps) || (len(transformationsList) != steps))
        throw std::runtime_error("Wrong number of elements for the times and/or the list of transformations of method Scene.DuplicateObject()");

      std::vector<float> timesVec(steps);
      std::vector<float> transVec(steps * 16);
      u_int transIndex = 0;

      for (u_int i = 0; i < steps; ++i) {
        timesVec[i] = py::cast<float>(timesList[i]);

        float mat[16];
        GetMatrix4x4(transformationsList[i], mat);
        for (u_int i = 0; i < 16; ++i)
          transVec[transIndex++] = mat[i];
      }

      scene->DuplicateObject(srcObjName, dstObjName, steps, &timesVec[0], &transVec[0], objectID);
    } else
      throw std::runtime_error("Wrong data type for the list of transformation values of method Scene.DuplicateObject()");
  } else
    throw std::runtime_error("None times and/or transformations in Scene.DuplicateObject(): " + srcObjName);
}

static void Scene_DuplicateMotionObjectMulti(const SceneImplPtr & scene,
    const std::string &srcObjName,
    const std::string &dstObjName,
    const unsigned int count,
    const size_t steps,
    const py::object &times,
    const py::object &transformations,
    const py::object &objectIDs) {

    if (!PyObject_CheckBuffer(times.ptr())){
      const std::string objType = py::cast<std::string>((times.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }
    if (!PyObject_CheckBuffer(transformations.ptr())){
      const std::string objType = py::cast<std::string>((transformations.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }
    if (!PyObject_CheckBuffer(objectIDs.ptr())){
      const std::string objType = py::cast<std::string>((objectIDs.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }

    Py_buffer timesView;
    if (PyObject_GetBuffer(times.ptr(), &timesView, PyBUF_SIMPLE)) {
      const std::string objType = py::cast<std::string>((times.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }
    Py_buffer transformationsView;
    if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
      PyBuffer_Release(&timesView);

      const std::string objType = py::cast<std::string>((transformations.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }
    Py_buffer objectIDsView;
    if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);

      const std::string objType = py::cast<std::string>((objectIDs.attr("__class__")).attr("__name__"));
      throw std::runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }

    const size_t timesBufferSize = sizeof(float) * count;
    if ((size_t)timesView.len < timesBufferSize) {
      const std::string errorMsg = "Not enough times in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(timesView.len) + " instead of " + luxrays::ToString(timesBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw std::runtime_error(errorMsg);
    }
    const size_t transformationsBufferSize = sizeof(float) * 16 * count;
    if ((size_t)transformationsView.len < transformationsBufferSize) {
      const std::string errorMsg = "Not enough matrices in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(transformationsView.len) + " instead of " + luxrays::ToString(transformationsBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw std::runtime_error(errorMsg);
    }
    const size_t objectIDsBufferSize = sizeof(u_int) * count;
    if ((size_t)objectIDsView.len < objectIDsBufferSize) {
      const std::string errorMsg = "Not enough object IDs in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(objectIDsView.len) + " instead of " + luxrays::ToString(objectIDsBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw std::runtime_error(errorMsg);
    }

    float *timesBuffer = (float *)timesView.buf;
    float *transformationsBuffer = (float *)transformationsView.buf;
    u_int *objectIDsBuffer = (u_int *)objectIDsView.buf;
    scene->DuplicateObject(srcObjName, dstObjName, count, steps, timesBuffer,
        transformationsBuffer, objectIDsBuffer);

    PyBuffer_Release(&timesView);
    PyBuffer_Release(&transformationsView);
    PyBuffer_Release(&objectIDsView);
}

static void Scene_DeleteObjects(const SceneImplPtr & scene,
    const py::list &l) {
  const py::ssize_t size = len(l);
  std::vector<std::string> names;
  names.reserve(size);
  for (py::ssize_t i = 0; i < size; ++i) {
    const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "str")
      names.push_back(py::cast<std::string>(l[i]));
    else
      throw std::runtime_error("Unsupported data type included in Scene.DeleteObjects() list: " + objType);
  }

  scene->DeleteObjects(names);
}

static void Scene_DeleteLights(const SceneImplPtr & scene,
    const py::list &l) {
  const py::ssize_t size = len(l);
  std::vector<std::string> names;
  names.reserve(size);
  for (py::ssize_t i = 0; i < size; ++i) {
    const std::string objType = py::cast<std::string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "str")
      names.push_back(py::cast<std::string>(l[i]));
    else
      throw std::runtime_error("Unsupported data type included in Scene.DeleteLights() list: " + objType);
  }

  scene->DeleteLights(names);
}

static void Scene_UpdateObjectTransformation(const SceneImplPtr & scene,
    const std::string &objName,
    const py::object &transformation
) {
	float mat[16];
	GetMatrix4x4(transformation, mat);
	scene->UpdateObjectTransformation(objName, mat);
}

//------------------------------------------------------------------------------
// Glue for RenderConfig class
//------------------------------------------------------------------------------

static py::tuple RenderConfig_LoadResumeFile(const py::str &fileNameStr) {
  const std::string fileName = py::cast<std::string>(fileNameStr);
  RenderStateImplRPtr startState;
  FilmImplUPtr startFilm;
  auto config = RenderConfigImpl::Create<
	  const std::string&, RenderStateImplRPtr&, FilmImplUPtr&
  >(fileName, startState, startFilm);

  return py::make_tuple(config, startState, startFilm);
}

static std::unique_ptr<luxcore::detail::RenderConfigImpl>
RenderConfig_LoadFile(const py::str &fileNameStr) {
  const std::string fileName = py::cast<std::string>(fileNameStr);
  auto config = RenderConfigImpl::Create(fileName);

  return config;
}

static const luxcore::detail::SceneImpl&
RenderConfig_GetScene(luxcore::detail::RenderConfigImpl *renderConfig) {
	auto& luxscene = renderConfig->GetScene();
  return static_cast<const luxcore::detail::SceneImpl&>(luxscene);
}

static py::tuple RenderConfig_GetFilmSize(luxcore::detail::RenderConfigImpl *renderConfig) {
  u_int filmWidth, filmHeight, filmSubRegion[4];
  const bool result = renderConfig->GetFilmSize(&filmWidth, &filmHeight, filmSubRegion);

  return py::make_tuple(filmWidth, filmHeight,
      py::make_tuple(filmSubRegion[0], filmSubRegion[1], filmSubRegion[2], filmSubRegion[3]),
      result);
}

//------------------------------------------------------------------------------
// Glue for RenderSession class
//------------------------------------------------------------------------------


static std::shared_ptr<luxcore::detail::RenderStateImpl>
RenderSession_GetRenderState(
	const std::unique_ptr<luxcore::detail::RenderSessionImpl> & renderSession
) {
  return static_pointer_cast<luxcore::detail::RenderStateImpl>(renderSession->GetRenderState());
}

//------------------------------------------------------------------------------
// Film output conversion functions
//------------------------------------------------------------------------------

static float FindMaxValue(const float *buffer, const size_t buffersize) {
	float maxValue = 0;
	for (u_int i = 0; i < buffersize; ++i) {
		const float value = buffer[i];
		if (!isinf(value) && !isnan(value) && (value > maxValue)) {
			maxValue = value;
		}
	}
	return maxValue;
}

// Note: This function is used by pyluxcoredemo.py, do not remove.
// Note2: This function could be usefully implemented in Python/numpy (TODO).
void ConvertFilmChannelOutput_3xFloat_To_4xUChar(const size_t width, const size_t height,
		py::object &objSrc, py::object &objDst, const bool normalize) {
	if (!PyObject_CheckBuffer(objSrc.ptr())) {
		const std::string objType = py::cast<std::string>((objSrc.attr("__class__")).attr("__name__"));
		throw std::runtime_error("Unsupported data type in source object of ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}
	if (!PyObject_CheckBuffer(objDst.ptr())) {
		const std::string objType = py::cast<std::string>((objDst.attr("__class__")).attr("__name__"));
		throw std::runtime_error("Unsupported data type in destination object of ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}

	Py_buffer srcView;
	if (PyObject_GetBuffer(objSrc.ptr(), &srcView, PyBUF_SIMPLE)) {
		const std::string objType = py::cast<std::string>((objSrc.attr("__class__")).attr("__name__"));
		throw std::runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}
	Py_buffer dstView;
	if (PyObject_GetBuffer(objDst.ptr(), &dstView, PyBUF_SIMPLE)) {
		PyBuffer_Release(&srcView);

		const std::string objType = py::cast<std::string>((objSrc.attr("__class__")).attr("__name__"));
		throw std::runtime_error("Unable to get a source data view in ConvertFilmChannelOutput_3xFloat_To_4xUChar(): " + objType);
	}

	if (srcView.len / (3 * 4) != dstView.len / 4) {
		PyBuffer_Release(&srcView);
		PyBuffer_Release(&dstView);
		throw std::runtime_error("Wrong buffer size in ConvertFilmChannelOutput_3xFloat_To_4xUChar()");
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

PYBIND11_MODULE(pyluxcore, m) {
  // I get a crash on Ubuntu 19.10 without this line and this should be
  // good anyway to avoid problems with "," Vs. "." decimal separator, etc.

  try {
    std::locale::global(std::locale("C.UTF-8"));
  } catch (std::runtime_error &) {
    // "C.UTF-8" std::locale may not exist on some system so I ignore the error
  }

  // This 'module' is actually a fake package
  m.attr("__path__") = "pyluxcore";
  m.attr("__package__") = "pyluxcore";
  m.attr("__doc__") = "LuxCoreRender Python bindings\n\n"
      "Provides access to the LuxCoreRender API in python\n\n";

  m.def("Version", LuxCoreVersion, "Returns the LuxCore version");

  // Generic union-find (parallel C++ implementation): given num_elements
  // elements [0, num_elements) and pairs of equivalent elements, returns an
  // int32 array with the class (component) label of each element
  m.def("union_find", &union_find_labels,
	  "Union-find over integer elements: returns the component label of each "
	  "element [0, num_elements), given pairs of equivalent elements",
	  py::arg("num_elements"), py::arg("pairs"));

  // Parallel argsort (tbb::parallel_sort) of a 1-D array: returns the int64
  // indices that would sort it. Releases the GIL during the sort (numpy's
  // argsort holds it).
  m.def("parallel_argsort", &parallel_argsort,
	  "Parallel argsort of a 1-D array (4/8-byte int or float): returns the "
	  "int64 indices that would sort the array. With an optional 1-D "
	  "'secondary' array of the same length, sorts lexicographically by "
	  "(keys, secondary)",
	  py::arg("keys"), py::arg("secondary") = py::none());

  m.def("Init", &LuxCore_Init);
  m.def("Init", &LuxCore_InitDefaultHandler);
  m.def("SetLogHandler", &LuxCore_SetLogHandler);
  m.def("ParseLXS", &ParseLXS);
  m.def("MakeTx", &MakeTx);

  m.def("GetPlatformDesc", &GetPlatformDesc);
  m.def("GetOpenCLDeviceDescs", &GetOpenCLDeviceDescs);

  // Deprecated, use GetOpenCLDeviceDescs instead
  m.def("GetOpenCLDeviceList", &GetOpenCLDeviceList);

  m.def("ClearFileNameResolverPaths", &ClearFileNameResolverPaths);
  m.def("AddFileNameResolverPath", &AddFileNameResolverPath);
  m.def("GetFileNameResolverPaths", &GetFileNameResolverPaths);

  // Kernel cache management
  m.def("ClearCUDAKernelCache", &ClearCUDAKernelCache, "Clear all CUDA kernel caches");
  m.def("ClearOCLKernelCache", &ClearOCLKernelCache, "Clear all OpenCL kernel caches");
  m.def("ClearAllKernelCaches", &ClearAllKernelCaches, "Clear all kernel caches (CUDA and OpenCL)");

  m.def("KernelCacheFill", &LuxCore_KernelCacheFill1);
  m.def("KernelCacheFill", &LuxCore_KernelCacheFill2);

  //--------------------------------------------------------------------------
  // Property class
  //--------------------------------------------------------------------------

  py::class_<luxrays::Property, py::smart_holder>(m, "Property")
    .def(
		py::init(
			[](std::string name) -> std::unique_ptr<luxrays::Property>
			{ return std::make_unique<luxrays::Property>(name); }
		)
	)
    .def(
		py::init(
			[](std::string name, bool val) -> std::unique_ptr<luxrays::Property>
			{ return std::make_unique<luxrays::Property>(name, val); }
		)
	)
    .def(
		py::init(
			[](std::string name, long long val) -> std::unique_ptr<luxrays::Property>
			{ return std::make_unique<luxrays::Property>(name, val); }
		)
	)
    .def(
		py::init(
			[](std::string name, double val) -> std::unique_ptr<luxrays::Property>
			{ return std::make_unique<luxrays::Property>(name, val); }
		)
	)
    .def(
		py::init(
			[](std::string name, std::string val) -> std::unique_ptr<luxrays::Property>
			{ return std::make_unique<luxrays::Property>(name, val); }
		)
	)

    //.def(py::init<std::string, bool>())
    //.def(py::init<std::string, long long>())
    //.def(py::init<std::string, double>())
    //.def(py::init<std::string, std::string>())
    .def(py::init(&Property_InitWithList))

    .def("GetName", &luxrays::Property::GetName, py::return_value_policy::copy)
    .def("GetSize", &luxrays::Property::GetSize)
    .def("Clear", &luxrays::Property::Clear)

    .def("Get", &Property_Get)

    .def<bool (luxrays::Property::*)(const u_int) const>
      ("GetBool", &luxrays::Property::Get)
    .def<long long (luxrays::Property::*)(const u_int) const>
      ("GetInt", &luxrays::Property::Get)
    .def<double (luxrays::Property::*)(const u_int) const>
      ("GetFloat", &luxrays::Property::Get)
    .def<std::string (luxrays::Property::*)(const u_int) const>
      ("GetString", &luxrays::Property::Get)
    .def("GetBlob", &Property_GetBlobByIndex)

    .def("GetBool", &Property_GetBool)
    .def("GetInt", &Property_GetInt)
    .def("GetUnsignedLongLong", &Property_GetUnsignedLongLong)
    .def("GetFloat", &Property_GetFloat)
    .def("GetString", &Property_GetString)
    .def("GetBlob", &Property_GetBlob)

    .def("GetBools", &Property_GetBools)
    .def("GetInts", &Property_GetInts)
    .def("GetFloats", &Property_GetFloats)
    .def("GetStrings", &Property_GetStrings)
    .def("GetBlobs", &Property_GetBlobs)

    .def("GetValuesString", &luxrays::Property::GetValuesString)
    .def("ToString", &luxrays::Property::ToString)

    .def("Add", &Property_Add)
    .def("AddAllBool", &Property_AddAllBool)
    .def("AddAllInt", &Property_AddAllInt)
    .def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLong)
    .def("AddAllFloat", &Property_AddAllFloat)
    .def("AddAllBool", &Property_AddAllBoolStride)
    .def("AddAllInt", &Property_AddAllIntStride)
    .def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLongStride)
    .def("AddAllFloat", &Property_AddAllFloatStride)
    .def<PropertyRPtr (*)(PropertyRPtr , const py::list &)>
      ("Set", &Property_Set)
    .def<PropertyRPtr (*)(PropertyRPtr , const size_t, const py::object &)>
      ("Set", &Property_Set)

    .def("__str__", &luxrays::Property::ToString)
  ;

  //--------------------------------------------------------------------------
  // Properties class
  //--------------------------------------------------------------------------

  py::class_<luxrays::Properties, py::smart_holder>(m, "Properties")
    .def(
		py::init(
			[]() -> std::unique_ptr<luxrays::Properties>
			{ return std::make_unique<luxrays::Properties>(); }
		)
	)
    .def(
		py::init(
			[](std::string s) -> std::unique_ptr<luxrays::Properties>
			{ return std::make_unique<luxrays::Properties>(s); }
		)
	)
    .def(
		py::init(
			[](const Properties& props) -> std::unique_ptr<luxrays::Properties>
			{ return props.Clone(); }
		)
	)

    // Required because Properties::Set is overloaded
	.def<luxrays::Properties &(luxrays::Properties::*)(PropertyRPtr)>
      ("Set", &luxrays::Properties::Set)

    .def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &)>
      ("Set", &luxrays::Properties::Set)

    .def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &, const std::string &)>
      ("Set", &luxrays::Properties::Set)

    .def("SetFromFile", &luxrays::Properties::SetFromFile)
    .def("SetFromString", &luxrays::Properties::SetFromString)

    .def("Clear", &luxrays::Properties::Clear)
    .def("GetAllNamesRE", &luxrays::Properties::GetAllNamesRE)
    .def(
		"GetAllNames",
		py::overload_cast<>( &luxrays::Properties::GetAllNames, py::const_)
	)
    .def(
		"GetAllNames",
		py::overload_cast<const std::string &>(
			&luxrays::Properties::GetAllNames, py::const_
		)
	)
    .def(
		"GetAllUniqueSubNames",
		&luxrays::Properties::GetAllUniqueSubNames,
		"Get all unique subnames given a prefix",
		py::arg("prefix"),
		py::arg("sorted")=false
	)
    .def("HaveNames", &luxrays::Properties::HaveNames)
    .def("HaveNamesRE", &luxrays::Properties::HaveNamesRE)
    .def("GetAllProperties", &luxrays::Properties::GetAllProperties)

    .def<const luxrays::Property (luxrays::Properties::*)(const std::string &) const>
      ("Get", &luxrays::Properties::Get)
    .def("Get", &Properties_GetWithDefaultValues)

    .def("GetSize", &luxrays::Properties::GetSize)

    .def("IsDefined", &luxrays::Properties::IsDefined)
    .def("Delete", &luxrays::Properties::Delete)
    .def("DeleteAll", &luxrays::Properties::DeleteAll)
    .def("ToString", &luxrays::Properties::ToString)

    .def("__str__", &luxrays::Properties::ToString)
  ;

  //--------------------------------------------------------------------------
  // Film class
  //--------------------------------------------------------------------------

  py::enum_<Film::FilmOutputType>(m, "FilmOutputType")
    .value("RGB", Film::OUTPUT_RGB)
    .value("RGBA", Film::OUTPUT_RGBA)
    // RGB_TONEMAPPED is deprecated
    .value("RGB_TONEMAPPED", Film::OUTPUT_RGB_IMAGEPIPELINE)
    .value("RGB_IMAGEPIPELINE", Film::OUTPUT_RGB_IMAGEPIPELINE)
    // RGBA_TONEMAPPED is deprecated
    .value("RGBA_TONEMAPPED", Film::OUTPUT_RGBA_IMAGEPIPELINE)
    .value("RGBA_IMAGEPIPELINE", Film::OUTPUT_RGBA_IMAGEPIPELINE)
    .value("ALPHA", Film::OUTPUT_ALPHA)
    .value("DEPTH", Film::OUTPUT_DEPTH)
    .value("POSITION", Film::OUTPUT_POSITION)
    .value("GEOMETRY_NORMAL", Film::OUTPUT_GEOMETRY_NORMAL)
    .value("SHADING_NORMAL", Film::OUTPUT_SHADING_NORMAL)
    .value("MATERIAL_ID", Film::OUTPUT_MATERIAL_ID)
    .value("DIRECT_DIFFUSE", Film::OUTPUT_DIRECT_DIFFUSE)
    .value("DIRECT_DIFFUSE_REFLECT", Film::OUTPUT_DIRECT_DIFFUSE_REFLECT)
    .value("DIRECT_DIFFUSE_TRANSMIT", Film::OUTPUT_DIRECT_DIFFUSE_TRANSMIT)
    .value("DIRECT_GLOSSY", Film::OUTPUT_DIRECT_GLOSSY)
    .value("DIRECT_GLOSSY_REFLECT", Film::OUTPUT_DIRECT_GLOSSY_REFLECT)
    .value("DIRECT_GLOSSY_TRANSMIT", Film::OUTPUT_DIRECT_GLOSSY_TRANSMIT)
    .value("EMISSION", Film::OUTPUT_EMISSION)
    .value("INDIRECT_DIFFUSE", Film::OUTPUT_INDIRECT_DIFFUSE)
    .value("INDIRECT_DIFFUSE_REFLECT", Film::OUTPUT_INDIRECT_DIFFUSE_REFLECT)
    .value("INDIRECT_DIFFUSE_TRANSMIT", Film::OUTPUT_INDIRECT_DIFFUSE_TRANSMIT)
    .value("INDIRECT_GLOSSY", Film::OUTPUT_INDIRECT_GLOSSY)
    .value("INDIRECT_GLOSSY_REFLECT", Film::OUTPUT_INDIRECT_GLOSSY_REFLECT)
    .value("INDIRECT_GLOSSY_TRANSMIT", Film::OUTPUT_INDIRECT_GLOSSY_TRANSMIT)
    .value("INDIRECT_SPECULAR", Film::OUTPUT_INDIRECT_SPECULAR)
    .value("INDIRECT_SPECULAR_REFLECT", Film::OUTPUT_INDIRECT_SPECULAR_REFLECT)
    .value("INDIRECT_SPECULAR_TRANSMIT", Film::OUTPUT_INDIRECT_SPECULAR_TRANSMIT)
    .value("MATERIAL_ID_MASK", Film::OUTPUT_MATERIAL_ID_MASK)
    .value("DIRECT_SHADOW_MASK", Film::OUTPUT_DIRECT_SHADOW_MASK)
    .value("INDIRECT_SHADOW_MASK", Film::OUTPUT_INDIRECT_SHADOW_MASK)
    .value("RADIANCE_GROUP", Film::OUTPUT_RADIANCE_GROUP)
    .value("UV", Film::OUTPUT_UV)
    .value("RAYCOUNT", Film::OUTPUT_RAYCOUNT)
    .value("BY_MATERIAL_ID", Film::OUTPUT_BY_MATERIAL_ID)
    .value("IRRADIANCE", Film::OUTPUT_IRRADIANCE)
    .value("OBJECT_ID", Film::OUTPUT_OBJECT_ID)
    .value("OBJECT_ID_MASK", Film::OUTPUT_OBJECT_ID_MASK)
    .value("BY_OBJECT_ID", Film::OUTPUT_BY_OBJECT_ID)
    .value("SAMPLECOUNT", Film::OUTPUT_SAMPLECOUNT)
    .value("CONVERGENCE", Film::OUTPUT_CONVERGENCE)
    .value("SERIALIZED_FILM", Film::OUTPUT_SERIALIZED_FILM)
    .value("MATERIAL_ID_COLOR", Film::OUTPUT_MATERIAL_ID_COLOR)
    .value("ALBEDO", Film::OUTPUT_ALBEDO)
    .value("AVG_SHADING_NORMAL", Film::OUTPUT_AVG_SHADING_NORMAL)
    .value("NOISE", Film::OUTPUT_NOISE)
    .value("USER_IMPORTANCE", Film::OUTPUT_USER_IMPORTANCE)
    .value("CAUSTIC", Film::OUTPUT_CAUSTIC)
    .def_property_readonly_static("names", [](py::object self){
        return self.attr("__members__");
    })
  ;

  //py::class_<luxcore::detail::FilmImpl, FilmImpl &>(m, "Film")
  py::class_<luxcore::detail::FilmImpl, py::smart_holder>(m, "Film")
    .def(
		py::init(
			[](std::string s) -> FilmImplUPtr {
				return luxcore::detail::FilmImpl::Create(s);
			}
		)
	)
    .def(
		py::init(
			[] (
				luxrays::PropertiesRPtr props,
				bool hasPixelNormalizedChannel,
				bool hasScreenNormalizedChannel
			) -> FilmImplUPtr {
				return luxcore::detail::FilmImpl::Create(
					props, hasPixelNormalizedChannel, hasScreenNormalizedChannel
				);
			}
		)
	)
    //.def(py::init<std::string>([](std::string s){ return luxcore::detail::FilmImpl::Create(s);})
    //.def(py::init<luxrays::Properties, bool, bool>())
    .def("GetWidth", &luxcore::detail::FilmImpl::GetWidth)
    .def("GetHeight", &luxcore::detail::FilmImpl::GetHeight)
    .def("GetStats", &luxcore::detail::FilmImpl::GetStats)
    .def("GetFilmY", &Film_GetFilmY1)
    .def("GetFilmY", &Film_GetFilmY2)
    .def("Save", &luxcore::detail::FilmImpl::SaveOutputs) // Deprecated
    .def("Clear", &luxcore::detail::FilmImpl::Clear)
    .def("AddFilm", &Film_AddFilm1)
    .def("AddFilm", &Film_AddFilm2)
    .def("HasOutput", &luxcore::detail::FilmImpl::HasOutput)
    .def("GetOutputCount", &luxcore::detail::FilmImpl::GetOutputCount)
    .def("SaveOutputs", &luxcore::detail::FilmImpl::SaveOutputs)
    .def("SaveOutput", &luxcore::detail::FilmImpl::SaveOutput)
    .def("SaveFilm", &luxcore::detail::FilmImpl::SaveFilm)
    .def("GetRadianceGroupCount", &luxcore::detail::FilmImpl::GetRadianceGroupCount)
    .def("GetOutputSize", &luxcore::detail::FilmImpl::GetOutputSize)
    .def("GetOutputFloat", &Film_GetOutputFloat1)
    .def("GetOutputFloat", &Film_GetOutputFloat2)
    .def("GetOutputFloat", &Film_GetOutputFloat3)
    .def("GetOutputUInt", &Film_GetOutputUInt1)
    .def("GetOutputUInt", &Film_GetOutputUInt2)
    .def("GetOutputUInt", &Film_GetOutputUInt3)
    .def("UpdateOutputFloat", &Film_UpdateOutputFloat1)
    .def("UpdateOutputFloat", &Film_UpdateOutputFloat2)
    .def("UpdateOutputFloat", &Film_UpdateOutputFloat3)
    .def("UpdateOutputUInt", &Film_UpdateOutputUInt1)
    .def("UpdateOutputUInt", &Film_UpdateOutputUInt2)
    .def("UpdateOutputUInt", &Film_UpdateOutputUInt3)
    .def("Parse", &luxcore::detail::FilmImpl::Parse)
    .def("DeleteAllImagePipelines", &luxcore::detail::FilmImpl::DeleteAllImagePipelines)
    .def("ExecuteImagePipeline", &luxcore::detail::FilmImpl::ExecuteImagePipeline)
    .def("AsyncExecuteImagePipeline", &luxcore::detail::FilmImpl::AsyncExecuteImagePipeline)
    .def("HasDoneAsyncExecuteImagePipeline", &luxcore::detail::FilmImpl::HasDoneAsyncExecuteImagePipeline)
    .def("WaitAsyncExecuteImagePipeline", &luxcore::detail::FilmImpl::WaitAsyncExecuteImagePipeline)
	.def("ApplyOIDN", &Film_ApplyOIDN)
  ;

  //--------------------------------------------------------------------------
  // Camera class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::CameraImpl, py::smart_holder>(m, "Camera")
    .def("Translate", &Camera_Translate)
    .def("TranslateLeft", &luxcore::detail::CameraImpl::TranslateLeft)
    .def("TranslateRight", &luxcore::detail::CameraImpl::TranslateRight)
    .def("TranslateForward", &luxcore::detail::CameraImpl::TranslateForward)
    .def("TranslateBackward", &luxcore::detail::CameraImpl::TranslateBackward)
    .def("Rotate", &Camera_Rotate)
    .def("RotateLeft", &luxcore::detail::CameraImpl::RotateLeft)
    .def("RotateRight", &luxcore::detail::CameraImpl::RotateRight)
    .def("RotateUp", &luxcore::detail::CameraImpl::RotateUp)
    .def("RotateDown", &luxcore::detail::CameraImpl::RotateDown)
  ;

  //--------------------------------------------------------------------------
  // Scene class
  //--------------------------------------------------------------------------

  using luxcore::detail::SceneImpl;
  py::class_<SceneImpl, py::smart_holder>(m, "Scene")
	.def(
		py::init(&SceneImpl::Create<slg::SceneRef>),
		py::keep_alive<1, 2>()
	)
	.def(
		py::init(&SceneImpl::Create<slg::SceneRef>),
		py::keep_alive<1, 2>()
	)
    .def(
		py::init(&SceneImpl::Create<luxrays::PropertiesRPtr, luxrays::PropertiesRPtr>),
		py::keep_alive<1, 2>(),
		py::keep_alive<1, 3>()
	)
    .def(
		py::init(&SceneImpl::Create<luxrays::PropertiesRPtr>),
		py::arg("resizePolicyProps") = nullptr,
		py::keep_alive<1, 2>()
	)
    .def(
		py::init(&SceneImpl::Create<std::string>),
		py::keep_alive<1, 2>()
	)
    .def("ToProperties", &luxcore::detail::SceneImpl::ToProperties)
    .def("GetCamera", &Scene_GetCamera)
    .def("GetLightCount", &luxcore::detail::SceneImpl::GetLightCount)
    .def("GetObjectCount", &luxcore::detail::SceneImpl::GetObjectCount)
    .def("DefineImageMap", &Scene_DefineImageMap)
    .def("IsImageMapDefined", &luxcore::detail::SceneImpl::IsImageMapDefined)
    .def("DefineMesh", &Scene_DefineMesh1)
    .def("DefineMesh", &Scene_DefineMesh2)
    .def(
		"DefineMeshExt",
		&Scene_DefineMeshExt3,
		"Define an extended mesh from Numpy arrays. 'points' can also be the "
		"name of an existing mesh, in which case its point buffer is shared "
		"(not copied) with the new mesh",
		py::arg("name"),
		py::arg("points"),
		py::arg("triangles"),
		py::arg("normals") = py::none(),
		py::arg("uvs") = py::none(),
		py::arg("colors") = py::none(),
		py::arg("alphas") = py::none(),
		py::arg("transformation") = py::none(),
		py::arg("crease_pairs") = py::none(),
		py::arg("crease_weights") = py::none()
	)
	.def("DefineMeshExt", &Scene_DefineMeshExt1)
	.def("DefineMeshExt", &Scene_DefineMeshExt2)
    .def("GetMeshCreaseCount", &Scene_GetMeshCreaseCount, py::arg("name"))
    .def("SetMeshVertexAOV", &Scene_SetMeshVertexAOV)
    .def("SetMeshTriangleAOV", &Scene_SetMeshTriangleAOV)
    .def("SetMeshAppliedTransformation", &Scene_SetMeshAppliedTransformation)
    .def("SaveMesh", &luxcore::detail::SceneImpl::SaveMesh)
    .def("DefineStrands", &Scene_DefineStrands)
    .def("DefineBlenderStrands", &blender::Scene_DefineBlenderStrands)
    .def("DefineBlenderCurveStrands", &blender::Scene_DefineBlenderCurveStrands)
    .def("IsMeshDefined", &luxcore::detail::SceneImpl::IsMeshDefined)
    .def("IsTextureDefined", &luxcore::detail::SceneImpl::IsTextureDefined)
    .def("IsMaterialDefined", &luxcore::detail::SceneImpl::IsMaterialDefined)
    .def("Parse", &luxcore::detail::SceneImpl::Parse, py::keep_alive<1, 2>())
    .def("DuplicateObject", &Scene_DuplicateObject)
    .def("DuplicateObject", &Scene_DuplicateObjectMulti)
    .def("DuplicateObject", &Scene_DuplicateMotionObject)
    .def("DuplicateObject", &Scene_DuplicateMotionObjectMulti)
    .def("DeleteObjects", &Scene_DeleteObjects)
    .def("DeleteLights", &Scene_DeleteLights)
    .def("UpdateObjectTransformation", &Scene_UpdateObjectTransformation)
    .def("UpdateObjectMaterial", &luxcore::detail::SceneImpl::UpdateObjectMaterial)
    .def("DeleteObject", &luxcore::detail::SceneImpl::DeleteObject)
    .def("DeleteLight", &luxcore::detail::SceneImpl::DeleteLight)
    .def("RemoveUnusedImageMaps", &luxcore::detail::SceneImpl::RemoveUnusedImageMaps)
    .def("RemoveUnusedTextures", &luxcore::detail::SceneImpl::RemoveUnusedTextures)
    .def("RemoveUnusedMaterials", &luxcore::detail::SceneImpl::RemoveUnusedMaterials)
    .def("RemoveUnusedMeshes", &luxcore::detail::SceneImpl::RemoveUnusedMeshes)
    .def("Save", &luxcore::detail::SceneImpl::Save)
  ;

  //--------------------------------------------------------------------------
  // RenderConfig class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::RenderConfigImpl, py::smart_holder>(m, "RenderConfig")
    .def(
		py::init(&RenderConfigImpl::Create<luxrays::PropertiesRPtr>),
		py::keep_alive<1, 2>()
	)
    .def(
		py::init(&RenderConfigImpl::Create<luxrays::PropertiesRPtr, SceneImpl&>),
		py::keep_alive<1, 2>(),
		py::keep_alive<1, 3>()
	)
    //.def(py::init(&RenderConfig_LoadFile)) TODO
    .def("GetProperties", &luxcore::detail::RenderConfigImpl::GetProperties)
    .def("GetProperty", &luxcore::detail::RenderConfigImpl::GetProperty)
    .def("GetScene", &RenderConfig_GetScene)
    .def("HasCachedKernels", &luxcore::detail::RenderConfigImpl::HasCachedKernels)
    .def("Parse", &luxcore::detail::RenderConfigImpl::Parse)
    .def("Delete", &luxcore::detail::RenderConfigImpl::Delete)
    .def("GetFilmSize", &RenderConfig_GetFilmSize)
    .def("Save", &luxcore::detail::RenderConfigImpl::Save)
    .def("Export", &luxcore::detail::RenderConfigImpl::Export)
    .def_static("LoadResumeFile", &RenderConfig_LoadResumeFile)
    .def_static("GetDefaultProperties", &luxcore::detail::RenderConfigImpl::GetDefaultProperties)
  ;

  //--------------------------------------------------------------------------
  // RenderState class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::RenderStateImpl, py::smart_holder>(m, "RenderState")
    .def("Save", &RenderState::Save)
  ;

  //--------------------------------------------------------------------------
  // RenderSession class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::RenderSessionImpl, py::smart_holder>(m, "RenderSession")
	.def(
		py::init<>(&RenderSessionImpl::Create<luxcore::detail::RenderConfigImpl&>),
		py::keep_alive<1, 2>()
	)

	.def(
		py::init<>(&RenderSessionImpl::Create
			<RenderConfigImplRef, std::string&, std::string&>
		),
		py::keep_alive<1, 2>()
	)

	.def(
		py::init<>(&RenderSessionImpl::Create
			<RenderConfigImpl&, RenderStateImplRPtr&, FilmImplStandalone& >
		),
		py::keep_alive<1, 2>()
	)
	//TODO
    //.def("GetRenderConfig", &RenderSession_GetRenderConfig)
    .def("GetRenderConfig", &luxcore::detail::RenderSessionImpl::GetRenderConfig)
    .def("Start", &luxcore::detail::RenderSessionImpl::Start)
    .def("Stop", &luxcore::detail::RenderSessionImpl::Stop)
    .def("IsStarted", &luxcore::detail::RenderSessionImpl::IsStarted)
    .def("BeginSceneEdit", &luxcore::detail::RenderSessionImpl::BeginSceneEdit)
    .def("EndSceneEdit", &luxcore::detail::RenderSessionImpl::EndSceneEdit)
    .def("IsInSceneEdit", &luxcore::detail::RenderSessionImpl::IsInSceneEdit)
    .def("Pause", &luxcore::detail::RenderSessionImpl::Pause)
    .def("Resume", &luxcore::detail::RenderSessionImpl::Resume)
    .def("IsInPause", &luxcore::detail::RenderSessionImpl::IsInPause)
    .def("GetFilm", &luxcore::detail::RenderSessionImpl::GetFilmPtr)
    .def("UpdateStats", &luxcore::detail::RenderSessionImpl::UpdateStats)
    .def("GetStats", &luxcore::detail::RenderSessionImpl::GetStats)
    .def("WaitNewFrame", &luxcore::detail::RenderSessionImpl::WaitNewFrame)
    .def("WaitForDone", &luxcore::detail::RenderSessionImpl::WaitForDone)
    .def("HasDone", &luxcore::detail::RenderSessionImpl::HasDone)
    .def("Parse", &luxcore::detail::RenderSessionImpl::Parse)
    .def("GetRenderState", &RenderSession_GetRenderState, py::return_value_policy::take_ownership)
    .def("SaveResumeFile", &luxcore::detail::RenderSessionImpl::SaveResumeFile)
  ;

  m.def("GetOpenVDBGridNames", &GetOpenVDBGridNames);
  m.def("GetOpenVDBGridInfo", &GetOpenVDBGridInfo);
	m.def("BlenderMatrix4x4ToList", &blender::BlenderMatrix4x4ToList);


  // Note: used by pyluxcoredemo.py, do not remove.
  m.def("ConvertFilmChannelOutput_3xFloat_To_4xUChar", ConvertFilmChannelOutput_3xFloat_To_4xUChar);
}

}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
