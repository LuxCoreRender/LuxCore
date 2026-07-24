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
#include <regex>

#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/archive/iterators/base64_from_binary.hpp>
#include <boost/archive/iterators/binary_from_base64.hpp>
#include <boost/archive/iterators/transform_width.hpp>
#include <boost/archive/iterators/ostream_iterator.hpp>

#include "luxrays/usings.h"
#include "luxrays/utils/utils.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/proputils.h"

using namespace luxrays;
using namespace std;

//------------------------------------------------------------------------------
// Blob class
//------------------------------------------------------------------------------

// TODO
//Blob::Blob(const Blob &blob) {
	//data = std::make_unique<char[]>(blob.size);
	//size = blob.size;

	//std::copy(blob.data.get(), blob.data.get() + blob.size, data.get());
//}

Blob::Blob(const char *d, const size_t s) {
	data = std::make_unique<char[]>(s);
	copy(d, d + s, data.get());

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
	data = std::make_unique<char[]>(size);
	std::copy(decoded.begin(), decoded.end(), data.get());
}

// TODO
//Blob &Blob::operator=(const Blob &blob) {

	//data = std::make_unique<char[]>(blob.size);
	//size = blob.size;

	//copy(blob.data.get(), blob.data.get() + blob.size, data.get());

	//return *this;
//}

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
	data = val;
}

PropertyValue::PropertyValue(const int val) : dataType(INT_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const unsigned int val) : dataType(UINT_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const float val) : dataType(FLOAT_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const double val) : dataType(DOUBLE_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const long long val) : dataType(LONGLONG_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const unsigned long long val) : dataType(ULONGLONG_VAL) {
	data = val;
}

PropertyValue::PropertyValue(const std::string &val) : dataType(STRING_VAL) {
	data = std::string(val);
}

PropertyValue::PropertyValue(BlobSPtr val) : dataType(BLOB_VAL) {
	data = val;
}

PropertyValue::~PropertyValue() noexcept(false) {
	switch (dataType) {
		case NONE_VAL:
			std::cerr << "Warning: None type in PropertyValue::~PropertyValue";
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
		case BLOB_VAL:
			break;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::~PropertyValue(): " + ToString(dataType));
	}
}

// Helper
template<typename T>
static auto ret(
	const PropertyValue::VariantType& data, const PropertyValue::DataType& dataType
) {
	switch (dataType) {
		case luxrays::PropertyValue::DataType::BOOL_VAL:
			return boost::lexical_cast<T>(std::get<bool>(data));
		case luxrays::PropertyValue::DataType::INT_VAL:
			return boost::lexical_cast<T>(std::get<int>(data));
		case luxrays::PropertyValue::DataType::UINT_VAL:
			return boost::lexical_cast<T>(std::get<unsigned int>(data));
		case luxrays::PropertyValue::DataType::FLOAT_VAL:
			return boost::lexical_cast<T>(std::get<float>(data));
		case luxrays::PropertyValue::DataType::DOUBLE_VAL:
			return boost::lexical_cast<T>(std::get<double>(data));
		case luxrays::PropertyValue::DataType::LONGLONG_VAL:
			return boost::lexical_cast<T>(std::get<long long>(data));
		case luxrays::PropertyValue::DataType::ULONGLONG_VAL:
			return boost::lexical_cast<T>(std::get<unsigned long long>(data));
		case luxrays::PropertyValue::DataType::STRING_VAL:
			return FromString<T>(std::get<std::string>(data));
		case luxrays::PropertyValue::DataType::BLOB_VAL:
			throw std::runtime_error("Misuse");
		default:
			throw std::runtime_error(
				"Unknown type in PropertyValue::Get<>(): " + ToString(dataType)
			);
	}
}

template<> bool PropertyValue::Get<bool>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<bool>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<bool>(): " + ToString(dataType));
	}
}

template<> int PropertyValue::Get<int>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<int>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<int>(): " + ToString(dataType));
	}
}

template<> unsigned int PropertyValue::Get<unsigned int>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<unsigned int>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<unsigned int>(): " + ToString(dataType));
	}
}

template<> float PropertyValue::Get<float>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<float>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<double>(): " + ToString(dataType));
	}
}

