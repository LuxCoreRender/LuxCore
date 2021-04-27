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

#include <set>
#include <vector>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include <boost/foreach.hpp>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/regex.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include "luxrays/utils/utils.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"

using namespace luxrays;
using namespace std;

//------------------------------------------------------------------------------
// Blob class
//------------------------------------------------------------------------------

Blob::Blob(const Blob &blob) {
	data = new char[blob.size];
	size = blob.size;

	copy(blob.data, blob.data + blob.size, data);
}

Blob::Blob(const char *d, const size_t s) {
	data = new char[s];
	copy(d, d + s, data);

	size = s;
}

Blob::Blob(const string &base64Data) {
	using namespace boost::archive::iterators;

	typedef transform_width<
			binary_from_base64<string::const_iterator>, 8, 6
		> binary_t;

	if (base64Data.size() < 5)
		throw runtime_error("Wrong base64 data length in Blob::Blob()");
	// +2 and -2 are there to remove "{[" and "]}" at the begin and the end
	string decoded(binary_t(base64Data.begin() + 2), binary_t(base64Data.end() - 2));

	size = decoded.length();
	data = new char[size];	
	copy(decoded.begin(), decoded.end(), data);
}

Blob::~Blob() {
	delete[] data;
}

Blob &Blob::operator=(const Blob &blob) {
	delete[] data;

	data = new char[blob.size];
	size = blob.size;

	copy(blob.data, blob.data + blob.size, data);

	return *this;
}

string Blob::ToString() const {
	stringstream ss;
	ss << *this;

	return ss.str();
}

ostream &luxrays::operator<<(ostream &os, const Blob &blob) {
	os << "{[";

	using namespace boost::archive::iterators;

	typedef base64_from_binary<
			transform_width<const char *, 6, 8>
		> base64_t;

	const char *data = blob.GetData();
	const size_t size = blob.GetSize();
	copy(base64_t(data), base64_t(data + size), boost::archive::iterators::ostream_iterator<char>(os));

	os << "]}";

	return os;
}

//------------------------------------------------------------------------------
// PropertyValue class
//------------------------------------------------------------------------------

PropertyValue::PropertyValue() : dataType(NONE_VAL) {
}

PropertyValue::PropertyValue(const PropertyValue &prop) : dataType(NONE_VAL) {
	Copy(prop, *this);
}

PropertyValue::PropertyValue(const bool val) : dataType(BOOL_VAL) {
	data.boolVal = val;
}

PropertyValue::PropertyValue(const int val) : dataType(INT_VAL) {
	data.intVal = val;
}

PropertyValue::PropertyValue(const unsigned int val) : dataType(UINT_VAL) {
	data.uintVal = val;
}

PropertyValue::PropertyValue(const float val) : dataType(FLOAT_VAL) {
	data.floatVal = val;
}

PropertyValue::PropertyValue(const double val) : dataType(DOUBLE_VAL) {
	data.doubleVal = val;
}

PropertyValue::PropertyValue(const long long val) : dataType(LONGLONG_VAL) {
	data.longlongVal = val;
}

PropertyValue::PropertyValue(const unsigned long long val) : dataType(ULONGLONG_VAL) {
	data.ulonglongVal = val;
}

PropertyValue::PropertyValue(const std::string &val) : dataType(STRING_VAL) {
	data.stringVal = new std::string(val);
}

PropertyValue::PropertyValue(const Blob &val) : dataType(BLOB_VAL) {
	data.blobVal = new Blob(val);
}

PropertyValue::~PropertyValue() {
	switch (dataType) {
		case NONE_VAL:
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
			break;
		case STRING_VAL:
			delete data.stringVal;
			break;
		case BLOB_VAL:
			delete data.blobVal;
			break;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::~PropertyValue(): " + ToString(dataType));
	}
}

template<> bool PropertyValue::Get<bool>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<bool>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<bool>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<bool>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<bool>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<bool>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<bool>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<bool>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<bool>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<bool>(): " + ToString(dataType));
	}
}

template<> int PropertyValue::Get<int>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<int>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<int>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<int>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<int>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<int>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<int>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<int>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<int>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<int>(): " + ToString(dataType));
	}
}

template<> unsigned int PropertyValue::Get<unsigned int>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<unsigned int>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<unsigned int>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<unsigned int>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<unsigned int>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<unsigned int>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<unsigned int>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<unsigned int>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<unsigned int>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<unsigned int>(): " + ToString(dataType));
	}
}

template<> float PropertyValue::Get<float>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<float>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<float>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<float>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<float>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<float>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<float>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<float>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<float>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<float>(): " + ToString(dataType));
	}
}

