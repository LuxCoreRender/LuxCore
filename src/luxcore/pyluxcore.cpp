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
// MSVC 2017 has snprintf, but Python headers still define it the old way, causing
// error C2039: '_snprintf': is not a member of 'std'
// The problem started with Boost 1.72.0
#define HAVE_SNPRINTF
#endif

// The maximum number of arguments of a function being wrapped
#define BOOST_PYTHON_MAX_ARITY 22

#include <locale>
#include <memory>

#include <boost/foreach.hpp>
#include <boost/python.hpp>
#include <boost/python/call.hpp>
#include <boost/python/numpy.hpp>

#include <Python.h>

#include "luxrays/luxrays.h"
#include "luxcore/luxcore.h"
#include "luxcore/luxcoreimpl.h"
#include "luxcore/pyluxcore/pyluxcoreforblender.h"
#include "luxcore/pyluxcore/pyluxcoreutils.h"

using namespace std;
using namespace luxcore;
using namespace boost::python;
namespace np = boost::python::numpy;

namespace luxcore {

//------------------------------------------------------------------------------
// Module functions
//------------------------------------------------------------------------------

static boost::mutex luxCoreInitMutex;
static boost::python::object luxCoreLogHandler;

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
	boost::unique_lock<boost::mutex> lock(luxCoreInitMutex);
	Init();
}

static void LuxCore_InitDefaultHandler(boost::python::object &logHandler) {
	boost::unique_lock<boost::mutex> lock(luxCoreInitMutex);
	// I wonder if I should increase the reference count for Python
	luxCoreLogHandler = logHandler;

	Init(&PythonDebugHandler);
}

static void LuxCore_SetLogHandler(boost::python::object &logHandler) {
	luxCoreLogHandler = logHandler;

	if (logHandler.is_none())
		SetLogHandler(NULL);
}

static const char *LuxCoreVersion() {
	static const char *luxCoreVersion = LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR;
	return luxCoreVersion;
}

static boost::python::list GetOpenCLDeviceList() {
	luxrays::Context ctx;
	vector<luxrays::DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

	// Select only OpenCL devices
	luxrays::DeviceDescription::Filter((luxrays::DeviceType)(luxrays::DEVICE_TYPE_OPENCL_ALL | luxrays::DEVICE_TYPE_CUDA_ALL), deviceDescriptions);

	// Add all device information to the list
	boost::python::list l;
	for (size_t i = 0; i < deviceDescriptions.size(); ++i) {
		luxrays::DeviceDescription *desc = deviceDescriptions[i];

		l.append(boost::python::make_tuple(
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
// Glue for Property class
//------------------------------------------------------------------------------

static boost::python::list Property_GetBlobByIndex(luxrays::Property *prop, const u_int i) {
	const luxrays::Blob &blob = prop->Get<const luxrays::Blob &>(i);
	const char *data = blob.GetData();
	const size_t size = blob.GetSize();

	boost::python::list l;
	for (size_t i = 0; i < size; ++i)
		l.append((int)data[i]);

	return l;
}

static boost::python::list Property_Get(luxrays::Property *prop) {
	boost::python::list l;
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

static boost::python::list Property_GetBools(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i)
		l.append(prop->Get<bool>(i));
	return l;
}

static boost::python::list Property_GetInts(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i)
		l.append(prop->Get<long long>(i));
	return l;
}

static boost::python::list Property_GetFloats(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i)
		l.append(prop->Get<double>(i));
	return l;
}

static boost::python::list Property_GetStrings(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i)
		l.append(prop->Get<string>(i));
	return l;
}

static boost::python::list Property_GetBlobs(luxrays::Property *prop) {
	boost::python::list l;
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

static boost::python::list Property_GetBlob(luxrays::Property *prop) {
	return Property_GetBlobByIndex(prop, 0);
}

static luxrays::Property &Property_Add(luxrays::Property *prop, const boost::python::list &l) {
	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
		const boost::python::object obj = l[i];

		if (objType == "bool") {
			const bool v = extract<bool>(obj);
			prop->Add(v);
		} else if (objType == "int") {
			const long long v = extract<long long>(obj);
			prop->Add(v);
		} else if (objType == "float") {
			const double v = extract<double>(obj);
			prop->Add(v);
		} else if (objType == "str") {
			const string v = extract<string>(obj);
			prop->Add(v);
		} else if (objType == "list") {
			const boost::python::list ol = extract<boost::python::list>(obj);

			const boost::python::ssize_t os = len(ol);

			vector<char> data(os);
			for (boost::python::ssize_t i = 0; i < os; ++i)
				data[i] = extract<int>(ol[i]);

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
		const boost::python::object &obj) {
	vector<bool> v;
	GetArray<bool>(obj, v);

	for (auto e : v)
		prop->Add<bool>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllInt(luxrays::Property *prop,
		const boost::python::object &obj) {
	vector<long long> v;
	GetArray<long long>(obj, v);

	for (auto e : v)
		prop->Add<long long>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllUnsignedLongLong(luxrays::Property *prop,
		const boost::python::object &obj) {
	vector<unsigned long long> v;
	GetArray<unsigned long long>(obj, v);

	for (auto e : v)
		prop->Add<unsigned long long>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllFloat(luxrays::Property *prop,
		const boost::python::object &obj) {
	vector<float> v;
	GetArray<float>(obj, v);

	for (auto e : v)
		prop->Add<float>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllBoolStride(luxrays::Property *prop,
		const boost::python::object &obj, const u_int width, const u_int stride) {
	vector<bool> v;
	GetArray<bool>(obj, v, width, stride);

	for (auto e : v)
		prop->Add<bool>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllIntStride(luxrays::Property *prop,
		const boost::python::object &obj, const u_int width, const u_int stride) {
	vector<long long> v;
	GetArray<long long>(obj, v, width, stride);

	for (auto e : v)
		prop->Add<long long>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllUnsignedLongLongStride(luxrays::Property *prop,
		const boost::python::object &obj, const u_int width, const u_int stride) {
	vector<unsigned long long> v;
	GetArray<unsigned long long>(obj, v, width, stride);

	for (auto e : v)
		prop->Add<unsigned long long>(e);

	return *prop;
}

static luxrays::Property &Property_AddAllFloatStride(luxrays::Property *prop,
		const boost::python::object &obj, const u_int width, const u_int stride) {
	vector<float> v;
	GetArray<float>(obj, v, width, stride);

	for (auto e : v)
		prop->Add<float>(e);

	return *prop;
}

static luxrays::Property &Property_Set(luxrays::Property *prop, const u_int i,
		const boost::python::object &obj) {
	const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));

	if (objType == "bool") {
		const bool v = extract<bool>(obj);
		prop->Set(i, v);
	} else if (objType == "int") {
		const long long v = extract<long long>(obj);
		prop->Set(i, v);
	} else if (objType == "float") {
		const double v = extract<double>(obj);
		prop->Set(i, v);
	} else if (objType == "str") {
		const string v = extract<string>(obj);
		prop->Set(i, v);
	} else if (objType == "list") {
		const boost::python::list ol = extract<boost::python::list>(obj);

		const boost::python::ssize_t os = len(ol);

		vector<char> data(os);
		for (boost::python::ssize_t i = 0; i < os; ++i)
			data[i] = extract<int>(ol[i]);

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

static luxrays::Property &Property_Set(luxrays::Property *prop, const boost::python::list &l) {
	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const boost::python::object obj = l[i];
		Property_Set(prop, i, obj);
	}

	return *prop;
}

static luxrays::Property *Property_InitWithList(const str &name, const boost::python::list &l) {
	luxrays::Property *prop = new luxrays::Property(extract<string>(name));

	Property_Add(prop, l);

	return prop;
}

//------------------------------------------------------------------------------
// Glue for Properties class
//------------------------------------------------------------------------------

static boost::python::list Properties_GetAllNamesRE(luxrays::Properties *props, const string &pattern) {
	boost::python::list l;
	const vector<string> &keys = props->GetAllNamesRE(pattern);
	BOOST_FOREACH(const string &key, keys) {
		l.append(key);
	}

	return l;
}

static boost::python::list Properties_GetAllNames1(luxrays::Properties *props) {
	boost::python::list l;
	const vector<string> &keys = props->GetAllNames();
	BOOST_FOREACH(const string &key, keys) {
		l.append(key);
	}

	return l;
}

static boost::python::list Properties_GetAllNames2(luxrays::Properties *props, const string &prefix) {
	boost::python::list l;
	const vector<string> keys = props->GetAllNames(prefix);
	BOOST_FOREACH(const string &key, keys) {
		l.append(key);
	}

	return l;
}

static boost::python::list Properties_GetAllUniqueSubNames(luxrays::Properties *props, const string &prefix) {
	boost::python::list l;
	const vector<string> keys = props->GetAllUniqueSubNames(prefix);
	BOOST_FOREACH(const string &key, keys) {
		l.append(key);
	}

	return l;
}

static luxrays::Property Properties_GetWithDefaultValues(luxrays::Properties *props,
		const string &name, const boost::python::list &l) {
	luxrays::PropertyValues values;

	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));

		if (objType == "bool") {
			const bool v = extract<bool>(l[i]);
			values.push_back(v);
		} else if (objType == "int") {
			const long long v = extract<long long>(l[i]);
			values.push_back(v);
		} else if (objType == "float") {
			const double v = extract<double>(l[i]);
			values.push_back(v);
		} else if (objType == "str") {
			const string v = extract<string>(l[i]);
			values.push_back(v);
		} else
			throw runtime_error("Unsupported data type included in Properties Get with default method: " + objType);
	}

	return props->Get(luxrays::Property(name, values));
}

void Properties_DeleteAll(luxrays::Properties *props, const boost::python::list &l) {
	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));

		if (objType == "str")
			props->Delete(extract<string>(l[i]));
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

	int type;		/* GL_BYTE, GL_SHORT, GL_INT, GL_FLOAT */
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
		boost::python::object &obj, const u_int index, const bool executeImagePipeline) {
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
			throw runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
		}
	}
}

static void Film_GetOutputFloat2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_GetOutputFloat1(film, type, obj, 0, true);
}

