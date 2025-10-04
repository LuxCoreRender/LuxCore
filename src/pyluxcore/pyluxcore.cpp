/***************************************************************************
 * Copyright 1998-2020 by authors (see AUTHORS.txt)            *
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
#include "luxcore/pyluxcore/pyluxcoreutils.h"

using namespace std;
using namespace luxcore;
namespace py = pybind11;

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
    luxCoreLogHandler(string(msg));
  else {
    // The following code is supposed to work ... but it doesn't (it never
    // returns). So I'm just avoiding to call Python without the GIL and
    // I silently discard the message.

    //PyGILState_STATE state = PyGILState_Ensure();
    //luxCoreLogHandler(string(msg));
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
    luxCoreLogHandler(string(msg));
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

static py::list GetOpenCLDeviceList() {
  luxrays::Context ctx;
  vector<luxrays::DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

  // Select only OpenCL devices
  luxrays::DeviceDescription::Filter((luxrays::DeviceType)(luxrays::DEVICE_TYPE_OPENCL_ALL | luxrays::DEVICE_TYPE_CUDA_ALL), deviceDescriptions);

  // Add all device information to the list
  py::list l;
  for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
    luxrays::DeviceDescription *desc = deviceDescriptions[i];

    l.append(py::make_tuple(
        desc->GetName(),
        luxrays::DeviceDescription::GetDeviceType(desc->GetType()),
        desc->GetComputeUnits(),
        desc->GetNativeVectorWidthFloat(),
        desc->GetMaxMemory(),
        desc->GetMaxMemoryAllocSize()));
  }

  return l;
}

static void LuxCore_KernelCacheFill1() {
  KernelCacheFill(luxrays::Properties());
}

static void LuxCore_KernelCacheFill2(const luxrays::Properties &config) {
  KernelCacheFill(config);
}

//------------------------------------------------------------------------------
// OpenVDB helper functions
//------------------------------------------------------------------------------
py::list GetOpenVDBGridNames(const string &filePathStr) {
  py::list gridNames;

  openvdb::io::File file(filePathStr);
  file.open();
  for (auto i = file.beginName(); i != file.endName(); ++i)
    gridNames.append(*i);

  file.close();
  return gridNames;
}


py::tuple GetOpenVDBGridInfo(const string &filePathStr, const string &gridName) {
  py::list bBox;
  py::list bBox_w;
  py::list trans_matrix;
  py::list BlenderMetadata;
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

static py::list Property_GetBlobByIndex(luxrays::Property *prop, const size_t i) {
  const luxrays::Blob &blob = prop->Get<const luxrays::Blob &>(i);
  const char *data = blob.GetData();
  const size_t size = blob.GetSize();

  py::list l;
  for (size_t i = 0; i < size; ++i)
    l.append((int)data[i]);

  return l;
}

static py::list Property_Get(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i) {
    const luxrays::PropertyValue::DataType dataType = prop->GetValueType(i);

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
        l.append(prop->Get<string>(i));
        break;
      case luxrays::PropertyValue::BLOB_VAL:
        l.append(Property_GetBlobByIndex(prop, i));
        break;
      default:
        throw runtime_error("Unsupported data type in list extraction of a Property: " + prop->GetName());
    }
  }

  return l;
}

static py::list Property_GetBools(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<bool>(i));
  return l;
}

static py::list Property_GetInts(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<long long>(i));
  return l;
}

static py::list Property_GetFloats(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<double>(i));
  return l;
}

static py::list Property_GetStrings(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(prop->Get<string>(i));
  return l;
}

static py::list Property_GetBlobs(luxrays::Property *prop) {
  py::list l;
  for (u_int i = 0; i < prop->GetSize(); ++i)
    l.append(Property_GetBlobByIndex(prop, i));
  return l;
}

static bool Property_GetBool(luxrays::Property *prop) {
  return prop->Get<bool>(0);
}

static long long Property_GetInt(luxrays::Property *prop) {
  return prop->Get<long long>(0);
}

static unsigned long long Property_GetUnsignedLongLong(luxrays::Property *prop) {
  return prop->Get<unsigned long long>(0);
}

static double Property_GetFloat(luxrays::Property *prop) {
  return prop->Get<double>(0);
}

static string Property_GetString(luxrays::Property *prop) {
  return prop->Get<string>(0);
}

static py::list Property_GetBlob(luxrays::Property *prop) {
  return Property_GetBlobByIndex(prop, 0);
}

static luxrays::Property &Property_Add(luxrays::Property *prop, const py::list &l) {
  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
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
      const string v = py::cast<string>(obj);
      prop->Add(v);
    } else if (objType == "list") {
      const py::list ol = py::cast<py::list>(obj);

      const py::ssize_t os = len(ol);

      vector<char> data(os);
      for (py::ssize_t i = 0; i < os; ++i)
        data[i] = py::cast<int>(ol[i]);

      prop->Add(luxrays::Blob(&data[0], os));
    } else if (PyObject_CheckBuffer(obj.ptr())) {
      Py_buffer view;
      if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
        const char *buffer = (char *)view.buf;
        const size_t size = (size_t)view.len;

        luxrays::Blob blob(buffer, size);
        prop->Add(blob);

        PyBuffer_Release(&view);
      } else
        throw runtime_error("Unable to get a data view in Property.Add() method: " + objType);
    } else
      throw runtime_error("Unsupported data type included in Property.Add() method list: " + objType);
  }

  return *prop;
}

static luxrays::Property &Property_AddAllBool(luxrays::Property *prop,
    const py::object &obj) {
  vector<bool> v;
  GetArray<bool>(obj, v);

  for (auto e : v)
    prop->Add<bool>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllInt(luxrays::Property *prop,
    const py::object &obj) {
  vector<long long> v;
  GetArray<long long>(obj, v);

  for (auto e : v)
    prop->Add<long long>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllUnsignedLongLong(luxrays::Property *prop,
    const py::object &obj) {
  vector<unsigned long long> v;
  GetArray<unsigned long long>(obj, v);

  for (auto e : v)
    prop->Add<unsigned long long>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllFloat(luxrays::Property *prop,
    const py::object &obj) {
  vector<float> v;
  GetArray<float>(obj, v);

  for (auto e : v)
    prop->Add<float>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllBoolStride(luxrays::Property *prop,
    const py::object &obj, const size_t width, const size_t stride) {
  vector<bool> v;
  GetArray<bool>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<bool>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllIntStride(luxrays::Property *prop,
    const py::object &obj, const size_t width, const size_t stride) {
  vector<long long> v;
  GetArray<long long>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<long long>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllUnsignedLongLongStride(luxrays::Property *prop,
    const py::object &obj, const size_t width, const size_t stride) {
  vector<unsigned long long> v;
  GetArray<unsigned long long>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<unsigned long long>(e);

  return *prop;
}

static luxrays::Property &Property_AddAllFloatStride(luxrays::Property *prop,
    const py::object &obj, const size_t width, const size_t stride) {
  vector<float> v;
  GetArray<float>(obj, v, width, stride);

  for (auto e : v)
    prop->Add<float>(e);

  return *prop;
}

static luxrays::Property &Property_Set(luxrays::Property *prop, const size_t i,
    const py::object &obj) {
  const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));

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
    const string v = py::cast<string>(obj);
    prop->Set(i, v);
  } else if (objType == "list") {
    const py::list ol = py::cast<py::list>(obj);

    const py::ssize_t os = len(ol);

    vector<char> data(os);
    for (py::ssize_t i = 0; i < os; ++i)
      data[i] = py::cast<int>(ol[i]);

    prop->Set(i, luxrays::Blob(&data[0], os));
  } else if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      const char *buffer = (char *)view.buf;
      const size_t size = (size_t)view.len;

      luxrays::Blob blob(buffer, size);
      prop->Set(i, blob);

      PyBuffer_Release(&view);
    } else
      throw runtime_error("Unable to get a data view in Property.Set() method: " + objType);
  } else
    throw runtime_error("Unsupported data type used for Property.Set() method: " + objType);

  return *prop;
}

static luxrays::Property &Property_Set(luxrays::Property *prop, const py::list &l) {
  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const py::object obj = l[i];
    Property_Set(prop, i, obj);
  }

  return *prop;
}

static luxrays::Property *Property_InitWithList(const py::str &name, const py::list &l) {
  luxrays::Property *prop = new luxrays::Property(py::cast<string>(name));

  Property_Add(prop, l);

  return prop;
}

//------------------------------------------------------------------------------
// Glue for Properties class
//------------------------------------------------------------------------------

static py::list Properties_GetAllNamesRE(luxrays::Properties *props, const string &pattern) {
  py::list l;
  const vector<string> &keys = props->GetAllNamesRE(pattern);
  for(const string &key: keys) {
    l.append(key);
  }

  return l;
}

static py::list Properties_GetAllNames1(luxrays::Properties *props) {
  py::list l;
  const vector<string> &keys = props->GetAllNames();
  for(const string &key: keys) {
    l.append(key);
  }

  return l;
}

static py::list Properties_GetAllNames2(luxrays::Properties *props, const string &prefix) {
  py::list l;
  const vector<string> keys = props->GetAllNames(prefix);
  for(const string &key: keys) {
    l.append(key);
  }

  return l;
}

static py::list Properties_GetAllUniqueSubNames(luxrays::Properties *props, const string &prefix) {
  py::list l;
  const vector<string> keys = props->GetAllUniqueSubNames(prefix);
  for(const string &key: keys) {
    l.append(key);
  }

  return l;
}

static luxrays::Property Properties_GetWithDefaultValues(luxrays::Properties *props,
    const string &name, const py::list &l) {
  luxrays::PropertyValues values;

  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));

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
      const string v = py::cast<string>(l[i]);
      values.push_back(v);
    } else
      throw runtime_error("Unsupported data type included in Properties Get with default method: " + objType);
  }

  return props->Get(luxrays::Property(name, values));
}

void Properties_DeleteAll(luxrays::Properties *props, const py::list &l) {
  const py::ssize_t size = len(l);
  for (py::ssize_t i = 0; i < size; ++i) {
    const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "str")
      props->Delete(py::cast<string>(l[i]));
    else
      throw runtime_error("Unsupported data type included in Properties.DeleteAll() list: " + objType);
  }
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

static void Film_GetOutputFloat1(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj, const size_t index, const bool executeImagePipeline) {
  const size_t outputSize = film->GetOutputSize(type) * sizeof(float);

  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= outputSize) {
        if(!film->HasOutput(type)) {
          const string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw runtime_error(errorMsg);
        }

        float *buffer = (float *)view.buf;

        film->GetOutput<float>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const string errorMsg = "Not enough space in the buffer of Film.GetOutputFloat() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(outputSize);
        PyBuffer_Release(&view);

        throw runtime_error(errorMsg);
      }
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Film.GetOutputFloat() method: " + objType);
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
            if(!film->HasOutput(type)) {
              throw runtime_error("Film Output not available: " + luxrays::ToString(type));
            }

            film->GetOutput<float>(type, bglBuffer->buf.asfloat, index, executeImagePipeline);
          } else
            throw runtime_error("Not enough space in the Blender bgl.Buffer of Film.GetOutputFloat() method: " +
                luxrays::ToString(bglBuffer->dimensions[0] * sizeof(float)) + " instead of " + luxrays::ToString(outputSize));
        } else
          throw runtime_error("A Blender bgl.Buffer has the wrong dimension in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->ndimensions));
      } else
        throw runtime_error("A Blender bgl.Buffer has the wrong type in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->type));
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
    }
  }
}

static void Film_GetOutputFloat2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_GetOutputFloat1(film, type, obj, 0, true);
}

static void Film_GetOutputFloat3(
    luxcore::detail::FilmImpl *film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index) {
  Film_GetOutputFloat1(film, type, obj, index, true);
}

static void Film_GetOutputUInt1(
    luxcore::detail::FilmImpl *film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index,
    const bool executeImagePipeline) {
  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= film->GetOutputSize(type) * sizeof(u_int)) {
        if(!film->HasOutput(type)) {
          const string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw runtime_error(errorMsg);
        }

        u_int *buffer = (u_int *)view.buf;

        film->GetOutput<unsigned int>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const string errorMsg = "Not enough space in the buffer of Film.GetOutputUInt() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(film->GetOutputSize(type) * sizeof(u_int));
        PyBuffer_Release(&view);

        throw runtime_error(errorMsg);
      }
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Film.GetOutputUInt() method: " + objType);
    }
  } else {
    const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
    throw runtime_error("Unsupported data type in Film.GetOutputUInt() method: " + objType);
  }
}

static void Film_GetOutputUInt2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_GetOutputUInt1(film, type, obj, 0, true);
}

static void Film_GetOutputUInt3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_GetOutputUInt1(film, type, obj, index, true);
}

//------------------------------------------------------------------------------
// File UpdateOutput() related functions
//------------------------------------------------------------------------------

static void Film_UpdateOutputFloat1(
    luxcore::detail::FilmImpl *film,
    const Film::FilmOutputType type,
    py::object &obj,
    const size_t index,
    const bool executeImagePipeline) {
  const size_t outputSize = film->GetOutputSize(type) * sizeof(float);

  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= outputSize) {
        if(!film->HasOutput(type)) {
          const string errorMsg = "Film Output not available: " + luxrays::ToString(type);
          PyBuffer_Release(&view);
          throw runtime_error(errorMsg);
        }

        float *buffer = (float *)view.buf;

        film->UpdateOutput<float>(type, buffer, index, executeImagePipeline);

        PyBuffer_Release(&view);
      } else {
        const string errorMsg = "Not enough space in the buffer of Film.UpdateOutputFloat() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(outputSize);
        PyBuffer_Release(&view);

        throw runtime_error(errorMsg);
      }
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Film.UpdateOutputFloat() method: " + objType);
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
            if(!film->HasOutput(type)) {
              throw runtime_error("Film Output not available: " + luxrays::ToString(type));
            }

            film->UpdateOutput<float>(type, bglBuffer->buf.asfloat, index, executeImagePipeline);
          } else
            throw runtime_error("Not enough space in the Blender bgl.Buffer of Film.UpdateOutputFloat() method: " +
                luxrays::ToString(bglBuffer->dimensions[0] * sizeof(float)) + " instead of " + luxrays::ToString(outputSize));
        } else
          throw runtime_error("A Blender bgl.Buffer has the wrong dimension in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->ndimensions));
      } else
        throw runtime_error("A Blender bgl.Buffer has the wrong type in Film.GetOutputFloat(): " + luxrays::ToString(bglBuffer->type));
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
    }
  }
}

static void Film_UpdateOutputFloat2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_UpdateOutputFloat1(film, type, obj, 0, false);
}

static void Film_UpdateOutputFloat3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_UpdateOutputFloat1(film, type, obj, index, false);
}

static void Film_UpdateOutputUInt1(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj, const size_t index, const bool executeImagePipeline) {
  throw runtime_error("Film Output not available: " + luxrays::ToString(type));
}

static void Film_UpdateOutputUInt2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj) {
  Film_UpdateOutputUInt1(film, type, obj, 0, false);
}

static void Film_UpdateOutputUInt3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
    py::object &obj, const size_t index) {
  Film_UpdateOutputUInt1(film, type, obj, index, false);
}

//------------------------------------------------------------------------------

static void Film_AddFilm1(luxcore::detail::FilmImpl *film, luxcore::detail::FilmImpl *srcFilm) {
  film->AddFilm(*srcFilm);
}

static void Film_AddFilm2(luxcore::detail::FilmImpl *film, luxcore::detail::FilmImpl *srcFilm,
    const size_t srcOffsetX, const size_t srcOffsetY,
    const size_t srcWidth, const size_t srcHeight,
    const size_t dstOffsetX, const size_t dstOffsetY) {
  film->AddFilm(*srcFilm, srcOffsetX,  srcOffsetY, srcWidth,  srcHeight, dstOffsetX,  dstOffsetY);
}

static float Film_GetFilmY1(luxcore::detail::FilmImpl *film) {
  return film->GetFilmY();
}

static float Film_GetFilmY2(luxcore::detail::FilmImpl *film, const size_t imagePipelineIndex) {
  return film->GetFilmY(imagePipelineIndex);
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

static luxcore::detail::CameraImpl &Scene_GetCamera(luxcore::detail::SceneImpl *scene) {
  return (luxcore::detail::CameraImpl &)scene->GetCamera();
}

static void Scene_DefineImageMap(luxcore::detail::SceneImpl *scene, const string &imgMapName,
    py::object &obj, const float gamma,
    const size_t channels, const size_t width, const size_t height) {
  if (PyObject_CheckBuffer(obj.ptr())) {
    Py_buffer view;
    if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
      if ((size_t)view.len >= width * height * channels * sizeof(float)) {
        float *buffer = (float *)view.buf;
        scene->DefineImageMap(imgMapName, buffer, gamma, channels,
            width, height, Scene::DEFAULT, Scene::REPEAT);

        PyBuffer_Release(&view);
      } else {
        const string errorMsg = "Not enough space in the buffer of Scene.DefineImageMap() method: " +
            luxrays::ToString(view.len) + " instead of " + luxrays::ToString(width * height * channels * sizeof(float));
        PyBuffer_Release(&view);

        throw runtime_error(errorMsg);
      }
    } else {
      const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Scene.DefineImageMap() method: " + objType);
    }
  }  else {
    const string objType = py::cast<string>((obj.attr("__class__")).attr("__name__"));
    throw runtime_error("Unsupported data type Scene.DefineImageMap() method: " + objType);
  }
}

static void Scene_DefineMesh1(luxcore::detail::SceneImpl *scene, const string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas,
    const py::object &transformation) {
  // NOTE: I would like to use boost::scoped_array but
  // some guy has decided that boost::scoped_array must not have
  // a release() method for some ideological reason...

  // Translate all vertices
  long plyNbVerts;
  luxrays::Point *points = NULL;
  if (py::isinstance<py::list>(p)) {
    const py::list &l = py::cast<py::list>(p);
    const py::ssize_t size = len(l);
    plyNbVerts = size;

    points = (luxrays::Point *)luxcore::detail::SceneImpl::AllocVerticesBuffer(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if (py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        points[i] = luxrays::Point(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
      } else {
        const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type in the list of vertices of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const string objType = py::cast<string>((p.attr("__class__")).attr("__name__"));
    throw runtime_error("Wrong data type for the list of vertices of method Scene.DefineMesh(): " + objType);
  }

  // Translate all triangles
  long plyNbTris;
  luxrays::Triangle *tris = NULL;
  if (py::isinstance<py::list>(vi)) {
    const py::list &l = py::cast<py::list>(vi);
    const py::ssize_t size = len(l);
    plyNbTris = size;

    tris = (luxrays::Triangle *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if (py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        tris[i] = luxrays::Triangle(py::cast<u_int>(t[0]), py::cast<u_int>(t[1]), py::cast<u_int>(t[2]));
      } else {
        const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const string objType = py::cast<string>((vi.attr("__class__")).attr("__name__"));
    throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
  }

  // Translate all normals
  luxrays::Normal *normals = NULL;
  if (!n.is_none()) {
    if(py::isinstance<py::list>(n)) {
      const py::list &l = py::cast<py::list>(n);
      const py::ssize_t size = len(l);

      normals = new luxrays::Normal[size];
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          normals[i] = luxrays::Normal(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
        } else {
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((n.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
    }
  }

  // Translate all UVs
  luxrays::UV *uvs = NULL;
  if (!uv.is_none()) {
    if(py::isinstance<py::list>(uv)) {
      const py::list &l = py::cast<py::list>(uv);
      const py::ssize_t size = len(l);

      uvs = new luxrays::UV[size];
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          uvs[i] = luxrays::UV(py::cast<float>(t[0]), py::cast<float>(t[1]));
        } else {
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((uv.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMesh(): " + objType);
    }
  }

  // Translate all colors
  luxrays::Spectrum *colors = NULL;
  if (!cols.is_none()) {
    if(py::isinstance<py::list>(cols)) {
      const py::list &l = py::cast<py::list>(cols);
      const py::ssize_t size = len(l);

      colors = new luxrays::Spectrum[size];
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          colors[i] = luxrays::Spectrum(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
        } else {
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of colors of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((cols.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMesh(): " + objType);
    }
  }

  // Translate all alphas
  float *as = NULL;
  if (!alphas.is_none()) {
    if(py::isinstance<py::list>(alphas)) {
      const py::list &l = py::cast<py::list>(alphas);
      const py::ssize_t size = len(l);

      as = new float[size];
      for (py::ssize_t i = 0; i < size; ++i)
        as[i] = py::cast<float>(l[i]);
    } else {
      const string objType = py::cast<string>((alphas.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of alphas of method Scene.DefineMesh(): " + objType);
    }
  }

  luxrays::ExtTriangleMesh *mesh = new luxrays::ExtTriangleMesh(plyNbVerts, plyNbTris, points, tris, normals, uvs, colors, as);

  // Apply the transformation if required
  if (!transformation.is_none()) {
    float mat[16];
    GetMatrix4x4(transformation, mat);
    mesh->ApplyTransform(luxrays::Transform(luxrays::Matrix4x4(mat).Transpose()));
  }

  mesh->SetName(meshName);
  scene->DefineMesh(mesh);
}

static void Scene_DefineMesh2(luxcore::detail::SceneImpl *scene, const string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas) {
  Scene_DefineMesh1(scene, meshName, p, vi, n, uv, cols, alphas, py::none());
}

static void Scene_DefineMeshExt1(luxcore::detail::SceneImpl *scene, const string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas,
    const py::object &transformation) {
  // NOTE: I would like to use boost::scoped_array but
  // some guy has decided that boost::scoped_array must not have
  // a release() method for some ideological reason...

  // Translate all vertices
  long plyNbVerts;
  luxrays::Point *points = NULL;
  if(py::isinstance<py::list>(p)) {
    const py::list &l = py::cast<py::list>(p);
    const py::ssize_t size = len(l);
    plyNbVerts = size;

    points = (luxrays::Point *)luxcore::detail::SceneImpl::AllocVerticesBuffer(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if(py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        points[i] = luxrays::Point(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
      } else {
        const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type in the list of vertices of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const string objType = py::cast<string>((p.attr("__class__")).attr("__name__"));
    throw runtime_error("Wrong data type for the list of vertices of method Scene.DefineMeshExt(): " + objType);
  }

  // Translate all triangles
  long plyNbTris;
  luxrays::Triangle *tris = NULL;
  if(py::isinstance<py::list>(vi)) {
    const py::list &l = py::cast<py::list>(vi);
    const py::ssize_t size = len(l);
    plyNbTris = size;

    tris = (luxrays::Triangle *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(size);
    for (py::ssize_t i = 0; i < size; ++i) {
      if(py::isinstance<py::tuple>(l[i])) {
        const py::tuple &t = py::cast<py::tuple>(l[i]);
        tris[i] = luxrays::Triangle(py::cast<u_int>(t[0]), py::cast<u_int>(t[1]), py::cast<u_int>(t[2]));
      } else {
        const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
        throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
      }
    }
  } else {
    const string objType = py::cast<string>((vi.attr("__class__")).attr("__name__"));
    throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
  }

  // Translate all normals
  luxrays::Normal *normals = NULL;
  if (!n.is_none()) {
    if(py::isinstance<py::list>(n)) {
      const py::list &l = py::cast<py::list>(n);
      const py::ssize_t size = len(l);

      normals = new luxrays::Normal[size];
      for (py::ssize_t i = 0; i < size; ++i) {
        if(py::isinstance<py::tuple>(l[i])) {
          const py::tuple &t = py::cast<py::tuple>(l[i]);
          normals[i] = luxrays::Normal(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
        } else {
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((n.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
    }
  }

  // Translate all UVs
  std::array<luxrays::UV *, LC_MESH_MAX_DATA_COUNT> uvs;
  fill(uvs.begin(), uvs.end(), nullptr);
  if (!uv.is_none()) {
    if(py::isinstance<py::list>(uv)) {
      const py::list &uvsArray = py::cast<py::list>(uv);
      const py::ssize_t uvsArraySize = luxrays::Min<py::ssize_t>(len(uvsArray), LC_MESH_MAX_DATA_COUNT);

      for (py::ssize_t j= 0; j < uvsArraySize; ++j) {
        if(py::isinstance<py::list>(uv)) {
          const py::list &l = py::cast<py::list>(uv);  // TODO: Shouldn't it be uv[j]?

          if (!l.is_none()) {
            const py::ssize_t size = len(l);

            uvs[j] = new luxrays::UV[size];
            for (py::ssize_t i = 0; i < size; ++i) {
              if(py::isinstance<py::tuple>(l[i])) {
                const py::tuple &t = py::cast<py::tuple>(l[i]);
                uvs[j][i] = luxrays::UV(py::cast<float>(t[0]), py::cast<float>(t[1]));
              } else {
                const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
                throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
              }
            }
          }
        } else {
          const string objType = py::cast<string>((uv.attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMeshExt(): " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((uv.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMeshExt(): " + objType);
    }
  }

  // Translate all colors
  std::array<luxrays::Spectrum *, LC_MESH_MAX_DATA_COUNT> colors;
  fill(colors.begin(), colors.end(), nullptr);
  if (!cols.is_none()) {
    if(py::isinstance<py::list>(cols)) {
      const py::list &colorsArray = py::cast<py::list>(cols);
      const py::ssize_t colorsArraySize = luxrays::Min<py::ssize_t>(len(colorsArray), LC_MESH_MAX_DATA_COUNT);

      for (py::ssize_t j= 0; j < colorsArraySize; ++j) {
        if(py::isinstance<py::list>(cols)) {
          const py::list &l = py::cast<py::list>(cols);  // TODO Shouldn't it be cols[j]?
          if (!l.is_none()) {
            const py::ssize_t size = len(l);

            colors[j] = new luxrays::Spectrum[size];
            for (py::ssize_t i = 0; i < size; ++i) {
              if(py::isinstance<py::tuple>(l[i])) {
                const py::tuple &t = py::cast<py::tuple>(l[i]);
                colors[j][i] = luxrays::Spectrum(py::cast<float>(t[0]), py::cast<float>(t[1]), py::cast<float>(t[2]));
              } else {
                const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
                throw runtime_error("Wrong data type in the list of colors of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
              }
            }
          }
        } else {
          const string objType = py::cast<string>((cols.attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMeshExt(): " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((cols.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMeshExt(): " + objType);
    }
  }

  // Translate all alphas
  std::array<float *, LC_MESH_MAX_DATA_COUNT> as;
  fill(as.begin(), as.end(), nullptr);
  if (!alphas.is_none()) {
    if(py::isinstance<py::list>(alphas)) {
      const py::list &asArray = py::cast<py::list>(alphas);
      const py::ssize_t asArraySize = luxrays::Min<py::ssize_t>(len(asArray), LC_MESH_MAX_DATA_COUNT);

      for (py::ssize_t j= 0; j < asArraySize; ++j) {
        if(py::isinstance<py::list>(alphas)) {  // TODO Shouldn't it be alpahas[j]?
          const py::list &l = py::cast<py::list>(alphas);

          if (!l.is_none()) {
            const py::ssize_t size = len(l);

            as[j] = new float[size];
            for (py::ssize_t i = 0; i < size; ++i)
              as[j][i] = py::cast<float>(l[i]);
          }
        } else {
          const string objType = py::cast<string>((alphas.attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type for the list of alphas of method Scene.DefineMeshExt(): " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((alphas.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of alphas of method Scene.DefineMeshExt(): " + objType);
    }
  }

  luxrays::ExtTriangleMesh *mesh = new luxrays::ExtTriangleMesh(plyNbVerts, plyNbTris, points, tris, normals, &uvs, &colors, &as);

  // Apply the transformation if required
  if (!transformation.is_none()) {
    float mat[16];
    GetMatrix4x4(transformation, mat);
    mesh->ApplyTransform(luxrays::Transform(luxrays::Matrix4x4(mat).Transpose()));
  }

  mesh->SetName(meshName);
  scene->DefineMesh(mesh);
}

static void Scene_DefineMeshExt2(luxcore::detail::SceneImpl *scene, const string &meshName,
    const py::object &p, const py::object &vi,
    const py::object &n, const py::object &uv,
    const py::object &cols, const py::object &alphas) {
  Scene_DefineMeshExt1(scene, meshName, p, vi, n, uv, cols, alphas, py::none());
}


// TODO make template
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

// Helper for DefineMeshExt3
// Allocate internal data structure and copy array inside
template<
	typename S,  // Source type
	typename D,  // Destination type
	D* (*Allocator)(unsigned int),
	size_t stride=3
>
std::tuple<std::unique_ptr<D>, u_int> dataCopy(
	py::array_t<S, py::array::c_style> src,
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
    auto dest = std::unique_ptr<D>(Allocator(count));
	std::memcpy(dest.get(), direct_src.data(0, 0), direct_src.nbytes());

	return std::tuple(std::move(dest), count);
}

// Variant of DataCopy for optional arguments
template<
	typename S,  // Source type
	typename D,  // Destination type
	D* (*Allocator)(unsigned int),
	size_t stride=3
>
std::tuple<std::unique_ptr<D>, u_int> dataCopyOptional(
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

// Define Mesh from Numpy arrays
static void Scene_DefineMeshExt3(
	luxcore::detail::SceneImpl *scene,
	const string &meshName,
    const py_float_array p,
	const py::array_t<triangle_underlying_type, py::array::c_style > tri,
    const std::optional<py_float_array> n,
    const std::optional<std::vector<py_float_array>> uv_layers,
    const std::optional<std::vector<py_float_array>> color_layers,
    const std::optional<std::vector<py_float_array>> alpha_layers,
    const std::optional<py_float_array> transformation
) {
	// TODO Release GIL when possible

	// Points
	auto [points, numPoints] = dataCopy<
		float,
		luxrays::Point,
		&luxcore::detail::SceneImpl::AllocVerticesBuffer,
		3
	> (p, meshName, "Points");

	// Triangles
	auto [triangles, numTriangles] = dataCopy<
		triangle_underlying_type,
		luxrays::Triangle,
		&luxcore::detail::SceneImpl::AllocTrianglesBuffer,
		3
	> (tri, meshName, "Triangles");

	// Normals
	auto [normals, numNormals] = dataCopyOptional<
		float,
		luxrays::Normal,
		&AllocNormalsBuffer,
		3
	> (n, meshName, "Normals");

	// UV
	std::array<luxrays::UV *, EXTMESH_MAX_DATA_COUNT> meshUVs;
	std::fill(meshUVs.begin(), meshUVs.end(), nullptr);
	if (uv_layers.has_value()) {
		auto uv_layers_count = uv_layers.value().size();
		if (uv_layers_count > EXTMESH_MAX_DATA_COUNT) {
			throw runtime_error("Too many UV Maps in list for method Scene.DefineMesh()");
		}

		for (size_t i = 0; i < uv_layers_count; ++i) {
			auto uv_layer = uv_layers.value()[i];

			auto [uv, numUV] = dataCopy< float, luxrays::UV, &AllocUVBuffer, 2>(
				uv_layer, meshName, "uv"
			);
			meshUVs[i] = uv.release();
		}
	}

	// Colors
	std::array<luxrays::Spectrum *, EXTMESH_MAX_DATA_COUNT> meshCols;
	std::fill(meshCols.begin(), meshCols.end(), nullptr);
	if (color_layers.has_value()) {
		auto color_layer_count = color_layers.value().size();
		if (color_layer_count > EXTMESH_MAX_DATA_COUNT) {
			throw runtime_error("Too many color attributes for method Scene.DefineMesh()");
		}

		for (size_t i = 0; i < color_layer_count; ++i) {
			auto color_layer = color_layers.value()[i];

			auto [colors, numColors] = dataCopy< float, luxrays::Spectrum, &AllocRGBBuffer, 3>(
				color_layer, meshName, "colors"
			);
			meshCols[i] = colors.release();
		}
	}

	// Alphas
	std::array<float *, EXTMESH_MAX_DATA_COUNT> meshAlphas;
	std::fill(meshAlphas.begin(), meshAlphas.end(), nullptr);
	if (alpha_layers.has_value()) {
		auto alpha_layer_count = alpha_layers.value().size();
		if (alpha_layer_count > EXTMESH_MAX_DATA_COUNT) {
			throw runtime_error("Too many alpha attributes for method Scene.DefineMesh()");
		}

		for (size_t i = 0; i < alpha_layer_count; ++i) {
			auto alpha_layer = alpha_layers.value()[i];

			// In caller's logic, alpha_layer shape must be (N,), but in
			// DataCopy logic, it must be (N,1), so we reshape
			pybind11::array::ShapeContainer new_shape({alpha_layer.shape(0), 1,});
			auto reshaped_alpha_layer = alpha_layer.reshape(new_shape);
			auto [alphas, numAlphas] = dataCopy< float, float, &AllocAlphaBuffer, 1>(
				reshaped_alpha_layer, meshName, "alphas"
			);
			meshAlphas[i] = alphas.release();
		}
	}

	// Create Mesh
	auto* newMesh =  new luxrays::ExtTriangleMesh(
		u_int(numPoints),
		u_int(numTriangles),
		points.release(),
		triangles.release(),
		normals.release(),
		&meshUVs,
		&meshCols,
		&meshAlphas
	);
	newMesh->SetName(meshName);

	if (transformation.has_value()) {
		auto transval = transformation.value();
		luxrays::Transform luxtrans{transval.data()};
		newMesh->ApplyTransform(luxtrans);
	}


	// Insert mesh into the scene
	scene->DefineMesh(newMesh);

}

static void Scene_SetMeshVertexAOV(luxcore::detail::SceneImpl *scene, const string &meshName,
    const size_t index, const py::object &data) {
  vector<float> v;
  GetArray<float>(data, v);
  
  float *vcpy = new float[v.size()];
  copy(v.begin(), v.end(), vcpy);

  scene->SetMeshVertexAOV(meshName, index, vcpy);
}

static void Scene_SetMeshTriangleAOV(
    luxcore::detail::SceneImpl *scene,
    const string &meshName,
    const size_t index,
    const py::object &data) {
  vector<float> t;
  GetArray<float>(data, t);
  
  float *tcpy = new float[t.size()];
  copy(t.begin(), t.end(), tcpy);

  scene->SetMeshTriangleAOV(meshName, index, tcpy);
}

static void Scene_SetMeshAppliedTransformation(
    luxcore::detail::SceneImpl *scene,
    const string &meshName,
    const py::object &transformation) {
  float mat[16];
  GetMatrix4x4(transformation, mat);
  scene->SetMeshAppliedTransformation(meshName, mat);
}

static void Scene_DefineStrands(
    luxcore::detail::SceneImpl *scene,
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
      const string objType = py::cast<string>((segments.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of segments of method Scene.DefineStrands(): " + objType);
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
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of points of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((points.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of points of method Scene.DefineStrands(): " + objType);
    }
  } else
    throw runtime_error("Points list can not be None in method Scene.DefineStrands()");

  // Translate all thickness
  if (!py::isinstance<py::float_>(thickness)) {
    if(py::isinstance<py::list>(thickness)) {
      const py::list &l = py::cast<py::list>(thickness);
      const py::ssize_t size = len(l);

      float *t = strands.GetThicknessArray();
      for (py::ssize_t i = 0; i < size; ++i)
        t[i] = py::cast<float>(l[i]);
    } else {
      const string objType = py::cast<string>((thickness.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of thickness of method Scene.DefineStrands(): " + objType);
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
      const string objType = py::cast<string>((transparency.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of transparency of method Scene.DefineStrands(): " + objType);
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
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of colors of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((colors.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of colors of method Scene.DefineStrands(): " + objType);
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
          const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));
          throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
        }
      }
    } else {
      const string objType = py::cast<string>((uvs.attr("__class__")).attr("__name__"));
      throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineStrands(): " + objType);
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
    throw runtime_error("Tessellation type unknown in method Scene.DefineStrands(): " + tessellationTypeStr);

  scene->DefineStrands(shapeName, strands,
      tessellationType, adaptiveMaxDepth, adaptiveError,
      solidSideCount, solidCapBottom, solidCapTop,
      useCameraPosition);
}

static void Scene_DuplicateObject(luxcore::detail::SceneImpl *scene,
    const string &srcObjName,
    const string &dstObjName,
    const py::object &transformation,
    const size_t objectID) {
  float mat[16];
  GetMatrix4x4(transformation, mat);

  scene->DuplicateObject(srcObjName, dstObjName, mat, objectID);
}

static void Scene_DuplicateObjectMulti(luxcore::detail::SceneImpl *scene,
    const string &srcObjName,
    const string &dstObjNamePrefix,
    const unsigned int count,
    const py::object &transformations,
    const py::object &objectIDs) {
  if (!PyObject_CheckBuffer(objectIDs.ptr())){
    const string objType = py::cast<string>((objectIDs.attr("__class__")).attr("__name__"));
    throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
  }
  if (!PyObject_CheckBuffer(transformations.ptr())) {
    const string objType = py::cast<string>((transformations.attr("__class__")).attr("__name__"));
    throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
  }

  Py_buffer transformationsView;
  if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
    const string objType = py::cast<string>((transformations.attr("__class__")).attr("__name__"));
    throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
  }
  Py_buffer objectIDsView;
  if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
    PyBuffer_Release(&transformationsView);

    const string objType = py::cast<string>((transformations.attr("__class__")).attr("__name__"));
    throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
  }

  const size_t objectIDsBufferSize = sizeof(u_int) * count;
  if ((size_t)objectIDsView.len < objectIDsBufferSize) {
    const string errorMsg = "Not enough objectIDs in the buffer of Scene.DuplicateObject() method: " +
        luxrays::ToString(objectIDsView.len) + " instead of " + luxrays::ToString(objectIDsBufferSize);

    PyBuffer_Release(&objectIDsView);
    PyBuffer_Release(&transformationsView);

    throw runtime_error(errorMsg);
  }
  const size_t transformationsBufferSize = sizeof(float) * 16 * count;
  if ((size_t)transformationsView.len < transformationsBufferSize) {
    const string errorMsg = "Not enough matrices in the buffer of Scene.DuplicateObject() method: " +
        luxrays::ToString(transformationsView.len) + " instead of " + luxrays::ToString(transformationsBufferSize);

    PyBuffer_Release(&objectIDsView);
    PyBuffer_Release(&transformationsView);

    throw runtime_error(errorMsg);
  }

  float *transformationsBuffer = (float *)transformationsView.buf;
  u_int *objectIDsBuffer = (u_int *)objectIDsView.buf;

  scene->DuplicateObject(srcObjName, dstObjNamePrefix, count,
      transformationsBuffer, objectIDsBuffer);

  PyBuffer_Release(&transformationsView);
  PyBuffer_Release(&objectIDsView);
}

static void Scene_DuplicateMotionObject(luxcore::detail::SceneImpl *scene,
    const string &srcObjName,
    const string &dstObjName,
    const size_t steps,
    const py::object &times,
    const py::object &transformations,
    const size_t objectID) {
  if (!times.is_none() && !transformations.is_none()) {
    if(py::isinstance<py::list>(times) && py::isinstance<py::list>(transformations)) {
      const py::list &timesList = py::cast<py::list>(times);
      const py::list &transformationsList = py::cast<py::list>(transformations);

      if ((len(timesList) != steps) || (len(transformationsList) != steps))
        throw runtime_error("Wrong number of elements for the times and/or the list of transformations of method Scene.DuplicateObject()");

      vector<float> timesVec(steps);
      vector<float> transVec(steps * 16);
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
      throw runtime_error("Wrong data type for the list of transformation values of method Scene.DuplicateObject()");
  } else
    throw runtime_error("None times and/or transformations in Scene.DuplicateObject(): " + srcObjName);
}

static void Scene_DuplicateMotionObjectMulti(luxcore::detail::SceneImpl *scene,
    const string &srcObjName,
    const string &dstObjName,
    const unsigned int count,
    const size_t steps,
    const py::object &times,
    const py::object &transformations,
    const py::object &objectIDs) {

    if (!PyObject_CheckBuffer(times.ptr())){
      const string objType = py::cast<string>((times.attr("__class__")).attr("__name__"));
      throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }
    if (!PyObject_CheckBuffer(transformations.ptr())){
      const string objType = py::cast<string>((transformations.attr("__class__")).attr("__name__"));
      throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }
    if (!PyObject_CheckBuffer(objectIDs.ptr())){
      const string objType = py::cast<string>((objectIDs.attr("__class__")).attr("__name__"));
      throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
    }

    Py_buffer timesView;
    if (PyObject_GetBuffer(times.ptr(), &timesView, PyBUF_SIMPLE)) {
      const string objType = py::cast<string>((times.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }
    Py_buffer transformationsView;
    if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
      PyBuffer_Release(&timesView);

      const string objType = py::cast<string>((transformations.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }
    Py_buffer objectIDsView;
    if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);

      const string objType = py::cast<string>((objectIDs.attr("__class__")).attr("__name__"));
      throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
    }

    const size_t timesBufferSize = sizeof(float) * count;
    if ((size_t)timesView.len < timesBufferSize) {
      const string errorMsg = "Not enough times in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(timesView.len) + " instead of " + luxrays::ToString(timesBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw runtime_error(errorMsg);
    }
    const size_t transformationsBufferSize = sizeof(float) * 16 * count;
    if ((size_t)transformationsView.len < transformationsBufferSize) {
      const string errorMsg = "Not enough matrices in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(transformationsView.len) + " instead of " + luxrays::ToString(transformationsBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw runtime_error(errorMsg);
    }
    const size_t objectIDsBufferSize = sizeof(u_int) * count;
    if ((size_t)objectIDsView.len < objectIDsBufferSize) {
      const string errorMsg = "Not enough object IDs in the buffer of Scene.DuplicateObject() method: " +
          luxrays::ToString(objectIDsView.len) + " instead of " + luxrays::ToString(objectIDsBufferSize);

      PyBuffer_Release(&timesView);
      PyBuffer_Release(&transformationsView);
      PyBuffer_Release(&objectIDsView);

      throw runtime_error(errorMsg);
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

static void Scene_DeleteObjects(luxcore::detail::SceneImpl *scene,
    const py::list &l) {
  const py::ssize_t size = len(l);
  vector<string> names;
  names.reserve(size);
  for (py::ssize_t i = 0; i < size; ++i) {
    const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "str")
      names.push_back(py::cast<string>(l[i]));
    else
      throw runtime_error("Unsupported data type included in Scene.DeleteObjects() list: " + objType);
  }

  scene->DeleteObjects(names);
}

static void Scene_DeleteLights(luxcore::detail::SceneImpl *scene,
    const py::list &l) {
  const py::ssize_t size = len(l);
  vector<string> names;
  names.reserve(size);
  for (py::ssize_t i = 0; i < size; ++i) {
    const string objType = py::cast<string>((l[i].attr("__class__")).attr("__name__"));

    if (objType == "str")
      names.push_back(py::cast<string>(l[i]));
    else
      throw runtime_error("Unsupported data type included in Scene.DeleteLights() list: " + objType);
  }

  scene->DeleteLights(names);
}

static void Scene_UpdateObjectTransformation(luxcore::detail::SceneImpl *scene,
    const string &objName,
    const py::object &transformation) {
  float mat[16];
  GetMatrix4x4(transformation, mat);
  scene->UpdateObjectTransformation(objName, mat);
}

//------------------------------------------------------------------------------
// Glue for RenderConfig class
//------------------------------------------------------------------------------

static py::tuple RenderConfig_LoadResumeFile(const py::str &fileNameStr) {
  const string fileName = py::cast<string>(fileNameStr);
  luxcore::detail::RenderStateImpl *startState;
  luxcore::detail::FilmImpl *startFilm;
  luxcore::detail::RenderConfigImpl *config = new luxcore::detail::RenderConfigImpl(fileName, &startState, &startFilm);

  //return py::make_tuple(TransferToPython(config), TransferToPython(startState), TransferToPython(startFilm));  TODO
  return py::make_tuple(config, startState, startFilm);
}

static luxcore::detail::RenderConfigImpl *RenderConfig_LoadFile(const py::str &fileNameStr) {
  const string fileName = py::cast<string>(fileNameStr);
  luxcore::detail::RenderConfigImpl *config = new luxcore::detail::RenderConfigImpl(fileName);

  return config;
}

static luxcore::detail::SceneImpl &RenderConfig_GetScene(luxcore::detail::RenderConfigImpl *renderConfig) {
  return (luxcore::detail::SceneImpl &)renderConfig->GetScene();
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

static luxcore::detail::RenderConfigImpl &RenderSession_GetRenderConfig(luxcore::detail::RenderSessionImpl *renderSession) {
  return (luxcore::detail::RenderConfigImpl &)renderSession->GetRenderConfig();
}

static luxcore::detail::FilmImpl &RenderSession_GetFilm(luxcore::detail::RenderSessionImpl *renderSession) {
  return (luxcore::detail::FilmImpl &)renderSession->GetFilm();
}

static luxcore::detail::RenderStateImpl *RenderSession_GetRenderState(luxcore::detail::RenderSessionImpl *renderSession) {
  return (luxcore::detail::RenderStateImpl *)renderSession->GetRenderState();
}

//------------------------------------------------------------------------------

PYBIND11_MODULE(pyluxcore, m) {
  // I get a crash on Ubuntu 19.10 without this line and this should be
  // good anyway to avoid problems with "," Vs. "." decimal separator, etc.

  try {
    locale::global(locale("C.UTF-8"));
  } catch (runtime_error &) {
    // "C.UTF-8" locale may not exist on some system so I ignore the error
  }

  //np::initialize();  TODO

  //docstring_options doc_options(
    //true,  // Show user defined docstrings
    //true,  // Show python signatures
    //false  // Show C++ signatures
  //);

  // This 'module' is actually a fake package
  m.attr("__path__") = "pyluxcore";
  m.attr("__package__") = "pyluxcore";
  m.attr("__doc__") = "LuxCoreRender Python bindings\n\n"
      "Provides access to the LuxCoreRender API in python\n\n";

  m.def("Version", LuxCoreVersion, "Returns the LuxCore version");

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

  m.def("KernelCacheFill", &LuxCore_KernelCacheFill1);
  m.def("KernelCacheFill", &LuxCore_KernelCacheFill2);

  //--------------------------------------------------------------------------
  // Property class
  //--------------------------------------------------------------------------

  py::class_<luxrays::Property>(m, "Property")
    .def(py::init<string>())
    .def(py::init<string, bool>())
    .def(py::init<string, long long>())
    .def(py::init<string, double>())
    .def(py::init<string, string>())
    //.def("__init__", make_constructor(Property_InitWithList))
    .def(py::init(&Property_InitWithList))

    //.def("GetName", &luxrays::Property::GetName, py::return_value_policy<copy_const_reference>())
    .def("GetName", &luxrays::Property::GetName, py::return_value_policy::copy)
    .def("GetSize", &luxrays::Property::GetSize)
    //.def("Clear", &luxrays::Property::Clear, py::py::return_value_policy::reference_internal)
    .def("Clear", &luxrays::Property::Clear, py::return_value_policy::reference_internal)

    .def("Get", &Property_Get)

    .def<bool (luxrays::Property::*)(const u_int) const>
      ("GetBool", &luxrays::Property::Get)
    .def<long long (luxrays::Property::*)(const u_int) const>
      ("GetInt", &luxrays::Property::Get)
    .def<double (luxrays::Property::*)(const u_int) const>
      ("GetFloat", &luxrays::Property::Get)
    .def<string (luxrays::Property::*)(const u_int) const>
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

    .def("Add", &Property_Add, py::return_value_policy::reference_internal)
    .def("AddAllBool", &Property_AddAllBool, py::return_value_policy::reference_internal)
    .def("AddAllInt", &Property_AddAllInt, py::return_value_policy::reference_internal)
    .def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLong, py::return_value_policy::reference_internal)
    .def("AddAllFloat", &Property_AddAllFloat, py::return_value_policy::reference_internal)
    .def("AddAllBool", &Property_AddAllBoolStride, py::return_value_policy::reference_internal)
    .def("AddAllInt", &Property_AddAllIntStride, py::return_value_policy::reference_internal)
    .def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLongStride, py::return_value_policy::reference_internal)
    .def("AddAllFloat", &Property_AddAllFloatStride, py::return_value_policy::reference_internal)
    .def<luxrays::Property &(*)(luxrays::Property *, const py::list &)>
      ("Set", &Property_Set, py::return_value_policy::reference_internal)
    .def<luxrays::Property &(*)(luxrays::Property *, const size_t, const py::object &)>
      ("Set", &Property_Set, py::return_value_policy::reference_internal)

    //.def(self_ns::str(self))  TODO
    .def("__str__", &luxrays::Property::ToString)
  ;

  //--------------------------------------------------------------------------
  // Properties class
  //--------------------------------------------------------------------------

  py::class_<luxrays::Properties>(m, "Properties")
    .def(py::init<>())
    .def(py::init<string>())
    .def(py::init<luxrays::Properties>())

    // Required because Properties::Set is overloaded
    .def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Property &)>
      ("Set", &luxrays::Properties::Set, py::return_value_policy::reference_internal)
    .def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &)>
      ("Set", &luxrays::Properties::Set, py::return_value_policy::reference_internal)
    .def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &, const string &)>
      ("Set", &luxrays::Properties::Set, py::return_value_policy::reference_internal)
    .def("SetFromFile", &luxrays::Properties::SetFromFile, py::return_value_policy::reference_internal)
    .def("SetFromString", &luxrays::Properties::SetFromString, py::return_value_policy::reference_internal)

    .def("Clear", &luxrays::Properties::Clear, py::return_value_policy::reference_internal)
    .def("GetAllNamesRE", &Properties_GetAllNamesRE)
    .def("GetAllNames", &Properties_GetAllNames1)
    .def("GetAllNames", &Properties_GetAllNames2)
    .def("GetAllUniqueSubNames", &Properties_GetAllUniqueSubNames)
    .def("HaveNames", &luxrays::Properties::HaveNames)
    .def("HaveNamesRE", &luxrays::Properties::HaveNamesRE)
    .def("GetAllProperties", &luxrays::Properties::GetAllProperties)

    .def<const luxrays::Property &(luxrays::Properties::*)(const string &) const>
      ("Get", &luxrays::Properties::Get, py::return_value_policy::reference_internal)
    .def("Get", &Properties_GetWithDefaultValues)

    .def("GetSize", &luxrays::Properties::GetSize)

    .def("IsDefined", &luxrays::Properties::IsDefined)
    .def("Delete", &luxrays::Properties::Delete)
    .def("DeleteAll", &Properties_DeleteAll)
    .def("ToString", &luxrays::Properties::ToString)

    //.def(self_ns::str(self))
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

  py::class_<luxcore::detail::FilmImpl>(m, "Film")
    .def(py::init<string>())
    .def(py::init<luxrays::Properties, bool, bool>())
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
  ;

  //--------------------------------------------------------------------------
  // Camera class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::CameraImpl>(m, "Camera")
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

  py::class_<luxcore::detail::SceneImpl>(m, "Scene")
    .def(py::init<>())
    .def(py::init<luxrays::Properties, luxrays::Properties *>())
    .def(py::init<luxrays::Properties>())
    .def(py::init<string>())
    .def("ToProperties", &luxcore::detail::SceneImpl::ToProperties, py::return_value_policy::reference_internal)
    .def("GetCamera", &Scene_GetCamera, py::return_value_policy::reference_internal)
    .def("GetLightCount", &luxcore::detail::SceneImpl::GetLightCount)
    .def("GetObjectCount", &luxcore::detail::SceneImpl::GetObjectCount)
    .def("DefineImageMap", &Scene_DefineImageMap)
    .def("IsImageMapDefined", &luxcore::detail::SceneImpl::IsImageMapDefined)
    .def("DefineMesh", &Scene_DefineMesh1)
    .def("DefineMesh", &Scene_DefineMesh2)
    .def(
		"DefineMeshExt",
		&Scene_DefineMeshExt3,
		"Define an extended mesh from Numpy arrays",
		py::arg("name"),
		py::arg("points"),
		py::arg("triangles"),
		py::arg("normals") = py::none(),
		py::arg("uvs") = py::none(),
		py::arg("colors") = py::none(),
		py::arg("alphas") = py::none(),
		py::arg("transformation") = py::none()
	)
	.def("DefineMeshExt", &Scene_DefineMeshExt1)
	.def("DefineMeshExt", &Scene_DefineMeshExt2)
    .def("SetMeshVertexAOV", &Scene_SetMeshVertexAOV)
    .def("SetMeshTriangleAOV", &Scene_SetMeshTriangleAOV)
    .def("SetMeshAppliedTransformation", &Scene_SetMeshAppliedTransformation)
    .def("SaveMesh", &luxcore::detail::SceneImpl::SaveMesh)
    .def("DefineBlenderMesh", &blender::Scene_DefineBlenderMesh1)
    .def("DefineBlenderMesh", &blender::Scene_DefineBlenderMesh2)
    .def("DefineStrands", &Scene_DefineStrands)
    .def("DefineBlenderStrands", &blender::Scene_DefineBlenderStrands)
    .def("DefineBlenderCurveStrands", &blender::Scene_DefineBlenderCurveStrands)
    .def("IsMeshDefined", &luxcore::detail::SceneImpl::IsMeshDefined)
    .def("IsTextureDefined", &luxcore::detail::SceneImpl::IsTextureDefined)
    .def("IsMaterialDefined", &luxcore::detail::SceneImpl::IsMaterialDefined)
    .def("Parse", &luxcore::detail::SceneImpl::Parse)
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

  py::class_<luxcore::detail::RenderConfigImpl>(m, "RenderConfig")
    .def(py::init<luxrays::Properties>())
    //.def(py::init<luxrays::Properties, luxcore::detail::SceneImpl *>()[with_custodian_and_ward<1, 3>()])
    .def(py::init<luxrays::Properties, luxcore::detail::SceneImpl *>(), py::keep_alive<1, 3>())
    //.def("__init__", make_constructor(RenderConfig_LoadFile))
    .def(py::init(&RenderConfig_LoadFile))
    .def("GetProperties", &luxcore::detail::RenderConfigImpl::GetProperties, py::return_value_policy::reference_internal)
    .def("GetProperty", &luxcore::detail::RenderConfigImpl::GetProperty)
    .def("GetScene", &RenderConfig_GetScene, py::return_value_policy::reference_internal)
    .def("HasCachedKernels", &luxcore::detail::RenderConfigImpl::HasCachedKernels)
    .def("Parse", &luxcore::detail::RenderConfigImpl::Parse)
    .def("Delete", &luxcore::detail::RenderConfigImpl::Delete)
    .def("GetFilmSize", &RenderConfig_GetFilmSize)
    .def("Save", &luxcore::detail::RenderConfigImpl::Save)
    .def("Export", &luxcore::detail::RenderConfigImpl::Export)
    .def_static("LoadResumeFile", &RenderConfig_LoadResumeFile)
    .def_static("GetDefaultProperties", &luxcore::detail::RenderConfigImpl::GetDefaultProperties, py::return_value_policy::reference_internal)
  ;

  //--------------------------------------------------------------------------
  // RenderState class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::RenderStateImpl>(m, "RenderState")
    .def("Save", &RenderState::Save)
  ;

  //--------------------------------------------------------------------------
  // RenderSession class
  //--------------------------------------------------------------------------

  py::class_<luxcore::detail::RenderSessionImpl>(m, "RenderSession")
    // TODO
    //.def(py::init<luxcore::detail::RenderConfigImpl *>()[with_custodian_and_ward<1, 2>()])
    //.def(init<luxcore::detail::RenderConfigImpl *, string, string>()[with_custodian_and_ward<1, 2>()])
    //.def(init<luxcore::detail::RenderConfigImpl *, luxcore::detail::RenderStateImpl *, luxcore::detail::FilmImpl *>()[with_custodian_and_ward<1, 2>()])
    .def(py::init<luxcore::detail::RenderConfigImpl *>(), py::keep_alive<1, 2>())
    .def(py::init<luxcore::detail::RenderConfigImpl *, string, string>(), py::keep_alive<1, 2>())
    .def(py::init<luxcore::detail::RenderConfigImpl *, luxcore::detail::RenderStateImpl *, luxcore::detail::FilmImpl *>(), py::keep_alive<1, 2>())
    .def("GetRenderConfig", &RenderSession_GetRenderConfig, py::return_value_policy::reference_internal)
    .def("Start", &luxcore::detail::RenderSessionImpl::Start)
    .def("Stop", &luxcore::detail::RenderSessionImpl::Stop)
    .def("IsStarted", &luxcore::detail::RenderSessionImpl::IsStarted)
    .def("BeginSceneEdit", &luxcore::detail::RenderSessionImpl::BeginSceneEdit)
    .def("EndSceneEdit", &luxcore::detail::RenderSessionImpl::EndSceneEdit)
    .def("IsInSceneEdit", &luxcore::detail::RenderSessionImpl::IsInSceneEdit)
    .def("Pause", &luxcore::detail::RenderSessionImpl::Pause)
    .def("Resume", &luxcore::detail::RenderSessionImpl::Resume)
    .def("IsInPause", &luxcore::detail::RenderSessionImpl::IsInPause)
    .def("GetFilm", &RenderSession_GetFilm, py::return_value_policy::reference_internal)
    .def("UpdateStats", &luxcore::detail::RenderSessionImpl::UpdateStats)
    .def("GetStats", &luxcore::detail::RenderSessionImpl::GetStats, py::return_value_policy::reference_internal)
    .def("WaitNewFrame", &luxcore::detail::RenderSessionImpl::WaitNewFrame)
    .def("WaitForDone", &luxcore::detail::RenderSessionImpl::WaitForDone)
    .def("HasDone", &luxcore::detail::RenderSessionImpl::HasDone)
    .def("Parse", &luxcore::detail::RenderSessionImpl::Parse)
    .def("GetRenderState", &RenderSession_GetRenderState, py::return_value_policy::take_ownership)
    .def("SaveResumeFile", &luxcore::detail::RenderSessionImpl::SaveResumeFile)
  ;

  m.def("GetOpenVDBGridNames", &GetOpenVDBGridNames);
  m.def("GetOpenVDBGridInfo", &GetOpenVDBGridInfo);

  //--------------------------------------------------------------------------
  // Blender related functions
  //--------------------------------------------------------------------------

  m.def("ConvertFilmChannelOutput_1xFloat_To_1xFloatList", &blender::ConvertFilmChannelOutput_1xFloat_To_1xFloatList);
  m.def("ConvertFilmChannelOutput_UV_to_Blender_UV", &blender::ConvertFilmChannelOutput_UV_to_Blender_UV);
  m.def("ConvertFilmChannelOutput_1xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_1xFloat_To_4xFloatList);
  m.def("ConvertFilmChannelOutput_3xFloat_To_3xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_3xFloatList);
  m.def("ConvertFilmChannelOutput_3xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_4xFloatList);
  m.def("ConvertFilmChannelOutput_4xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_4xFloat_To_4xFloatList);
  m.def("ConvertFilmChannelOutput_1xUInt_To_1xFloatList", &blender::ConvertFilmChannelOutput_1xUInt_To_1xFloatList);
  m.def("BlenderMatrix4x4ToList", &blender::BlenderMatrix4x4ToList);
  // Note: used by pyluxcoredemo.py, do not remove.
  m.def("ConvertFilmChannelOutput_3xFloat_To_4xUChar", &blender::ConvertFilmChannelOutput_3xFloat_To_4xUChar);
}

}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
