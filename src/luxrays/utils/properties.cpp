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

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <algorithm>

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/foreach.hpp>

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

Property::~Property() {
}

void Property::Clear() {
	values.clear();
}

template<class T> T Property::Get(const u_int index) const {
	if (index >= values.size())
		throw runtime_error("Out of bound error for property: " + name);

	return boost::apply_visitor(GetVistor<T>(), values[index]);
}

std::string Property::GetString() const {
	stringstream ss;

	for (uint i = 0; i < values.size(); ++i) {
		if (i != 0)
			ss << " ";
		ss << Get<string>(i);
	}

	return ss.str();
}

std::string Property::ToString() const {
	return name + " = " + GetString();
}

//------------------------------------------------------------------------------
// Properties class
//------------------------------------------------------------------------------

Properties::Properties(const string &fileName) {
	LoadFromFile(fileName);
}

void Properties::Load(const Properties &p) {
	const std::vector<std::string> &keys = p.GetAllKeys();
	for (std::vector<std::string>::const_iterator it = keys.begin(); it != keys.end(); ++it)
		SetString(*it, p.GetString(*it, ""));
}

void Properties::Load(istream &stream) {
	char buf[512];

	for (int lineNumber = 1;; ++lineNumber) {
		stream.getline(buf, 512);
		if (stream.eof())
			break;
		// Ignore comments
		if (buf[0] == '#')
			continue;

		string line = buf;
		size_t idx = line.find('=');
		if (idx == string::npos) {
			sprintf(buf, "Syntax error in a Properties at line %d", lineNumber);
			throw runtime_error(buf);
		}

		// Check if it is a valid key
		string key(line.substr(0, idx));
		boost::trim(key);
		string value(line.substr(idx + 1));
		// Check if the last char is a LF or a CR and remove that (in case of
		// a DOS file read under Linux/MacOS)
		if ((value.size() > 0) && ((value[value.size() - 1] == '\n') || (value[value.size() - 1] == '\r')))
			value.resize(value.size() - 1);
		boost::trim(value);

		SetString(key, value);
	}
}

void Properties::LoadFromFile(const string &fileName) {
	BOOST_IFSTREAM file(fileName.c_str(), ios::in);
	char buf[512];
	if (file.fail()) {
		sprintf(buf, "Unable to open file %s", fileName.c_str());
		throw runtime_error(buf);
	}

	Load(file);
}

void Properties::LoadFromString(const string &propDefinitions) {
	istringstream stream(propDefinitions);

	Load(stream);
}

void Properties::Clear() {
	keys.clear();
	props.clear();
}

const vector<string> &Properties::GetAllKeys() const {
	return keys;
}

vector<string> Properties::GetAllKeys(const string prefix) const {
	vector<string> keysSubset;
	for (vector<string>::const_iterator it = keys.begin(); it != keys.end(); ++it) {
		if (it->find(prefix) == 0)
			keysSubset.push_back(*it);
	}

	return keysSubset;
}

bool Properties::IsDefined(const string &propName) const {
	return (props.count(propName) != 0);
}

void Properties::Delete(const string &propName) {
	vector<string>::iterator it = find(keys.begin(), keys.end(), propName);
	if (it != keys.end())
		keys.erase(it);

	props.erase(propName);
}

string Properties::ToString() const {
	stringstream ss;

	for (vector<string>::const_iterator i = keys.begin(); i != keys.end(); ++i)
		ss << props.at(*i).ToString() << "\n";

	return ss.str();
}

Properties &Properties::Set(const Property &prop) {
	const string &propName = prop.GetName();

	if (!IsDefined(propName)) {
		// It is a new key
		keys.push_back(propName);
	}

	props.insert(std::pair<string, Property>(propName, prop));

	return *this;
}

Properties &Properties::operator%=(const Property &prop) {
	return Set(prop);
}

Properties &Properties::operator%(const Property &prop) {
	return Set(prop);
}

string Properties::ExtractField(const string &value, const size_t index) {
	char buf[512];
	memcpy(buf, value.c_str(), value.length() + 1);
	char *t = strtok(buf, ".");
	if ((index == 0) && (t == NULL))
		return value;

	size_t i = index;
	while (t != NULL) {
		if (i-- == 0)
			return string(t);
		t = strtok(NULL, ".");
	}

	return "";
}

Properties luxrays::operator%(const Property &prop0, const Property &prop1) {
	return Properties() % prop0 % prop1;
}

//------------------------------------------------------------------------------
// Old deprecated interface
//------------------------------------------------------------------------------

string Properties::GetString(const string &propName, const string defaultValue) const {
	if (IsDefined(propName))
		return props.find(propName)->second.GetString();
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
	if (props.find(propName) == props.end()) {
		// It is a new key
		keys.push_back(propName);
	}

	props.insert(std::make_pair(propName, Property(propName, value)));
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
