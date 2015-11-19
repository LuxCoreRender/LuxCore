/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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

#define BOOST_PYTHON_MAX_ARITY 18

#include <memory>

#include <boost/foreach.hpp>
#include <boost/python.hpp>
#include <boost/python/call.hpp>

#include <Python.h>

#include "luxrays/luxrays.h"
#include <luxrays/utils/cyhair/cyHairFile.h>
#include <luxcore/luxcore.h>
#include <luxcore/pyluxcore/pyluxcoreforblender.h>

using namespace std;
using namespace luxcore;
using namespace boost::python;

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

static const char *LuxCoreVersion() {
	static const char *luxCoreVersion = LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR;
	return luxCoreVersion;
}

static boost::python::list GetOpenCLDeviceList() {
	luxrays::Context ctx;
	vector<luxrays::DeviceDescription *> deviceDescriptions = ctx.GetAvailableDeviceDescriptions();

	// Select only OpenCL devices
	luxrays::DeviceDescription::Filter(luxrays::DEVICE_TYPE_OPENCL_ALL, deviceDescriptions);

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

//------------------------------------------------------------------------------
// Glue for Property class
//------------------------------------------------------------------------------

static luxrays::Property *Property_InitWithList(const str &name, const boost::python::list &l) {
	luxrays::Property *prop = new luxrays::Property(extract<string>(name));

	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));

		if (objType == "bool") {
			const bool v = extract<bool>(l[i]);
			prop->Add(v);
		} else if (objType == "int") {
			const int v = extract<int>(l[i]);
			prop->Add(v);
		} else if (objType == "float") {
			const double v = extract<double>(l[i]);
			prop->Add(v);
		} else if (objType == "str") {
			const string v = extract<string>(l[i]);
			prop->Add(v);
		} else
			throw runtime_error("Unsupported data type included in a Property constructor list: " + objType);
	}

	return prop;
}

static boost::python::list Property_Get(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i) {
		const std::type_info &tinfo = prop->GetValueType(i);

		if (tinfo == typeid(bool))
			l.append(prop->Get<bool>(i));
		else if (tinfo == typeid(int))
			l.append(prop->Get<int>(i));
		else if (tinfo == typeid(double))
			l.append(prop->Get<double>(i));
		else if (tinfo == typeid(string))
			l.append(prop->Get<string>(i));
		else
			throw runtime_error("Unsupported data type in list extraction of a Property: " + prop->GetName());
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
		l.append(prop->Get<int>(i));
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

static bool Property_GetBool(luxrays::Property *prop) {
	return prop->Get<bool>(0);
}

static int Property_GetInt(luxrays::Property *prop) {
	return prop->Get<int>(0);
}

static double Property_GetFloat(luxrays::Property *prop) {
	return prop->Get<double>(0);
}

static string Property_GetString(luxrays::Property *prop) {
	return prop->Get<string>(0);
}

static luxrays::Property &Property_Add(luxrays::Property *prop, const boost::python::list &l) {
	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));

		if (objType == "bool") {
			const bool v = extract<bool>(l[i]);
			prop->Add(v);
		} else if (objType == "int") {
			const int v = extract<int>(l[i]);
			prop->Add(v);
		} else if (objType == "float") {
			const double v = extract<double>(l[i]);
			prop->Add(v);
		} else if (objType == "str") {
			const string v = extract<string>(l[i]);
			prop->Add(v);
		} else
			throw runtime_error("Unsupported data type included in Property.Set() method list: " + objType);
	}

	return *prop;
}

static luxrays::Property &Property_Set(luxrays::Property *prop, const boost::python::list &l) {
	const boost::python::ssize_t size = len(l);
	for (boost::python::ssize_t i = 0; i < size; ++i) {
		const string objType = extract<string>((l[i].attr("__class__")).attr("__name__"));

		if (objType == "bool") {
			const bool v = extract<bool>(l[i]);
			prop->Set(i, v);
		} else if (objType == "int") {
			const int v = extract<int>(l[i]);
			prop->Set(i, v);
		} else if (objType == "float") {
			const double v = extract<double>(l[i]);
			prop->Set(i, v);
		} else if (objType == "str") {
			const string v = extract<string>(l[i]);
			prop->Set(i, v);
		} else
			throw runtime_error("Unsupported data type included in Property.Set() method list: " + objType);
	}

	return *prop;
}

