/***************************************************************************
 *   Copyright (C) 1998-2010 by authors (see AUTHORS.txt )                 *
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
#include <stdexcept>
#include <fstream>

#include <boost/algorithm/string.hpp>
#include <vector>

#include "luxrays/utils/properties.h"
#include "luxrays/core/utils.h"

using namespace luxrays;

Properties::Properties(const std::string &fileName) {
	LoadFile(fileName);
}

void Properties::LoadFile(const std::string &fileName) {
	std::ifstream file(fileName.c_str(), std::ios::in);
	char buf[512];
	for (int line = 1;; ++line) {
		file.getline(buf, 512);
		if (file.eof())
			break;
		// Ignore comments
		if (buf[0] == '#')
			continue;

		std::string line = buf;
		size_t idx = line.find('=');
		if (idx == std::string::npos)
			throw std::runtime_error("Syntax error at line " + line);

		// Check if it is a valid key
		std::string key(line.substr(0, idx));
		StringTrim(key);
		std::string value(line.substr(idx + 1));
		StringTrim(value);

		props.erase(key);
		props.insert(std::make_pair(key, std::string(value)));
	}
}

std::vector<std::string> Properties::GetAllKeys() const {
	std::vector<std::string> keys;
	for (std::map<std::string, std::string>::const_iterator it = props.begin(); it != props.end(); ++it)
		keys.push_back(it->first);

	return keys;
}

std::string Properties::GetString(const std::string propName, const std::string defaultValue) const {
	std::map<std::string, std::string>::const_iterator it = props.find(propName);

	if (it == props.end())
		return defaultValue;
	else
		return it->second;
}

int Properties::GetInt(const std::string propName, const int defaultValue) const {
	std::string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return atoi(s.c_str());
}

float Properties::GetFloat(const std::string propName, const float defaultValue) const {
	std::string s = GetString(propName, "");

	if (s.compare("") == 0)
		return defaultValue;
	else
		return atof(s.c_str());
}

std::vector<int> Properties::GetIntVector(const std::string propName, const std::string &defaultValue) const {
	std::string s = GetString(propName, "");

	if (s.compare("") == 0)
		return ConvertToIntVector(defaultValue);
	else
		return ConvertToIntVector(s);
}

std::vector<float> Properties::GetFloatVector(const std::string propName, const std::string &defaultValue) const {
	std::string s = GetString(propName, "");

	if (s.compare("") == 0)
		return ConvertToFloatVector(defaultValue);
	else
		return ConvertToFloatVector(s);
}

std::vector<int> Properties::ConvertToIntVector(const std::string &values) {
	std::vector<std::string> strs;
	boost::split(strs, values, boost::is_any_of("\t "));

	std::vector<int> ints;
	for (std::vector<std::string>::iterator it = strs.begin(); it != strs.end(); ++it)
		ints.push_back(atoi(it->c_str()));

	return ints;
}

std::vector<float> Properties::ConvertToFloatVector(const std::string &values) {
	std::vector<std::string> strs;
	boost::split(strs, values, boost::is_any_of("\t "));

	std::vector<float> floats;
	for (std::vector<std::string>::iterator it = strs.begin(); it != strs.end(); ++it)
		floats.push_back(atof(it->c_str()));

	return floats;
}