static void Film_GetOutputFloat3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	Film_GetOutputFloat1(film, type, obj, index, true);
}

static void Film_GetOutputUInt1(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index, const bool executeImagePipeline) {
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
			throw runtime_error("Unable to get a data view in Film.GetOutputUInt() method: " + objType);
		}
	} else {
		const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in Film.GetOutputUInt() method: " + objType);
	}
}

static void Film_GetOutputUInt2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_GetOutputUInt1(film, type, obj, 0, true);
}

static void Film_GetOutputUInt3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	Film_GetOutputUInt1(film, type, obj, index, true);
}

//------------------------------------------------------------------------------
// File UpdateOutput() related functions
//------------------------------------------------------------------------------

static void Film_UpdateOutputFloat1(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index, const bool executeImagePipeline) {
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
			throw runtime_error("Unsupported data type in Film.GetOutputFloat(): " + objType);
		}
	}
}

static void Film_UpdateOutputFloat2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_UpdateOutputFloat1(film, type, obj, 0, false);
}

static void Film_UpdateOutputFloat3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	Film_UpdateOutputFloat1(film, type, obj, index, false);
}

static void Film_UpdateOutputUInt1(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index, const bool executeImagePipeline) {
	throw runtime_error("Film Output not available: " + luxrays::ToString(type));
}

static void Film_UpdateOutputUInt2(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_UpdateOutputUInt1(film, type, obj, 0, false);
}

static void Film_UpdateOutputUInt3(luxcore::detail::FilmImpl *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	Film_UpdateOutputUInt1(film, type, obj, index, false);
}

//------------------------------------------------------------------------------

static void Film_AddFilm1(luxcore::detail::FilmImpl *film, luxcore::detail::FilmImpl *srcFilm) {
	film->AddFilm(*srcFilm);
}

static void Film_AddFilm2(luxcore::detail::FilmImpl *film, luxcore::detail::FilmImpl *srcFilm,
		const u_int srcOffsetX, const u_int srcOffsetY,
		const u_int srcWidth, const u_int srcHeight,
		const u_int dstOffsetX, const u_int dstOffsetY) {
	film->AddFilm(*srcFilm, srcOffsetX,  srcOffsetY, srcWidth,  srcHeight, dstOffsetX,  dstOffsetY);
}