template<> double PropertyValue::Get<double>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<double>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<double>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<double>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<double>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<double>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<double>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<double>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<double>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<double>(): " + ToString(dataType));
	}
}

template<> unsigned long long PropertyValue::Get<unsigned long long>() const {
	switch (dataType) {
		case BOOL_VAL:
			return boost::lexical_cast<unsigned long long>(data.boolVal);
		case INT_VAL:
			return boost::lexical_cast<unsigned long long>(data.intVal);
		case UINT_VAL:
			return boost::lexical_cast<unsigned long long>(data.uintVal);
		case FLOAT_VAL:
			return boost::lexical_cast<unsigned long long>(data.floatVal);
		case DOUBLE_VAL:
			return boost::lexical_cast<unsigned long long>(data.doubleVal);
		case LONGLONG_VAL:
			return boost::lexical_cast<unsigned long long>(data.longlongVal);
		case ULONGLONG_VAL:
			return boost::lexical_cast<unsigned long long>(data.ulonglongVal);
		case STRING_VAL:
			return FromString<unsigned long long>(*data.stringVal);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<unsigned long long>(): " + ToString(dataType));
	}
}

template<> string PropertyValue::Get<string>() const {
	switch (dataType) {
		case BOOL_VAL:
			return ToString(data.boolVal);
		case INT_VAL:
			return ToString(data.intVal);
		case UINT_VAL:
			return ToString(data.uintVal);
		case FLOAT_VAL:
			return ToString(data.floatVal);
		case DOUBLE_VAL:
			return ToString(data.doubleVal);
		case LONGLONG_VAL:
			return ToString(data.longlongVal);
		case ULONGLONG_VAL:
			return ToString(data.ulonglongVal);
		case STRING_VAL:
			return ToString(*data.stringVal);
		case BLOB_VAL:
			return data.blobVal->ToString();
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<string>(): " + ToString(dataType));
	}
}

template<> const Blob &PropertyValue::Get<const Blob &>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			throw std::runtime_error("Only a Blob property can be converted in a Blob");
		case BLOB_VAL:
			return *data.blobVal;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<const Blob &>(): " + ToString(dataType));
	}
}

PropertyValue::DataType PropertyValue::GetValueType() const {
	return dataType;
}

PropertyValue &PropertyValue::operator=(const PropertyValue &propVal) {
	Copy(propVal, *this);

	return *this;
}

void PropertyValue::Copy(const PropertyValue &propVal0, PropertyValue &propVal1) {
	switch (propVal1.dataType) {
		case NONE_VAL:
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
			break;
		case STRING_VAL:
			delete propVal1.data.stringVal;
			break;
		case BLOB_VAL:
			delete propVal1.data.blobVal;
			break;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Copy(): " + ToString(propVal1.dataType));
	}

	propVal1.dataType = propVal0.dataType;

	switch (propVal1.dataType) {
		case NONE_VAL:
			// Nothig to do
			break;
		case BOOL_VAL:
			propVal1.data.boolVal = propVal0.data.boolVal;
			break;
		case INT_VAL:
			propVal1.data.intVal = propVal0.data.intVal;
			break;
		case UINT_VAL:
			propVal1.data.uintVal = propVal0.data.uintVal;
			break;
		case FLOAT_VAL:
			propVal1.data.floatVal = propVal0.data.floatVal;
			break;
		case DOUBLE_VAL:
			propVal1.data.doubleVal = propVal0.data.doubleVal;
			break;
		case LONGLONG_VAL:
			propVal1.data.longlongVal = propVal0.data.longlongVal;
			break;
		case ULONGLONG_VAL:
			propVal1.data.ulonglongVal = propVal0.data.ulonglongVal;
			break;
		case STRING_VAL:
			propVal1.data.stringVal = new std::string(*propVal0.data.stringVal);
			break;
		case BLOB_VAL:
			propVal1.data.blobVal = new Blob(*propVal0.data.blobVal);
			break;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Copy(): " + ToString(propVal1.dataType));
	}
}

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

Property::Property() : name("") {
}

Property::Property(const string &propName) : name(propName) {
}

Property::Property(const string &propName, const PropertyValue &val) :
	name(propName) {
	values.push_back(val);
}

Property::Property(const string &propName, const PropertyValues &vals) :
	name(propName) {
	values = vals;
}

Property::~Property() {
}

Property &Property::Clear() {
	values.clear();
	return *this;
}

string Property::GetValuesString() const {
	stringstream ss;

	for (unsigned int i = 0; i < values.size(); ++i) {
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
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<bool>(0);
}

template<> int Property::Get<int>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<int>(0);
}

template<> unsigned int Property::Get<unsigned int>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<unsigned int>(0);
}