static luxrays::Property &Property_Set(luxrays::Property *prop, const u_int i,
		const boost::python::object &obj) {
	const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));

	if (objType == "bool") {
		const bool v = extract<bool>(obj);
		prop->Set(i, v);
	} else if (objType == "int") {
		const int v = extract<int>(obj);
		prop->Set(i, v);
	} else if (objType == "float") {
		const double v = extract<double>(obj);
		prop->Set(i, v);
	} else if (objType == "str") {
		const string v = extract<string>(obj);
		prop->Set(i, v);
	} else
		throw runtime_error("Unsupported data type used for Property.Set() method: " + objType);

	return *prop;
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
			const int v = extract<int>(l[i]);
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

	return luxrays::Property(name, values);
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

static void Film_GetOutputFloat1(Film *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	if (PyObject_CheckBuffer(obj.ptr())) {
		Py_buffer view;
		if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
			if ((size_t)view.len >= film->GetOutputSize(type) * sizeof(float)) {
				float *buffer = (float *)view.buf;

				film->GetOutput<float>(type, buffer, index);
				
				PyBuffer_Release(&view);
			} else {
				const string errorMsg = "Not enough space in the buffer of Film.GetOutputFloat() method: " +
						luxrays::ToString(view.len) + " instead of " + luxrays::ToString(film->GetOutputSize(type) * sizeof(float));
				PyBuffer_Release(&view);

				throw runtime_error(errorMsg);
			}
		} else {
			const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
			throw runtime_error("Unable to get a data view in Film.GetOutputFloat() method: " + objType);
		}
	} else {
		const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
		throw runtime_error("Unsupported data type in Film.GetOutputFloat() method: " + objType);
	}
}

static void Film_GetOutputFloat2(Film *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_GetOutputFloat1(film, type, obj, 0);
}