template<> double PropertyValue::Get<double>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<double>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<double>(): " + ToString(dataType));
	}
}

template<> long long PropertyValue::Get<long long>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<long long>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<long long>(): " + ToString(dataType));
	}
}

template<> unsigned long long PropertyValue::Get<unsigned long long>() const {
	switch (dataType) {
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
			return ret<unsigned long long>(data, dataType);
		case BLOB_VAL:
			throw std::runtime_error("A Blob property can not be converted to other types");
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Get<unsigned long long>(): " + ToString(dataType));
	}
}

template<> std::string PropertyValue::Get<std::string>() const {
	switch (dataType) {
		case BOOL_VAL:
			return ToString(std::get<bool>(data));
		case INT_VAL:
			return ToString(std::get<int>(data));
		case UINT_VAL:
			return ToString(std::get<unsigned int>(data));
		case FLOAT_VAL:
			return ToString(std::get<float>(data));
		case DOUBLE_VAL:
			return ToString(std::get<double>(data));
		case LONGLONG_VAL:
			return ToString(std::get<long long>(data));
		case ULONGLONG_VAL:
			return ToString(std::get<unsigned long long>(data));
		case STRING_VAL:
			return ToString(std::get<std::string>(data));
		case BLOB_VAL:
			return std::get<BlobSPtr>(data)->ToString();
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
			return *std::get<BlobSPtr>(data);
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
		case STRING_VAL:
		case BLOB_VAL:
			break;
		default:
			throw std::runtime_error("Unknown type in PropertyValue::Copy(): " + ToString(propVal1.dataType));
	}

	propVal1.dataType = propVal0.dataType;

	switch (propVal1.dataType) {
		case NONE_VAL:
			// Nothing to do
			break;
		case BOOL_VAL:
		case INT_VAL:
		case UINT_VAL:
		case FLOAT_VAL:
		case DOUBLE_VAL:
		case LONGLONG_VAL:
		case ULONGLONG_VAL:
		case STRING_VAL:
		case BLOB_VAL:
			propVal1.data = propVal0.data;
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

Property Property::Clone() const {
	return Property(name, values);
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


PropertyUPtr Property::Renamed(const std::string &newName) const {
	auto newProp = std::make_unique<Property>(newName);
	newProp->values.insert(newProp->values.begin(), values.begin(), values.end());

	return newProp;
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
	return UV(Get<double>(0), Get<double>(1));
}

template<> Vector Property::Get<Vector>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Vector(Get<double>(0), Get<double>(1), Get<double>(2));
}

template<> Normal Property::Get<Normal>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Normal(Get<double>(0), Get<double>(1), Get<double>(2));
}

template<> Point Property::Get<Point>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Point(Get<double>(0), Get<double>(1), Get<double>(2));
}

template<> Spectrum Property::Get<Spectrum>() const {
	if (values.size() != 3)
		throw runtime_error("Wrong number of values in property: " + name);
	return Spectrum(Get<double>(0), Get<double>(1), Get<double>(2));
}

