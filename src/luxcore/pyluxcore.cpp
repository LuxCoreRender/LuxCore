/***************************************************************************
 *   Copyright (C) 1998-2013 by authors (see AUTHORS.txt)                  *
 *                                                                         *
 *   This file is part of LuxRays.                                         *
 *                                                                         *
 *   LuxRays is free software; you can redistribute it and/or modify       *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   LuxRays is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *                                                                         *
 *   LuxRays website: http://www.luxrender.net                             *
 ***************************************************************************/

#include <boost/python.hpp>
#include <boost/foreach.hpp>

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

static luxrays::Property *Property_InitWithList(const str &name, boost::python::list &l) {
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
			throw std::runtime_error("Unsupported data type included in Property constructor list: " + objType);
	}

	return prop;
}

static boost::python::list Property_GetValuesList(luxrays::Property *prop) {
	boost::python::list l;
	for (u_int i = 0; i < prop->GetSize(); ++i) {
		const std::type_info &tinfo = prop->GetValueType(i);

		if (tinfo == typeid(bool))
			l.append(prop->GetValue<bool>(i));
		else if (tinfo == typeid(int))
			l.append(prop->GetValue<int>(i));
		else if (tinfo == typeid(double))
			l.append(prop->GetValue<double>(i));
		else if (tinfo == typeid(string))
			l.append(prop->GetValue<string>(i));
		else
			throw std::runtime_error("Unsupported data type in list extraction of Property: " + prop->GetName());
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

static boost::python::list Properties_GetAllUniqueNames(luxrays::Properties *props, const string &prefix) {
	boost::python::list l;
	const vector<string> keys = props->GetAllUniqueNames(prefix);
	BOOST_FOREACH(const string &key, keys) {
		l.append(key);
	}

	return l;
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

		.def("GetValue", &luxrays::Property::GetValue<bool>)
		.def("GetValue", &luxrays::Property::GetValue<int>)
		.def("GetValue", &luxrays::Property::GetValue<double>)
		.def("GetValue", &luxrays::Property::GetValue<string>)

		.def("Get", &Property_GetValuesList)
	
		.def("GetValuesString", &luxrays::Property::GetValuesString)
		.def("ToString", &luxrays::Property::ToString)

		.def("Add", &luxrays::Property::Add<bool>, return_internal_reference<>())
		.def("Add", &luxrays::Property::Add<int>, return_internal_reference<>())
		.def("Add", &luxrays::Property::Add<double>, return_internal_reference<>())
		.def("Add", &luxrays::Property::Add<string>, return_internal_reference<>())

		.def("Set", &luxrays::Property::Set<bool>, return_internal_reference<>())
		.def("Set", &luxrays::Property::Set<int>, return_internal_reference<>())
		.def("Set", &luxrays::Property::Set<double>, return_internal_reference<>())
		.def("Set", &luxrays::Property::Set<string>, return_internal_reference<>())

		.def(self_ns::str(self))
    ;

	//--------------------------------------------------------------------------
	// Properties class
	//--------------------------------------------------------------------------

    class_<luxrays::Properties>("Properties", init<>())
		.def(init<string>())

		// Required because Properties::Set is overloaded
		.def<luxrays::Properties &(luxrays::Properties::*)(const luxrays::Properties &)>
			("Set", &luxrays::Properties::Set, return_internal_reference<>())
		.def("SetFromFile", &luxrays::Properties::SetFromFile, return_internal_reference<>())
		.def("SetFromString", &luxrays::Properties::SetFromString, return_internal_reference<>())

		.def("Clear", &luxrays::Properties::Clear, return_internal_reference<>())
		.def("GetAllNames", &Properties_GetAllNames1)
		.def("GetAllNames", &Properties_GetAllNames2)
		.def("GetAllUniqueNames", &Properties_GetAllUniqueNames)

		.def(self_ns::str(self))
    ;
}

}
