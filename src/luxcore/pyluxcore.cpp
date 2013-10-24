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

#include <Python.h>

#include <boost/foreach.hpp>
#include <boost/python.hpp>
#include <boost/python/suite/indexing/vector_indexing_suite.hpp>

#include <luxcore/luxcore.h>

using namespace std;
using namespace luxcore;
using namespace boost::python;

namespace luxcore {

static const char *LuxCoreVersion() {
	static const char *luxCoreVersion = LUXCORE_VERSION_MAJOR "." LUXCORE_VERSION_MINOR;
	return luxCoreVersion;
}

//------------------------------------------------------------------------------
// Glue for Properties class
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
			throw std::runtime_error("Unsupported data type included in a Property constructor list: " + objType);
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
			throw std::runtime_error("Unsupported data type in list extraction of a Property: " + prop->GetName());
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
			throw std::runtime_error("Unsupported data type included in Property.Set() method list: " + objType);
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
			throw std::runtime_error("Unsupported data type included in Property.Set() method list: " + objType);
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
		throw std::runtime_error("Unsupported data type used for Property.Set() method: " + objType);

	return *prop;
}

//------------------------------------------------------------------------------

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
		const string &name, boost::python::list &l) {
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
			throw std::runtime_error("Unsupported data type included in Properties Get with default method: " + objType);
	}

	return luxrays::Property(name, values);
}

//------------------------------------------------------------------------------

static void RenderSession_GetScreenBuffer(RenderSession *renderSession,
		boost::python::object &obj) {
	if (PyByteArray_Check(obj.ptr())) {
		unsigned char *dst = (unsigned char *)PyByteArray_AsString(obj.ptr());
		const float *src = renderSession->GetScreenBuffer();

		const u_int width = renderSession->GetRenderConfig().GetProperties().Get("film.width").Get<u_int>();
		const u_int height = renderSession->GetRenderConfig().GetProperties().Get("film.height").Get<u_int>();

		for (u_int y = 0; y < height; ++y) {
			const u_int srcOffset = (height - y - 1) * width * 3;
			const u_int dstOffset = y * width * 3;

			for (u_int x = 0; x < width * 3; ++x)
				dst[dstOffset + x] = (unsigned char)floor((src[srcOffset + x] * 255.f + .5f));
		}
	} else {
		const string objType = extract<string>((obj.attr("__class__")).attr("__name__"));
		throw std::runtime_error("Unsupported data RenderSession.GetScreenBuffer() method: " + objType);
	}
}

//------------------------------------------------------------------------------

BOOST_PYTHON_MODULE(pyluxcore) {
	docstring_options doc_options(
		true,	// Show user defined docstrings
		true,	// Show python signatures
		false	// Show c++ signatures
	);

	//This 'module' is actually a fake package
	object package = scope();
	package.attr("__path__") = "pyluxcore";
	package.attr("__package__") = "pyluxcore";
	package.attr("__doc__") = "New LuxRender Python bindings\n\n"
			"Provides access to the new LuxRender API in python\n\n";

	def("version", LuxCoreVersion, "Returns the LuxCore version");

	def("Init", &Init);

//	class_<vector<unsigned char> >("UnsignedCharVector")
//        .def(vector_indexing_suite<vector<unsigned char> >());
//	class_<vector<float> >("FloatVector")
//        .def(vector_indexing_suite<vector<float> >());

	//--------------------------------------------------------------------------
	// Property class
	//--------------------------------------------------------------------------

    class_<luxrays::Property>("Property", init<string>())
		.def(init<string, bool>())
		.def(init<string, int>())
		.def(init<string, double>())
		.def(init<string, string>())
		.def("__init__", make_constructor(Property_InitWithList))

		.def("GetName", &luxrays::Property::GetName, return_internal_reference<>())
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

		// Required because Properties::Set is overloaded
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Property &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &, const std::string &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def("SetFromFile", &luxrays::Properties::SetFromFile, return_internal_reference<>())
		.def("SetFromString", &luxrays::Properties::SetFromString, return_internal_reference<>())

		.def("Clear", &luxrays::Properties::Clear, return_internal_reference<>())
		.def("GetAllNames", &Properties_GetAllNames1)
		.def("GetAllNames", &Properties_GetAllNames2)
		.def("GetAllUniqueSubNames", &Properties_GetAllUniqueSubNames)
		.def("HaveNames", &luxrays::Properties::HaveNames)
		.def("GetAllProperties", &luxrays::Properties::GetAllProperties)

		.def<const luxrays::Property &(luxrays::Properties::*)(const std::string &) const>
			("Get", &luxrays::Properties::Get, return_internal_reference<>())
		.def("Get", &Properties_GetWithDefaultValues)
	
		.def(self_ns::str(self))
    ;

	//--------------------------------------------------------------------------
	// Scene class
	//--------------------------------------------------------------------------

    class_<Scene>("Scene", init<optional<float> >())
		.def(init<string, optional<float> >())
		.def("GetProperties", &Scene::GetProperties, return_internal_reference<>())
		.def("Parse", &Scene::Parse)
		.def("DeleteObject", &Scene::DeleteObject)
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
		.def("GetScene", &RenderConfig::GetScene, return_internal_reference<>())
		.def("Parse", &RenderConfig::Parse)
    ;

	//--------------------------------------------------------------------------
	// RenderSession class
	//--------------------------------------------------------------------------

    class_<RenderSession>("RenderSession", init<RenderConfig *>()[with_custodian_and_ward<1, 2>()])
		.def("Start", &RenderSession::Start)
		.def("Stop", &RenderSession::Stop)
		.def("BeginSceneEdit", &RenderSession::BeginSceneEdit)
		.def("EndSceneEdit", &RenderSession::EndSceneEdit)
		.def("NeedPeriodicFilmSave", &RenderSession::NeedPeriodicFilmSave)
		.def("SaveFilm", &RenderSession::SaveFilm)
		.def("GetScreenBuffer", &RenderSession_GetScreenBuffer)
		.def("UpdateStats", &RenderSession::UpdateStats)
		.def("GetStats", &RenderSession::GetStats, return_internal_reference<>())
    ;
}

}
