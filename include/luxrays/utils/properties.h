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

#ifndef _LUXRAYS_PROPERTIES_H
#define	_LUXRAYS_PROPERTIES_H

#include <map>
#include <vector>
#include <string>
#include <istream>
#include <cstdarg>

#include <boost/lexical_cast.hpp>
#include <boost/variant.hpp>
#include <boost/unordered_map.hpp>
#include <boost/type_traits.hpp>

#include "luxrays/luxrays.h"
#include <luxrays/utils/properties.h>
#include <luxrays/core/geometry/uv.h>
#include <luxrays/core/geometry/vector.h>
#include <luxrays/core/geometry/normal.h>
#include <luxrays/core/geometry/point.h>
#include <luxrays/core/geometry/matrix4x4.h>
#include <luxrays/core/spectrum.h>

namespace luxrays {

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

/*!
 * \brief Values that can be stored in a Property.
 *
 * The current list of allowed data types is:
 * - bool
 * - int
 * - unsigned int
 * - float
 * - double
 * - size_t
 * - string
 */
typedef boost::variant<bool, int, u_int, float, double, size_t, std::string> PropertyValue;

/*!
 * \brief A generic container for values.
 *
 * A Property is a container associating a vector of values to a string name. The
 * vector of values can include items with different data types. Check
 * \ref PropertyValue for a list of allowed types.
 */
class Property {
public:
	/*!
	 * \brief Constructs a new empty property.
	 *
	 * Constructs a new empty property where the property name is initialized to
	 * the empty string (i.e. "") and the vector of values is empty too.
	 */
	Property();
	/*!
	 * \brief Constructs a new empty property with a given name.
	 *
	 * Constructs a new empty property where the property name is initialized
	 * to \p propName and the vector of values is empty too.
	 *
	 * \param propName is the name of the new property.
	 */
	Property(const std::string &propName);
	/*!
	 * \brief Constructs a new property with a given name and value.
	 *
	 * Constructs a new empty property where the property name is initialized to
	 * \p propName and the vector of values has one single element with the value
	 * of \p val.
	 *
	 * \param propName is the name of the new property.
	 * \param val is the value of the new property.
	 */
	Property(const std::string &propName, const PropertyValue &val);
	~Property();

	/*!
	 * \brief Returns the name of a property.
	 *
	 * \return the name of the property
	 */
	const std::string &GetName() const { return name; }
	/*!
	 * \brief Return a new property with a prefix added to the name.
	 * 
	 * \param prefix is the string to add to the name.
	 *
	 * \return a new property.
	 */
	Property AddedNamePrefix(const std::string &prefix) const {
		Property newProp(prefix + name);
		newProp.values.insert(newProp.values.begin(), values.begin(), values.end());

		return newProp;
	}
	/*!
	 * \brief Return a new property with a prefix added to the name.
	 * 
	 * \param prefix is the string to add to the name.
	 *
	 * \return a new property.
	 */
	Property Renamed(const std::string &newName) const {
		Property newProp(newName);
		newProp.values.insert(newProp.values.begin(), values.begin(), values.end());

		return newProp;
	}
	/*!
	 * \brief Returns the number of values associated to this property.
	 *
	 * \return the number of values in this property.
	 */
	u_int GetSize() const { return values.size(); }
	/*!
	 * \brief Removes any values associated to the property.
	 *
	 * \return a reference to the modified property.
	 */
	Property &Clear();
	/*!
	 * \brief Returns the value at the specified position.
	 * 
	 * \param index is the position of the value to return.
	 * 
	 * \return the value at specified position (casted or translated to the type
	 * required).
	 * 
	 * \throws std::runtime_error if the index is out of bound.
	 */
	template<class T> T GetValue(const u_int index) const {
		if (index >= values.size())
			throw std::runtime_error("Out of bound error for property: " + name);

		return boost::apply_visitor(GetValueVistor<T>(), values[index]);
	}
	/*!
	 * \brief Returns the type of the value at the specified position.
	 * 
	 * \param index is the position of the value.
	 * 
	 * \return the type information of the value at specified position.
	 * 
	 * \throws std::runtime_error if the index is out of bound.
	 */
	const std::type_info &GetValueType(const u_int index) const {
		if (index >= values.size())
			throw std::runtime_error("Out of bound error for property: " + name);

		return values[index].type();
	}
	/*!
	 * \brief Parses all values as a representation of the specified type.
	 *
	 * For instance, The values "0.5, 0.5, 0.5" can be parsed as a luxrays::Vector,
	 * luxrays::Normal, etc. The current list of supported data types is:
	 * - bool
	 * - int
	 * - unsigned int
	 * - float
	 * - double
	 * - size_t
	 * - string
	 * - luxrays::UV
	 * - luxrays::Vector
	 * - luxrays::Normal
	 * - luxrays::Point
	 * - luxrays::Matrix4x4
	 * 
	 * \return the value at specified position (casted or translated to the type
	 * required).
	 * 
	 * \throws std::runtime_error if the property has the wrong number of values
	 * for the specified data type.
	 */
	template<class T> T Get() const {
		throw std::runtime_error("Unsupported data type in property: " + name);
	}

