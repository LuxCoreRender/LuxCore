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

#include "luxcore/luxcore.h"
#include "luxrays/utils/properties.h"

using namespace std;
using namespace luxcore;

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

Property::Property(const string &propName) : prop(propName) {
}

Property::Property(const std::string &propName, const PropertyValue &val) :
	prop(propName, val) {
}

Property::~Property() {
}

void Property::Clear() {
	prop.Clear();
}

template<class T> T Property::Get(const u_int index) const {
	return prop.Get<T>();
}

std::string Property::GetString() const {
	return prop.GetString();
}

std::string Property::ToString() const {
	return prop.ToString();
}

//------------------------------------------------------------------------------
// Properties class
//------------------------------------------------------------------------------

Properties::Properties() {
}

Properties::Properties(const string &fileName) : props(fileName) {
}

Properties::~Properties() {
}

void Properties::Load(const Properties &p) {
	props.Load(p.props);
}

void Properties::Load(istream &stream) {
	props.Load(stream);
}

void Properties::LoadFromFile(const string &fileName) {
	props.LoadFromFile(fileName);
}

void Properties::LoadFromString(const string &propDefinitions) {
	props.LoadFromFile(propDefinitions);
}

void Properties::Clear() {
	props.Clear();
}

const vector<string> &Properties::GetAllKeys() const {
	return props.GetAllKeys();
}

vector<string> Properties::GetAllKeys(const string prefix) const {
	return props.GetAllKeys(prefix);
}

bool Properties::IsDefined(const string &propName) const {
	return props.IsDefined(propName);
}

void Properties::Delete(const string &propName) {
	props.Delete(propName);
}

string Properties::ToString() const {
	return props.ToString();
}

Properties &Properties::Set(const Property &prop) {
	props.Set(prop.prop);

	return *this;
}

Properties &Properties::operator%=(const Property &prop) {
	props %= prop.prop;

	return *this;
}

Properties &Properties::operator%(const Property &prop) {
	props % prop.prop;

	return *this;
}

string Properties::ExtractField(const string &value, const size_t index) {
	return luxrays::Properties::ExtractField(value, index);
}

Properties luxcore::operator%(const Property &prop0, const Property &prop1) {
	return Properties() % prop0 % prop1;
}
