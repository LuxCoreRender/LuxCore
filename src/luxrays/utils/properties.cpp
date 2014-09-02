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

#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/regex.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/utils.h"

using namespace luxrays;
using namespace std;

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

Property::Property(const string &propName) : name(propName) {
}

Property::Property(const std::string &propName, const PropertyValue &val) :
	name(propName) {
	values.push_back(val);
}

Property::Property(const std::string &propName, const PropertyValues &vals) :
	name(propName) {
	values = vals;
}

Property::~Property() {
}

Property &Property::Clear() {
	values.clear();
	return *this;
}

std::string Property::GetValuesString() const {
	stringstream ss;

	for (u_int i = 0; i < values.size(); ++i) {
		if (i != 0)
			ss << " ";
		ss << Get<string>(i);
	}
	return ss.str();
}

//------------------------------------------------------------------------------
// Get basic types
//------------------------------------------------------------------------------

namespace luxrays {

template<> bool Property::Get<bool>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<bool>(0);
}

template<> int Property::Get<int>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<int>(0);
}

template<> u_int Property::Get<u_int>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<u_int>(0);
}

template<> float Property::Get<float>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<float>(0);
}

template<> double Property::Get<double>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<double>(0);
}

template<> u_longlong Property::Get<u_longlong>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<u_longlong>(0);
}

template<> string Property::Get<string>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Get<string>(0);
}

}

//------------------------------------------------------------------------------
// Get LuxRays types
//------------------------------------------------------------------------------

template<> UV Property::Get<UV>() const {
	if (values.size() != 2)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return UV(Get<float>(0), Get<float>(1));
}

template<> Vector Property::Get<Vector>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Vector(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Normal Property::Get<Normal>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Normal(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Point Property::Get<Point>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Point(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Spectrum Property::Get<Spectrum>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Spectrum(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Matrix4x4 Property::Get<Matrix4x4>() const {
	if (values.size() != 16)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return Matrix4x4(
			Get<float>(0), Get<float>(4), Get<float>(8), Get<float>(12),
			Get<float>(1), Get<float>(5), Get<float>(9), Get<float>(13),
			Get<float>(2), Get<float>(6), Get<float>(10), Get<float>(14),
			Get<float>(3), Get<float>(7), Get<float>(11), Get<float>(15));
}

//------------------------------------------------------------------------------
// Add LuxRays types
//------------------------------------------------------------------------------

template<> Property &Property::Add<UV>(const UV &v) {
	return Add(v.u).Add(v.v);
}

template<> Property &Property::Add<Vector>(const Vector &v) {
	return Add(v.x).Add(v.y).Add(v.z);
}

template<> Property &Property::Add<Normal>(const Normal &v) {
	return Add(v.x).Add(v.y).Add(v.z);
}

template<> Property &Property::Add<Point>(const Point &v) {
	return Add(v.x).Add(v.y).Add(v.z);
}

template<> Property &Property::Add<Spectrum>(const Spectrum &v) {
	return Add(v.c[0]).Add(v.c[1]).Add(v.c[2]);
}

template<> Property &Property::Add<Matrix4x4>(const Matrix4x4 &m) {
	for (u_int i = 0; i < 4; ++i) {
		for (u_int j = 0; j < 4; ++j) {
			Add(m.m[j][i]);
		}
	}

	return *this;
}

//------------------------------------------------------------------------------

std::string Property::ToString() const {
	stringstream ss;

	ss << name + " = ";
	for (u_int i = 0; i < values.size(); ++i) {
		if (i != 0)
			ss << " ";
		
		if (GetValueType(i) == typeid(string)) {
			// Escape " char
			string s = Get<string>(i);
			boost::replace_all(s, "\"", "\\\"");
			ss << "\"" << s << "\"";
		} else
			ss << Get<string>(i);
	}

	return ss.str();
}

string Property::ExtractField(const string &name, const u_int index) {
	vector<string> strs;
	boost::split(strs, name, boost::is_any_of("."));

	if (index >= strs.size())
		return "";

	return strs[index];
}

string Property::ExtractPrefix(const string &name, const u_int count) {
	if (count <= 0)
		return "";

	size_t index = 0;
	for (u_int i = 0; i < count; ++i) {
		if (index >= name.length())
			return "";

		index = name.find('.', index);

		if (index == string::npos)
			return "";

		++index;
	}

	return name.substr(0, index - 1);
}

//------------------------------------------------------------------------------
// Properties class
//------------------------------------------------------------------------------

Properties::Properties(const string &fileName) {
	SetFromFile(fileName);
}

u_int Properties::GetSize() const {
	return names.size();
}

Properties &Properties::Set(const Properties &props) {
	BOOST_FOREACH(const string &name, props.GetAllNames()) {
		this->Set(props.Get(name));
	}

	return *this;
}

Properties &Properties::Set(const Properties &props, const std::string &prefix) {
	BOOST_FOREACH(const string &name, props.GetAllNames()) {
		Set(props.Get(name).AddedNamePrefix(prefix));
	}

	return *this;	
}

Properties &Properties::SetFromStream(istream &stream) {
	char buf[512];

	for (int lineNumber = 1;; ++lineNumber) {
		if (stream.eof())
			break;

		buf[0] = 0;
		stream.getline(buf, 512);

		// Ignore comments
		if (buf[0] == '#')
			continue;

		string line(buf);
		boost::trim(line);

		// Ignore empty lines
		if (line.length() == 0)
			continue;

		size_t idx = line.find('=');
		if (idx == string::npos)
			throw runtime_error("Syntax error in a Properties at line " + luxrays::ToString(lineNumber));

		// Check if it is a valid name
		string name(line.substr(0, idx));
		boost::trim(name);
		Property prop(name);

		string value(line.substr(idx + 1));
		// Check if the last char is a LF or a CR and remove that (in case of
		// a DOS file read under Linux/MacOS)
		if ((value.size() > 0) && ((value[value.size() - 1] == '\n') || (value[value.size() - 1] == '\r')))
			value.resize(value.size() - 1);
		boost::trim(value);

		// Iterate over value and extract all field (handling quotes)
		u_int first = 0;
		u_int last = 0;
		const u_int len = value.length();
		while (first < len) {
			// Check if it is a quoted field
			if ((value[first] == '"') || (value[first] == '\'')) {
				++first;
				last = first;
				bool found = false;
				while (last < len) {
					if ((value[last] == '"') || (value[last] == '\'')) {
						// Replace any escaped " or '
						string s = value.substr(first, last - first);
						boost::replace_all(s,"\\\"", "\"");
						boost::replace_all(s,"\\\'", "'");

						prop.Add(s);
						found = true;
						++last;

						// Eat all additional spaces
						while ((last < len) && ((value[last] == ' ') || (value[last] == '\t')))
							++last;
						break;
					}

					++last;
				}

				if (!found) 
					throw runtime_error("Unterminated quote in property: " + name);
			} else {
				last = first;
				while (last < len) {
					if ((value[last] == ' ') || (value[last] == '\t') || (last == len - 1)) {
						string field;
						if (last == len - 1) {
							field = value.substr(first, last - first + 1);
							++last;
						} else
							field = value.substr(first, last - first);
						prop.Add(field);

						// Eat all additional spaces
						while ((last < len) && ((value[last] == ' ') || (value[last] == '\t')))
							++last;
						break;
					}

					++last;
				}
			}

			first = last;
		}

		Set(prop);
	}

	return *this;
}

Properties &Properties::SetFromFile(const string &fileName) {
	BOOST_IFSTREAM file(fileName.c_str(), ios::in);
	char buf[512];
	if (file.fail()) {
		sprintf(buf, "Unable to open properties file: %s", fileName.c_str());
		throw runtime_error(buf);
	}

	return SetFromStream(file);
}

Properties &Properties::SetFromString(const string &propDefinitions) {
	istringstream stream(propDefinitions);

	return SetFromStream(stream);
}

Properties &Properties::Clear() {
	names.clear();
	props.clear();

	return *this;
}

const vector<string> &Properties::GetAllNames() const {
	return names;
}

vector<string> Properties::GetAllNames(const string &prefix) const {
	vector<string> namesSubset;
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0)
			namesSubset.push_back(name);
	}

	return namesSubset;
}

vector<string> Properties::GetAllNamesRE(const string &regularExpression) const {
	boost::regex re(regularExpression);
	
	vector<string> namesSubset;
	BOOST_FOREACH(const string &name, names) {
		if (boost::regex_match(name, re))
			namesSubset.push_back(name);
	}

	return namesSubset;
}

vector<string> Properties::GetAllUniqueSubNames(const string &prefix) const {
	size_t fieldsCount = std::count(prefix.begin(), prefix.end(), '.') + 2;

	set<string> definedNames;
	vector<string> namesSubset;
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0) {
			// Check if it has been already defined

			const string s = Property::ExtractPrefix(name, fieldsCount);
			if (definedNames.count(s) == 0) {
				namesSubset.push_back(s);
				definedNames.insert(s);
			}
		}
	}

	return namesSubset;
}