	/*!
	 * \brief Sets the value at the specified position.
	 * 
	 * \param index is the position of the value to set.
	 * \param val is the new value to set.
	 *
	 * \return a reference to the modified property.
	 *
	 * \throws std::runtime_error if the index is out of bound.
	 */
	template<class T> Property &Set(const u_int index, const T &val) {
		if (index >= values.size())
			throw std::runtime_error("Out of bound error for property: " + name);

		values[index] = val;

		return *this;
	}
	/*!
	 * \brief Adds an item at the end of the list of values associated with the
	 * property.
	 * 
	 * \param val is the value to append.
	 *
	 * \return a reference to the modified property.
	 */
	template<class T> Property &Add(const T &val) {
		values.push_back(val);
		return *this;
	}
	/*!
	 * \brief Returns a string with all values associated to the property.
	 * 
	 * \return a string with all values.
	 */
	std::string GetValuesString() const;
	/*!
	 * \brief Returns a string with the name of the property followed by " = "
	 * and by all values associated to the property.
	 * 
	 * \return a string with the name and all values.
	 */
	std::string ToString() const;
	/*!
	 * \brief Adds a value to a property.
	 *
	 * It can be used to write expressions like:
	 * 
	 * > Property("test1.prop1")("aa")
	 * 
	 * \param val0 is the value to assign.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0> Property &operator()(const T0 &val0) {
		return Add(val0);
	}
	/*!
	 * \brief Adds a value to a property.
	 *
	 * It can be used to write expressions like:
	 * 
	 * > Property("test1.prop1")(1.f, 2.f)
	 * 
	 * \param val0 is the value to assign as first item.
	 * \param val1 is the value to assign as second item.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0, class T1> Property &operator()(const T0 &val0, const T1 &val1) {
		return Add(val0).Add(val1);
	}
	/*!
	 * \brief Adds a value to a property.
	 *
	 * It can be used to write expressions like:
	 * 
	 * > Property("test1.prop1")(1.f, 2.f, 3.f)
	 * 
	 * \param val0 is the value to assign as first item.
	 * \param val1 is the value to assign as second item.
	 * \param val2 is the value to assign as third item.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0, class T1, class T2> Property &operator()(const T0 &val0, const T1 &val1, const T2 &val2) {
		return Add(val0).Add(val1).Add(val2);
	}
	/*!
	 * \brief Adds a value to a property.
	 *
	 * It can be used to write expressions like:
	 * 
	 * > Property("test1.prop1")(1.f, 2.f, 3.f, 4.f, 5.f, 6.f)
	 * 
	 * \param val0 is the value to assign as first item.
	 * \param val1 is the value to assign as second item.
	 * \param val2 is the value to assign as third item.
	 * \param val3 is the value to assign as third item.
	 * \param val4 is the value to assign as third item.
	 * \param val5 is the value to assign as third item.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0, class T1, class T2, class T3, class T4, class T5> Property &operator()(const T0 &val0, const T1 &val1, const T2 &val2, const T3 &val3, const T4 &val4, const T5 &val5) {
		return Add(val0).Add(val1).Add(val2).Add(val3).Add(val4).Add(val5);
	}
	/*!
	 * \brief Initializes a property with (only) the given value.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T> Property &operator=(const T &val) {
		values.clear();
		return Add(val);
	}
	/*!
	 * \brief Required to work around the problem of char* to bool conversion
	 * (instead of char* to string).
	 */
	Property &operator()(const char *val) {
		return Add(std::string(val));
	}
	/*!
	 * \brief Required to work around the problem of char* to bool conversion
	 * (instead of char* to string).
	 */
	Property &operator=(const char *val) {
		values.clear();
		return Add(std::string(val));
	}

private:
	template<class T> class GetValueVistor : public boost::static_visitor<T> {
	public:
		T operator()(const bool v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const int v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const u_int v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const float v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const double v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const size_t v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const std::string &v) const {
			return boost::lexical_cast<T>(v);
		}
	};