template<> float Property::Get<float>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<float>(0);
}

template<> double Property::Get<double>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<double>(0);
}

template<> unsigned long long Property::Get<unsigned long long>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<unsigned long long>(0);
}

template<> string Property::Get<string>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<string>(0);
}

template<> const Blob &Property::Get<const Blob &>() const {
	if (values.size() != 1)
		throw runtime_error("Wrong number of values in property: " + name);
	return Get<const Blob &>(0);
}

}

//------------------------------------------------------------------------------
// Get LuxRays types
//------------------------------------------------------------------------------

template<> UV Property::Get<UV>() const {
	if (values.size() != 2)
		throw runtime_error("Wrong number of values in property: " + name);
	return UV(Get<float>(0), Get<float>(1));
}

template<> Vector Property::Get<Vector>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Vector(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Normal Property::Get<Normal>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Normal(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Point Property::Get<Point>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Point(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Spectrum Property::Get<Spectrum>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Spectrum(Get<float>(0), Get<float>(1), Get<float>(2));
}

template<> Matrix4x4 Property::Get<Matrix4x4>() const {
	if (values.size() != 16)
		throw runtime_error("Wrong number of values in property: " + name);
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
	for (unsigned int i = 0; i < 4; ++i) {
		for (unsigned int j = 0; j < 4; ++j) {
			Add(m.m[j][i]);
		}
	}

	return *this;
}

//------------------------------------------------------------------------------

void Property::FromString(string &line) {
	const size_t idx = line.find('=');
	if (idx == string::npos)
		throw runtime_error("Syntax error in property string: " + line);

	// Check if it is a valid name
	name = line.substr(0, idx);
	boost::trim(name);

	values.clear();

	string value(line.substr(idx + 1));
	// Check if the last char is a LF or a CR and remove that (in case of
	// a DOS file read under Linux/MacOS)
	if ((value.size() > 0) && ((value[value.size() - 1] == '\n') || (value[value.size() - 1] == '\r')))
		value.resize(value.size() - 1);
	boost::trim(value);

	// Iterate over value and extract all field (handling quotes)
	unsigned int first = 0;
	unsigned int last = 0;
	const unsigned int len = value.length();
	while (first < len) {
		// Check if it is a blob field
		if ((first + 5 < len) && (value[first] == '{') && (value[first + 1] == '[')) {
			last = first;
			bool found = false;
			while (last < len - 1) {
				if ((value[last] == ']') || (value[last + 1] == '}')) {
					const size_t size = last - first + 2; // +2 is for "]}"
					const Blob blob(value.substr(first, size).c_str());
					Add(blob);
					found = true;
					// Eat the "]}"
					last += 2;

					// Eat all additional spaces
					while ((last < len) && ((value[last] == ' ') || (value[last] == '\t')))
						++last;
					break;
				}

				++last;
			}

			if (!found) 
				throw runtime_error("Unterminated blob in property: " + name);
		} else
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

					Add(s);
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
					Add(field);

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
}

string Property::ToString() const {
	stringstream ss;

	ss << name + " = ";
	for (unsigned int i = 0; i < values.size(); ++i) {
		if (i != 0)
			ss << " ";
		
		if (GetValueType(i) == PropertyValue::STRING_VAL) {
			// Escape " char
			string s = Get<string>(i);
			boost::replace_all(s, "\"", "\\\"");
			ss << "\"" << s << "\"";
		} else
			ss << Get<string>(i);
	}

	return ss.str();
}

unsigned int Property::CountFields(const string &name) {
	return count(name.begin(), name.end(), '.') + 1;
}

string Property::ExtractField(const string &name, const unsigned int index) {
	vector<string> strs;
	boost::split(strs, name, boost::is_any_of("."));

	if (index >= strs.size())
		return "";

	return strs[index];
}

string Property::ExtractPrefix(const string &name, const unsigned int count) {
	if (count <= 0)
		return "";

	size_t index = 0;
	for (unsigned int i = 0; i < count; ++i) {
		if (index >= name.length())
			return "";

		index = name.find('.', index);

		if (index == string::npos)
			return "";

		++index;
	}

	return name.substr(0, index - 1);
}

string Property::PopPrefix(const std::string &name) {
	const int fieldCount = CountFields(name);
	if (fieldCount < 1)
		throw runtime_error("Not enough fields in Property::PopPrefix() for: " + name);

	return ExtractPrefix(name, fieldCount -1);
}

//------------------------------------------------------------------------------
// Properties class
//------------------------------------------------------------------------------

Properties::Properties(const string &fileName) {
	SetFromFile(fileName);
}

unsigned int Properties::GetSize() const {
	return names.size();
}

Properties &Properties::Set(const Properties &props) {
	BOOST_FOREACH(const string &name, props.GetAllNames()) {
		this->Set(props.Get(name));
	}

	return *this;
}

Properties &Properties::Set(const Properties &props, const string &prefix) {
	BOOST_FOREACH(const string &name, props.GetAllNames()) {
		Set(props.Get(name).AddedNamePrefix(prefix));
	}

	return *this;	
}

Properties &Properties::SetFromStream(istream &stream) {
	string line;

	for (int lineNumber = 1;; ++lineNumber) {
		if (stream.eof())
			break;

		getline(stream, line);
		if (stream.bad())
			throw runtime_error("Error while reading from a properties stream at line " + luxrays::ToString(lineNumber));

		boost::trim(line);

		// Ignore comments
		if (line[0] == '#')
			continue;

		// Ignore empty lines
		if (line.length() == 0)
			continue;

		const size_t idx = line.find('=');
		if (idx == string::npos)
			throw runtime_error("Syntax error in a Properties at line " + luxrays::ToString(lineNumber));

		Property prop;
		prop.FromString(line);

		Set(prop);
	}

	return *this;
}

Properties &Properties::SetFromFile(const string &fileName) {
	// The use of boost::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	boost::filesystem::ifstream inFile(boost::filesystem::path(fileName),
			boost::filesystem::ifstream::in); 

	if (inFile.fail())
		throw runtime_error("Unable to open properties file: " + fileName);

	// Force to use C locale
	inFile.imbue(cLocale);

	return SetFromStream(inFile);
}

Properties &Properties::SetFromString(const string &propDefinitions) {
	istringstream stream(propDefinitions);

	// Force to use C locale
	stream.imbue(cLocale);

	return SetFromStream(stream);
}

void Properties::Save(const std::string &fileName) {
	// The use of boost::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	boost::filesystem::ofstream outFile(boost::filesystem::path(fileName),
			boost::filesystem::ofstream::trunc);
	
	// Force to use C locale
	outFile.imbue(cLocale);

	outFile << ToString();
	
	if (outFile.fail())
		throw runtime_error("Unable to save properties file: " + fileName);

	outFile.close();	
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

vector<string> Properties::GetAllUniqueSubNames(const string &prefix, const bool sorted) const {
	const size_t fieldsCount = count(prefix.begin(), prefix.end(), '.') + 2;

	set<string> definedNames;
	vector<string> namesSubset;
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0) {
			// Check if it has been already defined

			const string s = Property::ExtractPrefix(name, fieldsCount);
			if ((s.length() > 0) && (definedNames.count(s) == 0)) {
				namesSubset.push_back(s);
				definedNames.insert(s);
			}
		}
	}

	if (sorted) {
		std::sort(namesSubset.begin(), namesSubset.end(),
				[](const string &a, const string &b) -> bool{ 
			// Try to convert a and b to a number
			int aNumber = 0;
			bool validA;
			try {
				const u_int lastFieldIndex = Property::CountFields(a) - 1;
				const string lastField = Property::ExtractField(a, lastFieldIndex);
				aNumber = boost::lexical_cast<int>(lastField);
				validA = true;
			} catch(...) {
				validA = false;
			}

			int bNumber = 0;
			bool validB;
			try {
				const u_int lastFieldIndex = Property::CountFields(b) - 1;
				const string lastField = Property::ExtractField(b, lastFieldIndex);
				bNumber = boost::lexical_cast<int>(lastField);
				validB = true;
			} catch(...) {
				validB = false;
			}

			// Sort  with numbers natural order when possible
			if (validA) {
				if (validB)
					return aNumber < bNumber;
				else
					return true;
			} else {
				if (validB)
					return false;
				else
					return a < b;
			}
		});
	}

	return namesSubset;
}

bool Properties::HaveNames(const string &prefix) const {
	BOOST_FOREACH(const string &name, names) {
		if (name.find(prefix) == 0)
			return true;
	}

	return false;
}

bool Properties::HaveNamesRE(const string &regularExpression) const {
	boost::regex re(regularExpression);

	BOOST_FOREACH(const string &name, names) {
		if (boost::regex_match(name, re))
			return true;
	}

	return false;
}

Properties Properties::GetAllProperties(const string &prefix) const {
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

const Property &Properties::Get(const string &propName) const {
	std::map<string, Property>::const_iterator it = props.find(propName);
	if (it == props.end())
		throw runtime_error("Undefined property in Properties::Get(): " + propName);

	return it->second;
}

const Property &Properties::Get(const Property &prop) const {
	std::map<string, Property>::const_iterator it = props.find(prop.GetName());
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

	props.insert(pair<string, Property>(propName, prop));

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