static void Film_GetOutputUInt1(Film *film, const Film::FilmOutputType type,
		boost::python::object &obj, const u_int index) {
	if (PyObject_CheckBuffer(obj.ptr())) {
		Py_buffer view;
		if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
			if ((size_t)view.len >= film->GetOutputSize(type) * sizeof(u_int)) {
				u_int *buffer = (u_int *)view.buf;

				film->GetOutput<u_int>(type, buffer, index);
				
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

static void Film_GetOutputUInt2(Film *film, const Film::FilmOutputType type,
		boost::python::object &obj) {
	Film_GetOutputUInt1(film, type, obj, 0);
}

//------------------------------------------------------------------------------
// Glue for Camera class
//------------------------------------------------------------------------------

static void Camera_Translate(Camera *Camera, const boost::python::tuple t) {
	Camera->Translate(luxrays::Vector(extract<float>(t[0]), extract<float>(t[1]), extract<float>(t[2])));
}

static void Camera_Rotate(Camera *Camera, const float angle, const boost::python::tuple axis) {
	Camera->Rotate(angle, luxrays::Vector(extract<float>(axis[0]), extract<float>(axis[1]), extract<float>(axis[2])));
}

//------------------------------------------------------------------------------
// Glue for Scene class
//------------------------------------------------------------------------------

static void Scene_DefineImageMap(Scene *scene, const string &imgMapName,
		boost::python::object &obj, const float gamma,
		const u_int channels, const u_int width, const u_int height) {
	if (PyObject_CheckBuffer(obj.ptr())) {
		Py_buffer view;
		if (!PyObject_GetBuffer(obj.ptr(), &view, PyBUF_SIMPLE)) {
			if ((size_t)view.len >= width * height * channels * sizeof(float)) {
				float *buffer = (float *)view.buf;
				scene->DefineImageMap(imgMapName, buffer, gamma, channels,
						width, height, Scene::DEFAULT);

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

static void Scene_DefineMesh1(Scene *scene, const string &meshName,
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

		points = Scene::AllocVerticesBuffer(size);
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

		tris = Scene::AllocTrianglesBuffer(size);
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

			mesh->ApplyTransform(luxrays::Transform(mat));
		} else {
			const string objType = extract<string>((alphas.attr("__class__")).attr("__name__"));
			throw runtime_error("Wrong data type for the list of transformation values of method Scene.DefineMesh(): " + objType);
		}
	}

	scene->DefineMesh(meshName, mesh);
}

static void Scene_DefineMesh2(Scene *scene, const string &meshName,
		const boost::python::object &p, const boost::python::object &vi,
		const boost::python::object &n, const boost::python::object &uv,
		const boost::python::object &cols, const boost::python::object &alphas) {
	Scene_DefineMesh1(scene, meshName, p, vi, n, uv, cols, alphas, boost::python::object());
}

static void Scene_DefineStrands(Scene *scene, const string &shapeName,
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

//------------------------------------------------------------------------------
// Glue for RenderConfig class
//------------------------------------------------------------------------------

static boost::python::tuple RenderConfig_GetFilmSize(RenderConfig *renderConfig) {
	u_int filmWidth, filmHeight, filmSubRegion[4];
	const bool result = renderConfig->GetFilmSize(&filmWidth, &filmHeight, filmSubRegion);

	return boost::python::make_tuple(filmWidth, filmHeight,
			boost::python::make_tuple(filmSubRegion[0], filmSubRegion[1], filmSubRegion[2], filmSubRegion[3]),
			result);
}

//------------------------------------------------------------------------------

BOOST_PYTHON_MODULE(pyluxcore) {
	docstring_options doc_options(
		true,	// Show user defined docstrings
		true,	// Show python signatures
		false	// Show C++ signatures
	);

	//This 'module' is actually a fake package
	object package = scope();
	package.attr("__path__") = "pyluxcore";
	package.attr("__package__") = "pyluxcore";
	package.attr("__doc__") = "New LuxRender Python bindings\n\n"
			"Provides access to the new LuxRender API in python\n\n";

	def("Version", LuxCoreVersion, "Returns the LuxCore version");

	def("Init", &LuxCore_Init);
	def("Init", &LuxCore_InitDefaultHandler);
	def("ParseLXS", &ParseLXS);

	def("GetOpenCLDeviceList", &GetOpenCLDeviceList);
	
	//--------------------------------------------------------------------------
	// Property class
	//--------------------------------------------------------------------------

    class_<luxrays::Property>("Property", init<string>())
		.def(init<string, bool>())
		.def(init<string, int>())
		.def(init<string, double>())
		.def(init<string, string>())
		.def("__init__", make_constructor(Property_InitWithList))

		.def("GetName", &luxrays::Property::GetName, return_value_policy<copy_const_reference>())
		.def("GetSize", &luxrays::Property::GetSize)
		.def("Clear", &luxrays::Property::Clear, return_internal_reference<>())

		.def("Get", &Property_Get)

		.def<bool (luxrays::Property::*)(const u_int) const>
			("GetBool", &luxrays::Property::Get)
		.def<int (luxrays::Property::*)(const u_int) const>
			("GetInt", &luxrays::Property::Get)
		.def<double (luxrays::Property::*)(const u_int) const>
			("GetFloat", &luxrays::Property::Get)
		.def<string (luxrays::Property::*)(const u_int) const>
			("GetString", &luxrays::Property::Get)

		.def("GetBool", &Property_GetBool)
		.def("GetInt", &Property_GetInt)
		.def("GetFloat", &Property_GetFloat)
		.def("GetString", &Property_GetString)

		.def("GetBools", &Property_GetBools)
		.def("GetInts", &Property_GetInts)
		.def("GetFloats", &Property_GetFloats)
		.def("GetStrings", &Property_GetStrings)
	
		.def("GetValuesString", &luxrays::Property::GetValuesString)
		.def("ToString", &luxrays::Property::ToString)

		.def("Add", &Property_Add, return_internal_reference<>())
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
		.value("RGB_TONEMAPPED", Film::OUTPUT_RGB_TONEMAPPED)
		.value("RGBA_TONEMAPPED", Film::OUTPUT_RGBA_TONEMAPPED)
		.value("ALPHA", Film::OUTPUT_ALPHA)
		.value("DEPTH", Film::OUTPUT_DEPTH)
		.value("POSITION", Film::OUTPUT_POSITION)
		.value("GEOMETRY_NORMAL", Film::OUTPUT_GEOMETRY_NORMAL)
		.value("SHADING_NORMAL", Film::OUTPUT_SHADING_NORMAL)
		.value("MATERIAL_ID", Film::OUTPUT_MATERIAL_ID)
		.value("DIRECT_DIFFUSE", Film::OUTPUT_DIRECT_DIFFUSE)
		.value("DIRECT_GLOSSY", Film::OUTPUT_DIRECT_GLOSSY)
		.value("EMISSION", Film::OUTPUT_EMISSION)
		.value("INDIRECT_DIFFUSE", Film::OUTPUT_INDIRECT_DIFFUSE)
		.value("INDIRECT_GLOSSY", Film::OUTPUT_INDIRECT_GLOSSY)
		.value("INDIRECT_SPECULAR", Film::OUTPUT_INDIRECT_SPECULAR)
		.value("MATERIAL_ID_MASK", Film::OUTPUT_MATERIAL_ID_MASK)
		.value("DIRECT_SHADOW_MASK", Film::OUTPUT_DIRECT_SHADOW_MASK)
		.value("INDIRECT_SHADOW_MASK", Film::OUTPUT_INDIRECT_SHADOW_MASK)
		.value("RADIANCE_GROUP", Film::OUTPUT_RADIANCE_GROUP)
		.value("UV", Film::OUTPUT_UV)
		.value("RAYCOUNT", Film::OUTPUT_RAYCOUNT)
		.value("BY_MATERIAL_ID", Film::OUTPUT_BY_MATERIAL_ID)
		.value("IRRADIANCE", Film::OUTPUT_IRRADIANCE)
	;

    class_<Film>("Film", init<string>())
		.def("GetWidth", &Film::GetWidth)
		.def("GetHeight", &Film::GetHeight)
		.def("Save", &Film::SaveOutputs) // Deprecated
		.def("SaveOutputs", &Film::SaveOutputs)
		.def("SaveOutput", &Film::SaveOutput)
		.def("SaveFilm", &Film::SaveFilm)
		.def("GetRadianceGroupCount", &Film::GetRadianceGroupCount)
		.def("GetOutputSize", &Film::GetOutputSize)
		.def("GetOutputFloat", &Film_GetOutputFloat1)
		.def("GetOutputFloat", &Film_GetOutputFloat2)
		.def("GetOutputUInt", &Film_GetOutputUInt1)
		.def("GetOutputUInt", &Film_GetOutputUInt2)
		.def("Parse", &Film::Parse)
    ;

	//--------------------------------------------------------------------------
	// Camera class
	//--------------------------------------------------------------------------

    class_<Camera>("Camera", no_init)
		.def("Translate", &Camera_Translate)
		.def("TranslateLeft", &Camera::TranslateLeft)
		.def("TranslateRight", &Camera::TranslateRight)
		.def("TranslateForward", &Camera::TranslateForward)
		.def("TranslateBackward", &Camera::TranslateBackward)
		.def("Rotate", &Camera_Rotate)
		.def("RotateLeft", &Camera::RotateLeft)
		.def("RotateRight", &Camera::RotateRight)
		.def("RotateUp", &Camera::RotateUp)
		.def("RotateDown", &Camera::RotateDown)
    ;

	//--------------------------------------------------------------------------
	// Scene class
	//--------------------------------------------------------------------------

    class_<Scene>("Scene", init<optional<float> >())
		.def(init<string, optional<float> >())
		.def("ToProperties", &Scene::ToProperties, return_internal_reference<>())
		.def("GetCamera", &Scene::GetCamera, return_internal_reference<>())
		.def("GetLightCount", &Scene::GetLightCount)
		.def("GetObjectCount", &Scene::GetObjectCount)
		.def("DefineImageMap", &Scene_DefineImageMap)
		.def("IsImageMapDefined", &Scene::IsImageMapDefined)
		.def("DefineMesh", &Scene_DefineMesh1)
		.def("DefineMesh", &Scene_DefineMesh2)
		.def("DefineBlenderMesh", &blender::Scene_DefineBlenderMesh1)
		.def("DefineBlenderMesh", &blender::Scene_DefineBlenderMesh2)
		.def("DefineStrands", &Scene_DefineStrands)
		.def("IsMeshDefined", &Scene::IsMeshDefined)
		.def("IsTextureDefined", &Scene::IsTextureDefined)
		.def("IsMaterialDefined", &Scene::IsMaterialDefined)
		.def("Parse", &Scene::Parse)
		.def("DeleteObject", &Scene::DeleteObject)
		.def("DeleteLight", &Scene::DeleteLight)
		.def("RemoveUnusedImageMaps", &Scene::RemoveUnusedImageMaps)
		.def("RemoveUnusedTextures", &Scene::RemoveUnusedTextures)
		.def("RemoveUnusedMaterials", &Scene::RemoveUnusedMaterials)
		.def("RemoveUnusedMeshes", &Scene::RemoveUnusedMeshes)
    ;

	//--------------------------------------------------------------------------
	// RenderConfig class
	//--------------------------------------------------------------------------

    class_<RenderConfig>("RenderConfig", init<luxrays::Properties>())
		.def(init<luxrays::Properties, Scene *>()[with_custodian_and_ward<1, 3>()])
		.def("GetProperties", &RenderConfig::GetProperties, return_internal_reference<>())
		.def("GetProperty", &RenderConfig::GetProperty)
		.def("GetScene", &RenderConfig::GetScene, return_internal_reference<>())
		.def("Parse", &RenderConfig::Parse)
		.def("Delete", &RenderConfig::Delete)
		.def("GetFilmSize", &RenderConfig_GetFilmSize)
		.def("GetDefaultProperties", &RenderConfig::GetDefaultProperties, return_internal_reference<>()).staticmethod("GetDefaultProperties")
    ;

	//--------------------------------------------------------------------------
	// RenderSession class
	//--------------------------------------------------------------------------

    class_<RenderSession>("RenderSession", init<RenderConfig *>()[with_custodian_and_ward<1, 2>()])
		.def("GetRenderConfig", &RenderSession::GetRenderConfig, return_internal_reference<>())
		.def("Start", &RenderSession::Start)
		.def("Stop", &RenderSession::Stop)
		.def("BeginSceneEdit", &RenderSession::BeginSceneEdit)
		.def("EndSceneEdit", &RenderSession::EndSceneEdit)
		.def("NeedPeriodicFilmSave", &RenderSession::NeedPeriodicFilmSave)
		.def("GetFilm", &RenderSession::GetFilm, return_internal_reference<>())
		.def("UpdateStats", &RenderSession::UpdateStats)
		.def("GetStats", &RenderSession::GetStats, return_internal_reference<>())
		.def("WaitNewFrame", &RenderSession::WaitNewFrame)
		.def("WaitForDone", &RenderSession::WaitForDone)
		.def("HasDone", &RenderSession::HasDone)
		.def("Parse", &RenderSession::Parse)
    ;

	//--------------------------------------------------------------------------
	// Blender related functions
	//--------------------------------------------------------------------------

	def("ConvertFilmChannelOutput_3xFloat_To_4xUChar", &blender::ConvertFilmChannelOutput_3xFloat_To_4xUChar);
	def("ConvertFilmChannelOutput_3xFloat_To_3xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_3xFloatList);
	def("ConvertFilmChannelOutput_1xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_1xFloat_To_4xFloatList);
	def("ConvertFilmChannelOutput_2xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_2xFloat_To_4xFloatList);
	def("ConvertFilmChannelOutput_3xFloat_To_4xFloatList", &blender::ConvertFilmChannelOutput_3xFloat_To_4xFloatList);
}

}
