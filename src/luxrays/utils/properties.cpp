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

#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"
#include "luxrays/core/utils.h"

using namespace luxrays;
using namespace std;

template<> PropertyValues luxrays::MakePropertyValues<Matrix4x4>(const Matrix4x4 &m) {
	PropertyValues values(16);

	for (u_int i = 0; i < 4; ++i) {
		for (u_int j = 0; j < 4; ++j) {
			values[i * 4 + j] = m.m[j][i];
		}
	}

	return values;
}

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
		ss << GetValue<string>(i);
	}
	return ss.str();
}

//------------------------------------------------------------------------------
// Basic types
//------------------------------------------------------------------------------

template<> bool Property::Get<bool>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<bool>(0);
}

template<> int Property::Get<int>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<int>(0);
}

template<> u_int Property::Get<u_int>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<u_int>(0);
}

template<> float Property::Get<float>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<float>(0);
}

template<> double Property::Get<double>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<double>(0);
}

template<> size_t Property::Get<size_t>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<size_t>(0);
}

template<> string Property::Get<string>() const {
	if (values.size() != 1)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return GetValue<string>(0);
}

//------------------------------------------------------------------------------
// LuxRays types
//------------------------------------------------------------------------------

template<> luxrays::UV Property::Get<luxrays::UV>() const {
	if (values.size() != 2)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::UV(GetValue<float>(0), GetValue<float>(1));
}

template<> luxrays::Vector Property::Get<luxrays::Vector>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::Vector(GetValue<float>(0), GetValue<float>(1), GetValue<float>(2));
}

template<> luxrays::Normal Property::Get<luxrays::Normal>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::Normal(GetValue<float>(0), GetValue<float>(1), GetValue<float>(2));
}

template<> luxrays::Point Property::Get<luxrays::Point>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::Point(GetValue<float>(0), GetValue<float>(1), GetValue<float>(2));
}

template<> luxrays::Spectrum Property::Get<luxrays::Spectrum>() const {
	if (values.size() != 3)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::Spectrum(GetValue<float>(0), GetValue<float>(1), GetValue<float>(2));
}

template<> luxrays::Matrix4x4 Property::Get<luxrays::Matrix4x4>() const {
	if (values.size() != 16)
		throw std::runtime_error("Wrong number of values in property: " + name);
	return luxrays::Matrix4x4(
			GetValue<float>(0), GetValue<float>(1), GetValue<float>(2), GetValue<float>(3),
			GetValue<float>(4), GetValue<float>(5), GetValue<float>(6), GetValue<float>(7),
			GetValue<float>(8), GetValue<float>(9), GetValue<float>(10), GetValue<float>(11),
			GetValue<float>(12), GetValue<float>(13), GetValue<float>(14), GetValue<float>(15));
}

std::string Property::ToString() const {
	return name + " = " + GetValuesString();
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

Properties &Properties::Set(istream &stream) {
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
						prop.Add(value.substr(first, last - first - 1));
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

	return Set(file);
}

Properties &Properties::SetFromString(const string &propDefinitions) {
	istringstream stream(propDefinitions);

	return Set(stream);
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

bool Properties::IsDefined(const string &propName) const {
	return (props.count(propName) != 0);
}

const Property &Properties::Get(const std::string &propName) const {
	boost::unordered_map<std::string, Property>::const_iterator it = props.find(propName);
	if (it == props.end())
		throw runtime_error("Undefined property in Properties::Get(): " + propName);

	return it->second;
}

const Property Properties::Get(const std::string &propName, const PropertyValues &defaultValues) const {
	boost::unordered_map<std::string, Property>::const_iterator it = props.find(propName);
	if (it == props.end())
		return Property(propName, defaultValues);

	return it->second;
}

void Properties::Delete(const string &propName) {
	vector<string>::iterator it = find(names.begin(), names.end(), propName);
	if (it != names.end())
		names.erase(it);

	props.erase(propName);
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

//------------------------------------------------------------------------------
// Old deprecated interface
//------------------------------------------------------------------------------

string Properties::GetString(const string &propName, const string defaultValue) const {
	if (IsDefined(propName))
		return props.find(propName)->second.GetValuesString();
	else
		return defaultValue;
}

bool Properties::GetBoolean(const string &propName, const bool defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return boost::lexical_cast<bool>(s);
}

int Properties::GetInt(const string &propName, const int defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return boost::lexical_cast<int>(s);
}

size_t Properties::GetSize(const string &propName, const size_t defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return boost::lexical_cast<size_t>(s);
}

float Properties::GetFloat(const string &propName, const float defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return static_cast<float>(boost::lexical_cast<double>(s));
}

vector<string> Properties::GetStringVector(const string &propName, const string &defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return ConvertToStringVector(defaultValue);
	else
		return ConvertToStringVector(s);
}

vector<int> Properties::GetIntVector(const string &propName, const string &defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return ConvertToIntVector(defaultValue);
	else
		return ConvertToIntVector(s);
}

vector<float> Properties::GetFloatVector(const string &propName, const string &defaultValue) const {
	string s = GetString(propName, "");

	if (s.compare("") == 0)
		return ConvertToFloatVector(defaultValue);
	else
		return ConvertToFloatVector(s);
}

void Properties::SetString(const string &propName, const string &value) {
	Set(Property(propName)(value));
}

string Properties::SetString(const string &property) {
	vector<string> strs;
	boost::split(strs, property, boost::is_any_of("="));

	if (strs.size() != 2)
		throw runtime_error("Syntax error in property definition");

	boost::trim(strs[0]);
	boost::trim(strs[1]);
	SetString(strs[0], strs[1]);

	return strs[0];
}

vector<string>  Properties::ConvertToStringVector(const string &values) {
	vector<string> strs;
	boost::split(strs, values, boost::is_any_of("|"));

	vector<string> strs2;
	for (vector<string>::iterator it = strs.begin(); it != strs.end(); ++it) {
		if (it->length() != 0)
			strs2.push_back(*it);
	}

	return strs2;
}

vector<int> Properties::ConvertToIntVector(const string &values) {
	vector<string> strs;
	boost::split(strs, values, boost::is_any_of("\t "));

	vector<int> ints;
	for (vector<string>::iterator it = strs.begin(); it != strs.end(); ++it) {
		if (it->length() != 0) {
			const int i = boost::lexical_cast<int>(*it);
			ints.push_back(i);
		}
	}

	return ints;
}

vector<float> Properties::ConvertToFloatVector(const string &values) {
	vector<string> strs;
	boost::split(strs, values, boost::is_any_of("\t "));

	vector<float> floats;
	for (vector<string>::iterator it = strs.begin(); it != strs.end(); ++it) {
		if (it->length() != 0) {
			const double f = boost::lexical_cast<double>(*it);
			floats.push_back(static_cast<float>(f));
		}
	}

	return floats;
}