static float Film_GetFilmY1(luxcore::detail::FilmImpl *film) {
	return film->GetFilmY();
}

static float Film_GetFilmY2(luxcore::detail::FilmImpl *film, const u_int imagePipelineIndex) {
	return film->GetFilmY(imagePipelineIndex);
}

//------------------------------------------------------------------------------
// Glue for Camera class
//------------------------------------------------------------------------------

static void Camera_Translate(luxcore::detail::CameraImpl *camera, const boost::python::tuple t) {
	camera->Translate(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
}

static void Camera_Rotate(luxcore::detail::CameraImpl *camera, const float angle, const boost::python::tuple axis) {
	camera->Rotate(angle, extract<float>(axis[0]), extract<float>(axis[1]), extract<float>(axis[2]));
}

//------------------------------------------------------------------------------
// Glue for Scene class
//------------------------------------------------------------------------------

static luxcore::detail::CameraImpl &Scene_GetCamera(luxcore::detail::SceneImpl *scene) {
	return (luxcore::detail::CameraImpl &)scene->GetCamera();
}

static void Scene_DefineImageMap(luxcore::detail::SceneImpl *scene, const string &imgMapName,
		boost::python::object &obj, const float gamma,
		const u_int channels, const u_int width, const u_int height) {
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
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
			throw runtime_error("Unable to get a data view in Scene.DefineImageMap() method: " + objType);
		}
	}	else {
		const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type Scene.DefineImageMap() method: " + objType);
	}
}