template<> Matrix4x4 Property::Get<Matrix4x4>() const {
	if (values.size() != 16)
		throw runtime_error("Wrong number of values in property: " + name);
	return Matrix4x4(
			Get<double>(0), Get<double>(4), Get<double>(8), Get<double>(12),
			Get<double>(1), Get<double>(5), Get<double>(9), Get<double>(13),
			Get<double>(2), Get<double>(6), Get<double>(10), Get<double>(14),
			Get<double>(3), Get<double>(7), Get<double>(11), Get<double>(15));
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

Property Property::FromString(string &line) {
	Property prop;
	const size_t idx = line.find('=');
	if (idx == string::npos)
		throw runtime_error("Syntax error in property string: " + line);

	// Check if it is a valid name
	prop.name = line.substr(0, idx);
	boost::trim(prop.name);

	prop.values.clear();

	std::string value(line.substr(idx + 1));
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
					BlobSPtr blob = std::make_shared<Blob>(value.substr(first, size).c_str());
					prop.Add(blob);
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
				throw runtime_error("Unterminated blob in property: " + prop.name);
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
				throw runtime_error("Unterminated quote in property: " + prop.name);
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
	return prop;
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

// Some engine may operate concurrent accesses on Properties (eg bake engine),
// therefore this class has to be protected against race conditions (mutex).
// Beware, however, to avoid deadlocks...

Properties::Properties(Properties&& other) noexcept :
	names(std::move(other.names)),
	props(std::move(other.props)),
	mtx()  // Mutex is not movable
{}

Properties& Properties::operator=(Properties&& other) noexcept {
	if (this != &other) {
		names = std::move(other.names);
		props = std::move(props);
	}
	return *this;
}

Properties::Properties(const string &fileName) {
	SetFromFile(fileName);
}

unsigned int Properties::GetSize() const {

	std::lock_guard lk(mtx);
	return names.size();
}

void Properties::Save(const std::string &fileName) {
	// Mutex will be locked by operator <<

	// The use of std::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	std::ofstream outFile(std::filesystem::path(fileName),
			std::ofstream::trunc);

	// Force to use C locale
	outFile.imbue(cLocale);

	outFile << ToString();

	if (outFile.fail())
		throw runtime_error("Unable to save properties file: " + fileName);

	outFile.close();
}

Properties &Properties::Clear() {
	std::lock_guard lk(mtx);
	names.clear();
	props.clear();

	return *this;
}

std::vector<std::string> Properties::GetAllNames() const {
	std::lock_guard lk(mtx);
	std::vector<std::string> res;
	std::copy(names.begin(), names.end(), std::back_inserter(res));
	return res;
}

std::vector<string> Properties::GetAllNames(const string &prefix) const {
	std::lock_guard lk(mtx);
	vector<string> namesSubset;
	for(const string name: names) {
		if (name.find(prefix) == 0)
			namesSubset.push_back(name);
	}

	return namesSubset;
}

std::vector<string> Properties::GetAllNamesRE(const string &regularExpression) const {
	std::lock_guard lk(mtx);
	std::regex re(regularExpression);

	std::vector<string> namesSubset;
	for(const string name: names) {
		if (std::regex_match(name, re))
			namesSubset.push_back(name);
	}

	return namesSubset;
}

std::vector<std::string> Properties::GetAllUniqueSubNames(
	const string prefix, const bool sorted
) const {
	std::lock_guard lk(mtx);
	const size_t fieldsCount = count(prefix.begin(), prefix.end(), '.') + 2;

	std::set<std::string> definedNames;
	std::vector<std::string> namesSubset;
	for(const auto name: names) {
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
	std::lock_guard lk(mtx);
	for(const std::string& name: names) {
		if (name.find(prefix) == 0)
			return true;
	}

	return false;
}

bool Properties::HaveNamesRE(const string &regularExpression) const {
	std::lock_guard lk(mtx);
	std::regex re(regularExpression);

	for(const std::string& name: names) {
		if (std::regex_match(name, re))
			return true;
	}

	return false;
}

std::unique_ptr<Properties> Properties::GetAllProperties(const string &prefix) const {
	// Mutex is already locked in Set(Get)
	std::unique_ptr<Properties> subset = std::make_unique<Properties>();
	for(const std::string& name: names) {
		if (name.find(prefix) == 0)
			subset->Set(Get(name));
	}

	return std::move(subset);

}

bool Properties::IsDefined(const string &propName) const {
	std::lock_guard lk(mtx);
	return (props.count(propName) != 0);
}

// We return an object rather than a reference to avoid issues in concurrent
// accesses
const Property Properties::Get(const string &propName) const {
	std::lock_guard lk(mtx);
	auto it = props.find(propName);
	if (it == props.end())
		throw runtime_error("Undefined property in Properties::Get(): " + propName);

	return it->second->Clone();
}

// We return an object rather than a reference to avoid issues in concurrent
// accesses
const Property Properties::Get(const char * propName) const {
	std::lock_guard lk(mtx);
	std::string propNameStr(propName);
	auto it = props.find(propNameStr);
	if (it == props.end())
		throw runtime_error(
			std::string("Undefined property in Properties::Get(): ") + propName
		);

	return it->second->Clone();
}

// We return an object rather than a reference to avoid issues in concurrent
// accesses
const Property Properties::Get(const Property &prop) const {
	std::lock_guard lk(mtx);
	auto it = props.find(prop.GetName());
	if (it == props.end())
			return prop.Clone();

	return it->second->Clone();
}

PropertyUPtr Properties::Get(
	Property&& prop, const std::string alternativeName
) const {

	std::lock_guard lk(mtx);

	const auto& name = prop.GetName();

	// Look for the main property name
	auto it = props.find(name);
	if (it != props.end())
		return std::make_unique<Property>(it->second->Clone());  // Found

	// Look for the alternative property name
	auto itAlt = props.find(alternativeName);
	if (itAlt != props.end())
		return itAlt->second->Renamed(name);  // Found (alternate)

	// If not found, fall back to input
	return std::make_unique<Property>(std::move(prop));
}

void Properties::Delete(const string &propName) {

	std::lock_guard lk(mtx);

	vector<string>::iterator it = find(names.begin(), names.end(), propName);
	if (it != names.end())
		names.erase(it);

	props.erase(propName);
}

void Properties::DeleteAll(const vector<string> &propNames) {
	// Mutex is locked in Delete
	for(const string &n: propNames)
		Delete(n);
}

string Properties::ToString() const {
	stringstream ss;

	std::lock_guard lk(mtx);
	for (vector<string>::const_iterator i = names.begin(); i != names.end(); ++i)
		ss << props.at(*i)->ToString() << "\n";

	return ss.str();
}

// This overload version of Set is the mutex protected one and is to be used as
// a basis for the others.
Properties &Properties::Set(Property&& prop) {

	std::lock_guard lk(mtx);

	auto ptr = std::make_unique<Property>(std::move(prop));
	auto name = ptr->GetName();  // Copy

	auto [it, result] = props.insert_or_assign(name, std::move(ptr));
	if (result) names.push_back(name);  // It's a new name

	return std::ref(*this);
}

Properties &Properties::Set(const Property &prop) {
	return Set(prop.Clone());
}

Properties &Properties::Set(PropertyRPtr prop) {
	return Set(prop->Clone());
}

Properties &Properties::operator<<(const Property &prop) {
	return Set(prop.Clone());
}

Properties &Properties::operator<<(Property&& prop) {
	return Set(prop);
}

Properties &Properties::Set(const Properties &props) {
	for(const std::string name: props.GetAllNames()) {
		this->Set(std::move(props.Get(name)));
	}

	return std::ref(*this);
}

Properties &Properties::Set(const PropertiesUPtr &props) {
	for(const string name: props->GetAllNames()) {
		this->Set(std::move(props->Get(name)));
	}

	return std::ref(*this);
}

Properties &Properties::Set(const Properties &props, const string &prefix) {
	for(const string name: props.GetAllNames()) {
		Set(props.Get(name).AddedNamePrefix(prefix));
	}

	return std::ref(*this);
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

		Set(Property::FromString(line));

	}

	return std::ref(*this);
}

Properties &Properties::SetFromFile(const string &fileName) {
	// The use of std::filesystem::path is required for UNICODE support: fileName
	// is supposed to be UTF-8 encoded.
	std::ifstream inFile(std::filesystem::path(fileName),
			std::ifstream::in); 

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

Properties &Properties::operator<<(const Properties &props) {
	return Set(props);
}

Properties &Properties::operator<<(const std::unique_ptr<Properties> &props) {
	return Set(*props);
}

PropertiesUPtr Properties::Clone() const {

	std::lock_guard lk(mtx);

	auto result = std::make_unique<Properties>();

	// Property map
	decltype(props) copiedMap;

	for (const auto& [key, value] : props) {
		copiedMap[key] = std::make_unique<Property>(value->Clone());
	}

	result->props = std::move(copiedMap);

	// Name vector
	result->names = names;  // std::vector and std::string should make a deep
						   // copy out of the box

	return result;
}


Properties &luxrays::operator<<(const Property &prop0, const Property &prop1) {
	PropertiesUPtr props = std::make_unique<Properties>();
	*props << prop0 << prop1;
	return *props;
}

Properties &luxrays::operator<<(const Property &prop0, const Properties &props) {
	PropertiesUPtr res = std::make_unique<Properties>();
	*res << prop0 << props;
	return *res;
}
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