	const std::string name;
	std::vector<PropertyValue> values;
};	

// Basic types
template<> bool Property::Get<bool>() const;
template<> int Property::Get<int>() const;
template<> u_int Property::Get<u_int>() const;
template<> float Property::Get<float>() const;
template<> double Property::Get<double>() const;
template<> size_t Property::Get<size_t>() const;
template<> std::string Property::Get<std::string>() const;
// LuxRays types
template<> luxrays::UV Property::Get<luxrays::UV>() const;
template<> luxrays::Vector Property::Get<luxrays::Vector>() const;
template<> luxrays::Normal Property::Get<luxrays::Normal>() const;
template<> luxrays::Point Property::Get<luxrays::Point>() const;
template<> luxrays::Spectrum Property::Get<luxrays::Spectrum>() const;
template<> luxrays::Matrix4x4 Property::Get<luxrays::Matrix4x4>() const;

inline std::ostream &operator<<(std::ostream &os, const Property &p) {
	os << p.ToString();

	return os;
}

//------------------------------------------------------------------------------
// Properties class
//------------------------------------------------------------------------------

/*!
 * \brief A container for multiple Property.
 *
 * Properties is a container for instances of Property class. It keeps also
 * track of the insertion order.
 */
class Properties {
public:
	Properties() { }
	/*!
	 * \brief Sets the list of Property from a text file .
	 * 
	 * \param fileName is the name of the file to read.
	 */
	Properties(const std::string &fileName);
	~Properties() { }

	/*!
	 * \brief Sets the list of Property.
	 * 
	 * \param prop is the list of Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(const Properties &prop);
	/*!
	 * \brief Sets the list of Property while adding a prefix to all names .
	 * 
	 * \param prop is the list of Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(const Properties &prop, const std::string prefix);
	/*!
	 * \brief Sets the list of Property coming from a stream.
	 * 
	 * \param stream is the input stream to read.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(std::istream &stream);
	/*!
	 * \brief Sets the list of Property coming from a file.
	 * 
	 * \param fileName is the name of the file to read.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &SetFromFile(const std::string &fileName);
	/*!
	 * \brief Sets the list of Property coming from a std::string.
	 * 
	 * \param propDefinitions is the list of Property to add in text format.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &SetFromString(const std::string &propDefinitions);

	/*!
	 * \brief Removes all Property from the container.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Clear();

	/*!
	 * \brief Returns all Property names defined.
	 * 
	 * \return a reference to all Property names defined.
	 */
	const std::vector<std::string> &GetAllNames() const;
	/*!
	 * \brief Returns all Property names that start with a specific prefix.
	 *
	 * \param prefix is the prefix Property names must have to be included in
	 * in the result.
	 *
	 * \return a vector of Property names.
	 */
	std::vector<std::string> GetAllNames(const std::string &prefix) const;

	bool IsDefined(const std::string &propName) const;
	const Property &Get(const std::string &propName) const;
	void Delete(const std::string &propName);

	// The following 2 methods perform the same action
	Properties &Set(const Property &prop);
	Properties &operator<<(const Property &prop);
	
	Properties &operator<<(const Properties &props);

	std::string ToString() const;

	static std::string ExtractField(const std::string &value, const size_t index);

	//--------------------------------------------------------------------------
	// Old deprecated interface
	//--------------------------------------------------------------------------

	std::string GetString(const std::string &propName, const std::string defaultValue) const;
	bool GetBoolean(const std::string &propName, const bool defaultValue) const;
	int GetInt(const std::string &propName, const int defaultValue) const;
	size_t GetSize(const std::string &propName, const size_t defaultValue) const;
	float GetFloat(const std::string &propName, const float defaultValue) const;

	std::vector<std::string> GetStringVector(const std::string &propName, const std::string &defaultValue) const;
	std::vector<int> GetIntVector(const std::string &propName, const std::string &defaultValue) const;
	std::vector<float> GetFloatVector(const std::string &propName, const std::string &defaultValue) const;

	void SetString(const std::string &propName, const std::string &value);
	std::string SetString(const std::string &property);

	static std::vector<std::string> ConvertToStringVector(const std::string &values);
	static std::vector<int> ConvertToIntVector(const std::string &values);
	static std::vector<float> ConvertToFloatVector(const std::string &values);

private:
	// This vector used, among other things, to keep track of the insertion order
	std::vector<std::string> names;
	boost::unordered_map<std::string, Property> props;
};

Properties operator<<(const Property &prop0, const Property &prop1);

inline std::ostream &operator<<(std::ostream &os, const Properties &p) {
	os << p.ToString();

	return os;
}

}

#endif	/* _LUXRAYS_PROPERTIES_H */