bool Properties::HaveNames(const std::string &prefix) const {
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0)
			return true;
	}

	return false;
}

bool Properties::HaveNamesRE(const std::string &regularExpression) const {
	boost::regex re(regularExpression);

	BOOST_FOREACH(const string &name, names) {
		if (boost::regex_match(name, re))
			return true;
	}

	return false;
}

Properties Properties::GetAllProperties(const std::string &prefix) const {
	Properties subset;
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0)
			subset.Set(Get(name));
	}

	return subset;
}

bool Properties::IsDefined(const string &propName) const {
	return (props.count(propName) != 0);
}

const Property &Properties::Get(const std::string &propName) const {
	boost::unordered_map<std::string, Property>::const_iterator it = props.find(propName);
	if (it == props.end())
		throw runtime_error("Undefined property in Properties::Get(): " + propName);

	return it->second;
}

const Property &Properties::Get(const Property &prop) const {
	boost::unordered_map<std::string, Property>::const_iterator it = props.find(prop.GetName());
	if (it == props.end())
		return prop;

	return it->second;
}

void Properties::Delete(const string &propName) {
	vector<string>::iterator it = find(names.begin(), names.end(), propName);
	if (it != names.end())
		names.erase(it);

	props.erase(propName);
}

void Properties::DeleteAll(const vector<string> &propNames) {
	BOOST_FOREACH(const string &n, propNames)
		Delete(n);
}

string Properties::ToString() const {
	stringstream ss;

	for (vector<string>::const_iterator i = names.begin(); i != names.end(); ++i)
		ss << props.at(*i).ToString() << "\n";

	return ss.str();
}

Properties &Properties::Set(const Property &prop) {
	const string &propName = prop.GetName();

	if (!IsDefined(propName)) {
		// It is a new name
		names.push_back(propName);
	} else {
		// boost::unordered_set::insert() doesn't overwrite an existing entry
		props.erase(propName);
	}

	props.insert(std::pair<string, Property>(propName, prop));

	return *this;
}

Properties &Properties::operator<<(const Property &prop) {
	return Set(prop);
}

Properties &Properties::operator<<(const Properties &props) {
	return Set(props);
}

Properties luxrays::operator<<(const Property &prop0, const Property &prop1) {
	return Properties() << prop0 << prop1;
}

Properties luxrays::operator<<(const Property &prop0, const Properties &props) {
	return Properties() << prop0 << props;
}