static void Scene_DefineMesh1(luxcore::detail::SceneImpl *scene, const string &meshName,
		const boost::python::object &p, const boost::python::object &vi,
		const boost::python::object &n, const boost::python::object &uv,
		const boost::python::object &cols, const boost::python::object &alphas,
		const boost::python::object &transformation) {
	// NOTE: I would like to use boost::scoped_array but
	// some guy has decided that boost::scoped_array must not have
	// a release() method for some ideological reason...

	// Translate all vertices
	long plyNbVerts;
	luxrays::Point *points = NULL;
	extract<boost::python::list> getPList(p);
	if (getPList.check()) {
		const boost::python::list &l = getPList();
		const boost::python::ssize_t size = len(l);
		plyNbVerts = size;

		points = (luxrays::Point *)luxcore::detail::SceneImpl::AllocVerticesBuffer(size);
		for (boost::python::ssize_t i = 0; i < size; ++i) {
			extract<boost::python::tuple> getTuple(l[i]);
			if (getTuple.check()) {
				const boost::python::tuple &t = getTuple();
				points[i] = luxrays::Point(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
			} else {
				const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
				throw runtime_error("Wrong data type in the list of vertices of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
			}
		}
	} else {
		const string objType = extract<string>((p.attr("__class__")).attr("__name__"));
		throw runtime_error("Wrong data type for the list of vertices of method Scene.DefineMesh(): " + objType);
	}

	// Translate all triangles
	long plyNbTris;
	luxrays::Triangle *tris = NULL;
	extract<boost::python::list> getVIList(vi);
	if (getVIList.check()) {
		const boost::python::list &l = getVIList();
		const boost::python::ssize_t size = len(l);
		plyNbTris = size;

		tris = (luxrays::Triangle *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(size);
		for (boost::python::ssize_t i = 0; i < size; ++i) {
			extract<boost::python::tuple> getTuple(l[i]);
			if (getTuple.check()) {
				const boost::python::tuple &t = getTuple();
				tris[i] = luxrays::Triangle(extract<u_int>(t[0]), extract<u_int>(t[1]), extract<u_int>(t[2]));
			} else {
				const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
				throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
			}
		}
	} else {
		const string objType = extract<string>((vi.attr("__class__")).attr("__name__"));
		throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
	}

	// Translate all normals
	luxrays::Normal *normals = NULL;
	if (!n.is_none()) {
		extract<boost::python::list> getNList(n);
		if (getNList.check()) {
			const boost::python::list &l = getNList();
			const boost::python::ssize_t size = len(l);

			normals = new luxrays::Normal[size];
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					normals[i] = luxrays::Normal(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((n.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMesh(): " + objType);
		}
	}

	// Translate all UVs
	luxrays::UV *uvs = NULL;
	if (!uv.is_none()) {
		extract<boost::python::list> getUVList(uv);
		if (getUVList.check()) {
			const boost::python::list &l = getUVList();
			const boost::python::ssize_t size = len(l);

			uvs = new luxrays::UV[size];
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					uvs[i] = luxrays::UV(extract<float>(t[0]), extract<float>(t[1]));
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((uv.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMesh(): " + objType);
		}
	}

	// Translate all colors
	luxrays::Spectrum *colors = NULL;
	if (!cols.is_none()) {
		extract<boost::python::list> getColList(cols);
		if (getColList.check()) {
			const boost::python::list &l = getColList();
			const boost::python::ssize_t size = len(l);

			colors = new luxrays::Spectrum[size];
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					colors[i] = luxrays::Spectrum(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of colors of method Scene.DefineMesh() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((cols.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMesh(): " + objType);
		}
	}

	// Translate all alphas
	float *as = NULL;
	if (!alphas.is_none()) {
		extract<boost::python::list> getAlphaList(alphas);
		if (getAlphaList.check()) {
			const boost::python::list &l = getAlphaList();
			const boost::python::ssize_t size = len(l);

			as = new float[size];
			for (boost::python::ssize_t i = 0; i < size; ++i)
				as[i] = extract<float>(l[i]);
		} else {
			const string objType = extract<string>((alphas.attr("__class__")).attr("__name__"));
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
		const boost::python::object &p, const boost::python::object &vi,
		const boost::python::object &n, const boost::python::object &uv,
		const boost::python::object &cols, const boost::python::object &alphas) {
	Scene_DefineMesh1(scene, meshName, p, vi, n, uv, cols, alphas, boost::python::object());
}

static void Scene_DefineMeshExt1(luxcore::detail::SceneImpl *scene, const string &meshName,
		const boost::python::object &p, const boost::python::object &vi,
		const boost::python::object &n, const boost::python::object &uv,
		const boost::python::object &cols, const boost::python::object &alphas,
		const boost::python::object &transformation) {
	// NOTE: I would like to use boost::scoped_array but
	// some guy has decided that boost::scoped_array must not have
	// a release() method for some ideological reason...

	// Translate all vertices
	long plyNbVerts;
	luxrays::Point *points = NULL;
	extract<boost::python::list> getPList(p);
	if (getPList.check()) {
		const boost::python::list &l = getPList();
		const boost::python::ssize_t size = len(l);
		plyNbVerts = size;

		points = (luxrays::Point *)luxcore::detail::SceneImpl::AllocVerticesBuffer(size);
		for (boost::python::ssize_t i = 0; i < size; ++i) {
			extract<boost::python::tuple> getTuple(l[i]);
			if (getTuple.check()) {
				const boost::python::tuple &t = getTuple();
				points[i] = luxrays::Point(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
			} else {
				const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
				throw runtime_error("Wrong data type in the list of vertices of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
			}
		}
	} else {
		const string objType = extract<string>((p.attr("__class__")).attr("__name__"));
		throw runtime_error("Wrong data type for the list of vertices of method Scene.DefineMeshExt(): " + objType);
	}

	// Translate all triangles
	long plyNbTris;
	luxrays::Triangle *tris = NULL;
	extract<boost::python::list> getVIList(vi);
	if (getVIList.check()) {
		const boost::python::list &l = getVIList();
		const boost::python::ssize_t size = len(l);
		plyNbTris = size;

		tris = (luxrays::Triangle *)luxcore::detail::SceneImpl::AllocTrianglesBuffer(size);
		for (boost::python::ssize_t i = 0; i < size; ++i) {
			extract<boost::python::tuple> getTuple(l[i]);
			if (getTuple.check()) {
				const boost::python::tuple &t = getTuple();
				tris[i] = luxrays::Triangle(extract<u_int>(t[0]), extract<u_int>(t[1]), extract<u_int>(t[2]));
			} else {
				const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
				throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
			}
		}
	} else {
		const string objType = extract<string>((vi.attr("__class__")).attr("__name__"));
		throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
	}

	// Translate all normals
	luxrays::Normal *normals = NULL;
	if (!n.is_none()) {
		extract<boost::python::list> getNList(n);
		if (getNList.check()) {
			const boost::python::list &l = getNList();
			const boost::python::ssize_t size = len(l);

			normals = new luxrays::Normal[size];
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					normals[i] = luxrays::Normal(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of triangles of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((n.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of triangles of method Scene.DefineMeshExt(): " + objType);
		}
	}

	// Translate all UVs
	array<luxrays::UV *, LC_MESH_MAX_DATA_COUNT> uvs;
	fill(uvs.begin(), uvs.end(), nullptr);
	if (!uv.is_none()) {
		extract<boost::python::list> getUVsArray(uv);

		if (getUVsArray.check()) {
			const boost::python::list &uvsArray = getUVsArray();
			const boost::python::ssize_t uvsArraySize = luxrays::Min<boost::python::ssize_t>(len(uvsArray), LC_MESH_MAX_DATA_COUNT);

			for (boost::python::ssize_t j= 0; j < uvsArraySize; ++j) {
				extract<boost::python::list> getUVList(uv);

				if (getUVList.check()) {
					const boost::python::list &l = getUVList();
					
					if (!l.is_none()) {
						const boost::python::ssize_t size = len(l);

						uvs[j] = new luxrays::UV[size];
						for (boost::python::ssize_t i = 0; i < size; ++i) {
							extract<boost::python::tuple> getTuple(l[i]);
							if (getTuple.check()) {
								const boost::python::tuple &t = getTuple();
								uvs[j][i] = luxrays::UV(extract<float>(t[0]), extract<float>(t[1]));
							} else {
								const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
								throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
							}
						}
					}
				} else {
					const string objType = extract<string>((uv.attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMeshExt(): " + objType);
				}
			}
		} else {
			const string objType = extract<string>((uv.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of UVs of method Scene.DefineMeshExt(): " + objType);
		}
	}

	// Translate all colors
	array<luxrays::Spectrum *, LC_MESH_MAX_DATA_COUNT> colors;
	fill(colors.begin(), colors.end(), nullptr);
	if (!cols.is_none()) {
		extract<boost::python::list> getColorsArray(cols);

		if (getColorsArray.check()) {
			const boost::python::list &colorsArray = getColorsArray();
			const boost::python::ssize_t colorsArraySize = luxrays::Min<boost::python::ssize_t>(len(colorsArray), LC_MESH_MAX_DATA_COUNT);

			for (boost::python::ssize_t j= 0; j < colorsArraySize; ++j) {
				extract<boost::python::list> getColorsList(cols);

				if (getColorsList.check()) {
					const boost::python::list &l = getColorsList();
					
					if (!l.is_none()) {
						const boost::python::ssize_t size = len(l);

						colors[j] = new luxrays::Spectrum[size];
						for (boost::python::ssize_t i = 0; i < size; ++i) {
							extract<boost::python::tuple> getTuple(l[i]);
							if (getTuple.check()) {
								const boost::python::tuple &t = getTuple();
								colors[j][i] = luxrays::Spectrum(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2]));
							} else {
								const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
								throw runtime_error("Wrong data type in the list of colors of method Scene.DefineMeshExt() at position " + luxrays::ToString(i) +": " + objType);
							}
						}
					}
				} else {
					const string objType = extract<string>((cols.attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMeshExt(): " + objType);
				}
			}
		} else {
			const string objType = extract<string>((cols.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of colors of method Scene.DefineMeshExt(): " + objType);
		}
	}

	// Translate all alphas
	array<float *, LC_MESH_MAX_DATA_COUNT> as;
	fill(as.begin(), as.end(), nullptr);
	if (!alphas.is_none()) {
		extract<boost::python::list> getColorsArray(alphas);

		if (getColorsArray.check()) {
			const boost::python::list &asArray = getColorsArray();
			const boost::python::ssize_t asArraySize = luxrays::Min<boost::python::ssize_t>(len(asArray), LC_MESH_MAX_DATA_COUNT);

			for (boost::python::ssize_t j= 0; j < asArraySize; ++j) {
				extract<boost::python::list> getAlphasList(alphas);

				if (getAlphasList.check()) {
					const boost::python::list &l = getAlphasList();
					
					if (!l.is_none()) {
						const boost::python::ssize_t size = len(l);

						as[j] = new float[size];
						for (boost::python::ssize_t i = 0; i < size; ++i)
							as[j][i] = extract<float>(l[i]);
					}
				} else {
					const string objType = extract<string>((alphas.attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type for the list of alphas of method Scene.DefineMeshExt(): " + objType);
				}
			}
		} else {
			const string objType = extract<string>((alphas.attr("__class__")).attr("__name__"));
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
		const boost::python::object &p, const boost::python::object &vi,
		const boost::python::object &n, const boost::python::object &uv,
		const boost::python::object &cols, const boost::python::object &alphas) {
	Scene_DefineMeshExt1(scene, meshName, p, vi, n, uv, cols, alphas, boost::python::object());
}

static void Scene_SetMeshVertexAOV(luxcore::detail::SceneImpl *scene, const string &meshName,
		const u_int index, const boost::python::object &data) {
	vector<float> v;
	GetArray<float>(data, v);
	
	float *vcpy = new float[v.size()];
	copy(v.begin(), v.end(), vcpy);

	scene->SetMeshVertexAOV(meshName, index, vcpy);
}

static void Scene_SetMeshTriangleAOV(luxcore::detail::SceneImpl *scene, const string &meshName,
		const u_int index, const boost::python::object &data) {
	vector<float> t;
	GetArray<float>(data, t);
	
	float *tcpy = new float[t.size()];
	copy(t.begin(), t.end(), tcpy);

	scene->SetMeshTriangleAOV(meshName, index, tcpy);
}

static void Scene_SetMeshAppliedTransformation(luxcore::detail::SceneImpl *scene,
		const string &meshName,
		const boost::python::object &transformation) {
	float mat[16];
	GetMatrix4x4(transformation, mat);
	scene->SetMeshAppliedTransformation(meshName, mat);
}

static void Scene_DefineStrands(luxcore::detail::SceneImpl *scene, const string &shapeName,
		const int strandsCount, const int pointsCount,
		const boost::python::object &points,
		const boost::python::object &segments,
		const boost::python::object &thickness,
		const boost::python::object &transparency,
		const boost::python::object &colors,
		const boost::python::object &uvs,
		const string &tessellationTypeStr,
		const u_int adaptiveMaxDepth, const float adaptiveError,
		const u_int solidSideCount, const bool solidCapBottom, const bool solidCapTop,
		const bool useCameraPosition) {
	// Initialize the cyHairFile object
	luxrays::cyHairFile strands;
	strands.SetHairCount(strandsCount);
	strands.SetPointCount(pointsCount);

	// Set defaults if available
	int flags = CY_HAIR_FILE_POINTS_BIT;

	boost::python::extract<int> defaultSegmentValue(segments);
	if (defaultSegmentValue.check())
		strands.SetDefaultSegmentCount(defaultSegmentValue());
	else
		flags |= CY_HAIR_FILE_SEGMENTS_BIT;

	boost::python::extract<float> defaultThicknessValue(thickness);
	if (defaultThicknessValue.check())
		strands.SetDefaultThickness(defaultThicknessValue());
	else
		flags |= CY_HAIR_FILE_THICKNESS_BIT;

	boost::python::extract<float> defaultTransparencyValue(transparency);
	if (defaultTransparencyValue.check())
		strands.SetDefaultTransparency(defaultTransparencyValue());
	else
		flags |= CY_HAIR_FILE_TRANSPARENCY_BIT;

	boost::python::extract<boost::python::tuple> defaultColorsValue(colors);
	if (defaultColorsValue.check()) {
		const boost::python::tuple &t = defaultColorsValue();

		strands.SetDefaultColor(
			extract<float>(t[0]),
			extract<float>(t[1]),
			extract<float>(t[2]));
	} else
		flags |= CY_HAIR_FILE_COLORS_BIT;

	if (!uvs.is_none())
		flags |= CY_HAIR_FILE_UVS_BIT;

	strands.SetArrays(flags);

	// Translate all segments
	if (!defaultSegmentValue.check()) {
		extract<boost::python::list> getList(segments);

		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			u_short *s = strands.GetSegmentsArray();
			for (boost::python::ssize_t i = 0; i < size; ++i)
				s[i] = extract<u_short>(l[i]);
		} else {
			const string objType = extract<string>((segments.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of segments of method Scene.DefineStrands(): " + objType);
		}
	}

	// Translate all points
	if (!points.is_none()) {
		extract<boost::python::list> getList(points);
		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			float *p = strands.GetPointsArray();
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					p[i * 3] = extract<float>(t[0]);
					p[i * 3 + 1] = extract<float>(t[1]);
					p[i * 3 + 2] = extract<float>(t[2]);
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of points of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((points.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of points of method Scene.DefineStrands(): " + objType);
		}
	} else
		throw runtime_error("Points list can not be None in method Scene.DefineStrands()");

	// Translate all thickness
	if (!defaultThicknessValue.check()) {
		extract<boost::python::list> getList(thickness);
		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			float *t = strands.GetThicknessArray();
			for (boost::python::ssize_t i = 0; i < size; ++i)
				t[i] = extract<float>(l[i]);
		} else {
			const string objType = extract<string>((thickness.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of thickness of method Scene.DefineStrands(): " + objType);
		}
	}

	// Translate all transparency
	if (!defaultTransparencyValue.check()) {
		extract<boost::python::list> getList(transparency);
		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			float *t = strands.GetTransparencyArray();
			for (boost::python::ssize_t i = 0; i < size; ++i)
				t[i] = extract<float>(l[i]);
		} else {
			const string objType = extract<string>((transparency.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of transparency of method Scene.DefineStrands(): " + objType);
		}
	}

	// Translate all colors
	if (!defaultColorsValue.check()) {
		extract<boost::python::list> getList(colors);
		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			float *c = strands.GetColorsArray();
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					c[i * 3] = extract<float>(t[0]);
					c[i * 3 + 1] = extract<float>(t[1]);
					c[i * 3 + 2] = extract<float>(t[2]);
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of colors of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((colors.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of colors of method Scene.DefineStrands(): " + objType);
		}
	}

	// Translate all UVs
	if (!uvs.is_none()) {
		extract<boost::python::list> getList(uvs);
		if (getList.check()) {
			const boost::python::list &l = getList();
			const boost::python::ssize_t size = len(l);

			float *uv = strands.GetUVsArray();
			for (boost::python::ssize_t i = 0; i < size; ++i) {
				extract<boost::python::tuple> getTuple(l[i]);
				if (getTuple.check()) {
					const boost::python::tuple &t = getTuple();
					uv[i * 2] = extract<float>(t[0]);
					uv[i * 2 + 1] = extract<float>(t[1]);
				} else {
					const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));
					throw runtime_error("Wrong data type in the list of UVs of method Scene.DefineStrands() at position " + luxrays::ToString(i) +": " + objType);
				}
			}
		} else {
			const string objType = extract<string>((uvs.attr("__class__")).attr("__name__"));
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
		const boost::python::object &transformation,
		const u_int objectID) {
	float mat[16];
	GetMatrix4x4(transformation, mat);

	scene->DuplicateObject(srcObjName, dstObjName, mat, objectID);
}

static void Scene_DuplicateObjectMulti(luxcore::detail::SceneImpl *scene,
		const string &srcObjName,
		const string &dstObjNamePrefix,
		const unsigned int count,
		const boost::python::object &transformations,
		const boost::python::object &objectIDs) {
	if (!PyObject_CheckBuffer(objectIDs.ptr())){
		const string objType = extract<string>((objectIDs.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
	}
	if (!PyObject_CheckBuffer(transformations.ptr())) {
		const string objType = extract<string>((transformations.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
	}

	Py_buffer transformationsView;
	if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
		const string objType = extract<string>((transformations.attr("__class__")).attr("__name__"));
		throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
	}
	Py_buffer objectIDsView;
	if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
		PyBuffer_Release(&transformationsView);

		const string objType = extract<string>((transformations.attr("__class__")).attr("__name__"));
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
		const u_int steps,
		const boost::python::object &times,
		const boost::python::object &transformations,
		const u_int objectID) {
	if (!times.is_none() && !transformations.is_none()) {
		extract<boost::python::list> timesListExtract(times);
		extract<boost::python::list> transformationsListExtract(transformations);

		if (timesListExtract.check() && transformationsListExtract.check()) {
			const boost::python::list &timesList = timesListExtract();
			const boost::python::list &transformationsList = transformationsListExtract();

			if ((len(timesList) != steps) || (len(transformationsList) != steps))
				throw runtime_error("Wrong number of elements for the times and/or the list of transformations of method Scene.DuplicateObject()");

			vector<float> timesVec(steps);
			vector<float> transVec(steps * 16);
			u_int transIndex = 0;

			for (u_int i = 0; i < steps; ++i) {
				timesVec[i] = extract<float>(timesList[i]);

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
		const u_int steps,
		const boost::python::object &times,
		const boost::python::object &transformations,
		const boost::python::object &objectIDs) {

		if (!PyObject_CheckBuffer(times.ptr())){
			const string objType = extract<string>((times.attr("__class__")).attr("__name__"));
			throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
		}
		if (!PyObject_CheckBuffer(transformations.ptr())){
			const string objType = extract<string>((transformations.attr("__class__")).attr("__name__"));
			throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
		}
		if (!PyObject_CheckBuffer(objectIDs.ptr())){
			const string objType = extract<string>((objectIDs.attr("__class__")).attr("__name__"));
			throw runtime_error("Unsupported data type in Scene.DuplicateObject() method: " + objType);
		}

		Py_buffer timesView;
		if (PyObject_GetBuffer(times.ptr(), &timesView, PyBUF_SIMPLE)) {
			const string objType = extract<string>((times.attr("__class__")).attr("__name__"));
			throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
		}
		Py_buffer transformationsView;
		if (PyObject_GetBuffer(transformations.ptr(), &transformationsView, PyBUF_SIMPLE)) {
			PyBuffer_Release(&timesView);

			const string objType = extract<string>((transformations.attr("__class__")).attr("__name__"));
			throw runtime_error("Unable to get a data view in Scene.DuplicateObject() method: " + objType);
		}
		Py_buffer objectIDsView;
		if (PyObject_GetBuffer(objectIDs.ptr(), &objectIDsView, PyBUF_SIMPLE)) {
			PyBuffer_Release(&timesView);
			PyBuffer_Release(&transformationsView);

			const string objType = extract<string>((objectIDs.attr("__class__")).attr("__name__"));
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

static void Scene_UpdateObjectTransformation(luxcore::detail::SceneImpl *scene,
		const string &objName,
		const boost::python::object &transformation) {
	float mat[16];
	GetMatrix4x4(transformation, mat);
	scene->UpdateObjectTransformation(objName, mat);
}

//------------------------------------------------------------------------------
// Glue for RenderConfig class
//------------------------------------------------------------------------------

static boost::python::tuple RenderConfig_LoadResumeFile(const str &fileNameStr) {
	const string fileName = extract<string>(fileNameStr);
	luxcore::detail::RenderStateImpl *startState;
	luxcore::detail::FilmImpl *startFilm;
	luxcore::detail::RenderConfigImpl *config = new luxcore::detail::RenderConfigImpl(fileName, &startState, &startFilm);

	return boost::python::make_tuple(TransferToPython(config), TransferToPython(startState), TransferToPython(startFilm));
}

static luxcore::detail::RenderConfigImpl *RenderConfig_LoadFile(const str &fileNameStr) {
	const string fileName = extract<string>(fileNameStr);
	luxcore::detail::RenderConfigImpl *config = new luxcore::detail::RenderConfigImpl(fileName);

	return config;
}

static luxcore::detail::SceneImpl &RenderConfig_GetScene(luxcore::detail::RenderConfigImpl *renderConfig) {
	return (luxcore::detail::SceneImpl &)renderConfig->GetScene();
}

static boost::python::tuple RenderConfig_GetFilmSize(luxcore::detail::RenderConfigImpl *renderConfig) {
	u_int filmWidth, filmHeight, filmSubRegion[4];
	const bool result = renderConfig->GetFilmSize(&filmWidth, &filmHeight, filmSubRegion);

	return boost::python::make_tuple(filmWidth, filmHeight,
			boost::python::make_tuple(filmSubRegion[0], filmSubRegion[1], filmSubRegion[2], filmSubRegion[3]),
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

BOOST_PYTHON_MODULE(pyluxcore) {
	// I get a crash on Ubuntu 19.10 without this line and this should be
	// good anyway to avoid problems with "," Vs. "." decimal separator, etc.

	try {
		locale::global(locale("C.UTF-8"));
	} catch (runtime_error &) {
		// "C.UTF-8" locale may not exist on some system so I ignore the error
	}

	np::initialize();

	docstring_options doc_options(
		true,	// Show user defined docstrings
		true,	// Show python signatures
		false	// Show C++ signatures
	);

	// This 'module' is actually a fake package
	object package = scope();
	package.attr("__path__") = "pyluxcore";
	package.attr("__package__") = "pyluxcore";
	package.attr("__doc__") = "LuxCoreRender Python bindings\n\n"
			"Provides access to the LuxCoreRender API in python\n\n";

	def("Version", LuxCoreVersion, "Returns the LuxCore version");

	def("Init", &LuxCore_Init);
	def("Init", &LuxCore_InitDefaultHandler);
	def("SetLogHandler", &LuxCore_SetLogHandler);
	def("ParseLXS", &ParseLXS);

	def("GetPlatformDesc", &GetPlatformDesc);
	def("GetOpenCLDeviceDescs", &GetOpenCLDeviceDescs);

	// Deprecated, use GetOpenCLDeviceDescs instead
	def("GetOpenCLDeviceList", &GetOpenCLDeviceList);
	
	def("ClearFileNameResolverPaths", &ClearFileNameResolverPaths);
	def("AddFileNameResolverPath", &AddFileNameResolverPath);
	def("GetFileNameResolverPaths", &GetFileNameResolverPaths);
	
	def("KernelCacheFill", &LuxCore_KernelCacheFill1);
	def("KernelCacheFill", &LuxCore_KernelCacheFill2);
	
	//--------------------------------------------------------------------------
	// Property class
	//--------------------------------------------------------------------------

    class_<luxrays::Property>("Property", init<string>())
		.def(init<string, bool>())
		.def(init<string, double>())
		.def(init<string, long long>())
		.def(init<string, string>())
		.def("__init__", make_constructor(Property_InitWithList))

		.def("GetName", &luxrays::Property::GetName, return_value_policy<copy_const_reference>())
		.def("GetSize", &luxrays::Property::GetSize)
		.def("Clear", &luxrays::Property::Clear, return_internal_reference<>())

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

		.def("Add", &Property_Add, return_internal_reference<>())
		.def("AddAllBool", &Property_AddAllBool, return_internal_reference<>())
		.def("AddAllInt", &Property_AddAllInt, return_internal_reference<>())
		.def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLong, return_internal_reference<>())
		.def("AddAllFloat", &Property_AddAllFloat, return_internal_reference<>())
		.def("AddAllBool", &Property_AddAllBoolStride, return_internal_reference<>())
		.def("AddAllInt", &Property_AddAllIntStride, return_internal_reference<>())
		.def("AddUnsignedLongLong", &Property_AddAllUnsignedLongLongStride, return_internal_reference<>())
		.def("AddAllFloat", &Property_AddAllFloatStride, return_internal_reference<>())
		.def<luxrays::Property &(*)(luxrays::Property *, const boost::python::list &)>
			("Set", &Property_Set, return_internal_reference<>())
		.def<luxrays::Property &(*)(luxrays::Property *, const u_int, const boost::python::object &)>
			("Set", &Property_Set, return_internal_reference<>())

		.def(self_ns::str(self))
    ;

	//--------------------------------------------------------------------------
	// Properties class
	//--------------------------------------------------------------------------

    class_<luxrays::Properties>("Properties", init<>())
		.def(init<string>())
		.def(init<luxrays::Properties>())

		// Required because Properties::Set is overloaded
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Property &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &, const string &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def("SetFromFile", &luxrays::Properties::SetFromFile, return_internal_reference<>())
		.def("SetFromString", &luxrays::Properties::SetFromString, return_internal_reference<>())

		.def("Clear", &luxrays::Properties::Clear, return_internal_reference<>())
		.def("GetAllNamesRE", &Properties_GetAllNamesRE)
		.def("GetAllNames", &Properties_GetAllNames1)
		.def("GetAllNames", &Properties_GetAllNames2)
		.def("GetAllUniqueSubNames", &Properties_GetAllUniqueSubNames)
		.def("HaveNames", &luxrays::Properties::HaveNames)
		.def("HaveNamesRE", &luxrays::Properties::HaveNamesRE)
		.def("GetAllProperties", &luxrays::Properties::GetAllProperties)

		.def<const luxrays::Property &(luxrays::Properties::*)(const string &) const>
			("Get", &luxrays::Properties::Get, return_internal_reference<>())
		.def("Get", &Properties_GetWithDefaultValues)

		.def("GetSize", &luxrays::Properties::GetSize)

		.def("IsDefined", &luxrays::Properties::IsDefined)
		.def("Delete", &luxrays::Properties::Delete)
		.def("DeleteAll", &Properties_DeleteAll)
		.def("ToString", &luxrays::Properties::ToString)

		.def(self_ns::str(self))
    ;

	//--------------------------------------------------------------------------
	// Film class
	//--------------------------------------------------------------------------

	enum_<Film::FilmOutputType>("FilmOutputType")
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
	;

    class_<luxcore::detail::FilmImpl>("Film", init<string>())
		.def(init<luxrays::Properties, bool, bool>())
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

    class_<luxcore::detail::CameraImpl>("Camera", no_init)
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

    class_<luxcore::detail::SceneImpl>("Scene", init<>())
		.def(init<luxrays::Properties>())
		.def(init<string>())
		.def("ToProperties", &luxcore::detail::SceneImpl::ToProperties, return_internal_reference<>())
		.def("GetCamera", &Scene_GetCamera, return_internal_reference<>())
		.def("GetLightCount", &luxcore::detail::SceneImpl::GetLightCount)
		.def("GetObjectCount", &luxcore::detail::SceneImpl::GetObjectCount)
		.def("DefineImageMap", &Scene_DefineImageMap)
		.def("IsImageMapDefined", &luxcore::detail::SceneImpl::IsImageMapDefined)
		.def("DefineMesh", &Scene_DefineMesh1)
		.def("DefineMesh", &Scene_DefineMesh2)
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
		.def("IsMeshDefined", &luxcore::detail::SceneImpl::IsMeshDefined)
		.def("IsTextureDefined", &luxcore::detail::SceneImpl::IsTextureDefined)
		.def("IsMaterialDefined", &luxcore::detail::SceneImpl::IsMaterialDefined)
		.def("Parse", &luxcore::detail::SceneImpl::Parse)
		.def("DuplicateObject", &Scene_DuplicateObject)
		.def("DuplicateObject", &Scene_DuplicateObjectMulti)
		.def("DuplicateObject", &Scene_DuplicateMotionObject)
		.def("DuplicateObject", &Scene_DuplicateMotionObjectMulti)
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

    class_<luxcore::detail::RenderConfigImpl>("RenderConfig", init<luxrays::Properties>())
		.def(init<luxrays::Properties, luxcore::detail::SceneImpl *>()[with_custodian_and_ward<1, 3>()])
		.def("__init__", make_constructor(RenderConfig_LoadFile))
		.def("GetProperties", &luxcore::detail::RenderConfigImpl::GetProperties, return_internal_reference<>())
		.def("GetProperty", &luxcore::detail::RenderConfigImpl::GetProperty)
		.def("GetScene", &RenderConfig_GetScene, return_internal_reference<>())
		.def("HasCachedKernels", &luxcore::detail::RenderConfigImpl::HasCachedKernels)
		.def("Parse", &luxcore::detail::RenderConfigImpl::Parse)
		.def("Delete", &luxcore::detail::RenderConfigImpl::Delete)
		.def("GetFilmSize", &RenderConfig_GetFilmSize)
		.def("Save", &luxcore::detail::RenderConfigImpl::Save)
		.def("Export", &luxcore::detail::RenderConfigImpl::Export)
		.def("LoadResumeFile", &RenderConfig_LoadResumeFile).staticmethod("LoadResumeFile")
		.def("GetDefaultProperties", &luxcore::detail::RenderConfigImpl::GetDefaultProperties, return_internal_reference<>()).staticmethod("GetDefaultProperties")
    ;

	//--------------------------------------------------------------------------
	// RenderState class
	//--------------------------------------------------------------------------

	class_<luxcore::detail::RenderStateImpl>("RenderState", no_init)
		.def("Save", &RenderState::Save)
    ;

	//--------------------------------------------------------------------------
	// RenderSession class
	//--------------------------------------------------------------------------

    class_<luxcore::detail::RenderSessionImpl>("RenderSession", init<luxcore::detail::RenderConfigImpl *>()[with_custodian_and_ward<1, 2>()])
		.def(init<luxcore::detail::RenderConfigImpl *, string, string>()[with_custodian_and_ward<1, 2>()])
		.def(init<luxcore::detail::RenderConfigImpl *, luxcore::detail::RenderStateImpl *, luxcore::detail::FilmImpl *>()[with_custodian_and_ward<1, 2>()])
		.def("GetRenderConfig", &RenderSession_GetRenderConfig, return_internal_reference<>())
		.def("Start", &luxcore::detail::RenderSessionImpl::Start)
		.def("Stop", &luxcore::detail::RenderSessionImpl::Stop)
		.def("IsStarted", &luxcore::detail::RenderSessionImpl::IsStarted)
		.def("BeginSceneEdit", &luxcore::detail::RenderSessionImpl::BeginSceneEdit)
		.def("EndSceneEdit", &luxcore::detail::RenderSessionImpl::EndSceneEdit)
		.def("IsInSceneEdit", &luxcore::detail::RenderSessionImpl::IsInSceneEdit)
		.def("Pause", &luxcore::detail::RenderSessionImpl::Pause)
		.def("Resume", &luxcore::detail::RenderSessionImpl::Resume)
		.def("IsInPause", &luxcore::detail::RenderSessionImpl::IsInPause)
		.def("GetFilm", &RenderSession_GetFilm, return_internal_reference<>())
		.def("UpdateStats", &luxcore::detail::RenderSessionImpl::UpdateStats)
		.def("GetStats", &luxcore::detail::RenderSessionImpl::GetStats, return_internal_reference<>())
		.def("WaitNewFrame", &luxcore::detail::RenderSessionImpl::WaitNewFrame)
		.def("WaitForDone", &luxcore::detail::RenderSessionImpl::WaitForDone)
		.def("HasDone", &luxcore::detail::RenderSessionImpl::HasDone)
		.def("Parse", &luxcore::detail::RenderSessionImpl::Parse)
		.def("GetRenderState", &RenderSession_GetRenderState, return_value_policy<manage_new_object>())
		.def("SaveResumeFile", &luxcore::detail::RenderSessionImpl::SaveResumeFile)
    ;

	//--------------------------------------------------------------------------
	// Blender related functions
	//--------------------------------------------------------------------------

	def("ConvertFilmChannelOutput_1xFloat_To_1xFloatList", &blender::ConvertFilmChannelOutput_1xFloat_To_1xFloatList);
	def("ConvertFilmChannelOutput_UV_to_Blender_UV", &blender::ConvertFilmChannelOutput_UV_to_Blender_UV);
	def("ConvertFilmChannelOutput_1xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_1xFloat_To_4xFloatList);
	def("ConvertFilmChannelOutput_3xFloat_To_3xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_3xFloatList);
	def("ConvertFilmChannelOutput_3xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_4xFloatList);
	def("ConvertFilmChannelOutput_4xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_4xFloat_To_4xFloatList);
	def("ConvertFilmChannelOutput_1xUInt_To_1xFloatList", &blender::ConvertFilmChannelOutput_1xUInt_To_1xFloatList);
	def("BlenderMatrix4x4ToList", &blender::BlenderMatrix4x4ToList);
	def("GetOpenVDBGridNames", &blender::GetOpenVDBGridNames);
	def("GetOpenVDBGridInfo", &blender::GetOpenVDBGridInfo);

	// Note: used by pyluxcoredemo.py, do not remove.
	def("ConvertFilmChannelOutput_3xFloat_To_4xUChar", &blender::ConvertFilmChannelOutput_3xFloat_To_4xUChar);
}

}
