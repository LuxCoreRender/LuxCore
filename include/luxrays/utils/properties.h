/***************************************************************************
 * Copyright 1998-2015 by authors (see AUTHORS.txt)                        *
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
#include <boost/foreach.hpp>
#include <boost/serialization/vector.hpp>

#include "luxrays/luxrays.h"
#include <luxrays/utils/properties.h>
#include <luxrays/core/geometry/uv.h>
#include <luxrays/core/geometry/vector.h>
#include <luxrays/core/geometry/normal.h>
#include <luxrays/core/geometry/point.h>
#include <luxrays/core/geometry/matrix4x4.h>
#include <luxrays/core/color/color.h>

namespace luxrays {

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

/*!
 * \brief Value that can be stored in a Property.
 *
 * The current list of allowed data types is:
 * - bool
 * - int
 * - u_int
 * - float
 * - double
 * - u_longlong
 * - string
 */
typedef boost::variant<bool, int, u_int, float, double, u_longlong, std::string> PropertyValue;

/*!
 * \brief A vector of values that can be stored in a Property.
 */
typedef std::vector<PropertyValue> PropertyValues;

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
	 * Constructs a new property where the property name is initialized to
	 * \p propName and the vector of values has one single element with the value
	 * of \p val.
	 *
	 * \param propName is the name of the new property.
	 * \param val is the value of the new property.
	 */
	Property(const std::string &propName, const PropertyValue &val);
	/*!
	 * \brief Constructs a new property with a given name and values.
	 *
	 * Constructs a new property where the property name is initialized to
	 * \p propName and the vector of values is initialize with the values
	 * of \p vals.
	 *
	 * \param propName is the name of the new property.
	 * \param vals is the value of the new property.
	 */
	Property(const std::string &propName, const PropertyValues &vals);
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
	 * \brief Return a new property with a new name.
	 * 
	 * \param prefix is the string to use for the new name.
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
	template<class T> T Get(const u_int index) const {
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
	 * - u_int
	 * - float
	 * - double
	 * - u_longlong
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
	 * > Property("test1.prop1")(1.f, 2.f, 3.f, 4.f)
	 * 
	 * \param val0 is the value to assign as first item.
	 * \param val1 is the value to assign as second item.
	 * \param val2 is the value to assign as third item.
	 * \param val3 is the value to assign as forth item.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0, class T1, class T2, class T3> Property &operator()(const T0 &val0, const T1 &val1, const T2 &val2, const T3 &val3) {
		return Add(val0).Add(val1).Add(val2).Add(val3);
	}
	/*!
	 * \brief Adds a vector of values to a property.
	 * 
	 * \param vals is the value to assign.
	 * 
	 * \return a reference to the modified property.
	 */
	template<class T0> Property &operator()(const std::vector<T0> &vals) {
		BOOST_FOREACH(T0 v, vals)
			values.push_back(v);

		return *this; 
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
	Property &Add(const char *val) {
		values.push_back(std::string(val));
		return *this;
	}
	/*!
	 * \brief Required to work around the problem of char* to bool conversion
	 * (instead of char* to string).
	 */
	Property &Add(char *val) {
		values.push_back(std::string(val));
		return *this;
	}
	Property &operator()(const char *val) {
		return Add(std::string(val));
	}
	/*!
	 * \brief Required to work around the problem of char* to bool conversion
	 * (instead of char* to string).
	 */
	Property &operator()(char *val) {
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
	/*!
	 * \brief Required to work around the problem of char* to bool conversion
	 * (instead of char* to string).
	 */
	Property &operator=(char *val) {
		values.clear();
		return Add(std::string(val));
	}

	static std::string ExtractField(const std::string &name, const u_int index);
	static std::string ExtractPrefix(const std::string &name, const u_int count);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & name;
		ar & values;
	}

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

		T operator()(const u_longlong v) const {
			return boost::lexical_cast<T>(v);
		}

		T operator()(const std::string &v) const {
			return boost::lexical_cast<T>(v);
		}
	};

	std::string name;
	PropertyValues values;
};	

// Get basic types
template<> bool Property::Get<bool>() const;
template<> int Property::Get<int>() const;
template<> u_int Property::Get<u_int>() const;
template<> float Property::Get<float>() const;
template<> double Property::Get<double>() const;
template<> u_longlong Property::Get<u_longlong>() const;
template<> std::string Property::Get<std::string>() const;
// Get LuxRays types
template<> UV Property::Get<UV>() const;
template<> Vector Property::Get<Vector>() const;
template<> Normal Property::Get<Normal>() const;
template<> Point Property::Get<Point>() const;
template<> Spectrum Property::Get<Spectrum>() const;
template<> Matrix4x4 Property::Get<Matrix4x4>() const;

// Add LuxRays types
template<> Property &Property::Add<UV>(const UV &val);
template<> Property &Property::Add<Vector>(const Vector &val);
template<> Property &Property::Add<Normal>(const Normal &val);
template<> Property &Property::Add<Point>(const Point &val);
template<> Property &Property::Add<Spectrum>(const Spectrum &val);
template<> Property &Property::Add<Matrix4x4>(const Matrix4x4 &val);

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
	 * \brief Returns the number of Property in this container.
	 *
	 * \return the number of Property.
	 */
	u_int GetSize() const;

	// The following 2 methods perform the same action

	/*!
	 * \brief Sets a single Property.
	 * 
	 * \param prop is the Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(const Property &prop);
	/*!
	 * \brief Sets a single Property.
	 * 
	 * \param prop is the Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &operator<<(const Property &prop);
	/*!
	 * \brief Sets the list of Property.
	 * 
	 * \param prop is the list of Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(const Properties &prop);
	/*!
	 * \brief Sets the list of Property.
	 * 
	 * \param props is the list of Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &operator<<(const Properties &props);
	/*!
	 * \brief Sets the list of Property while adding a prefix to all names.
	 * 
	 * \param props is the list of Property to set.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Set(const Properties &props, const std::string &prefix);
	/*!
	 * \brief Sets the list of Property coming from a stream.
	 * 
	 * \param stream is the input stream to read.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &SetFromStream(std::istream &stream);
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
	 * This method is used to iterate over all properties.
	 * 
	 * \param prefix of the Property names to return.
	 *
	 * \return a vector of Property names.
	 */
	std::vector<std::string> GetAllNames(const std::string &prefix) const;
	/*!
	 * \brief Returns all Property unique names that match the passed regular
	 * expression.
	 *
	 * \param regularExpression to use for the pattern matching.
	 *
	 * \return a vector of Property names.
	 */
	std::vector<std::string> GetAllNamesRE(const std::string &regularExpression) const;
	/*!
	 * \brief Returns all Property unique names that start with a specific prefix.
	 *
	 * For instance, given the the following names:
	 * - test.prop1.subprop1
	 * - test.prop1.subprop2
	 * - test.prop2.subprop1
	 * 
	 * GetAllUniqueSubNames("test") will return:
	 * - test.prop1
	 * - test.prop2
	 *
	 * \param prefix of the Property names to return.
	 *
	 * \return a vector of Property names.
	 */
	std::vector<std::string> GetAllUniqueSubNames(const std::string &prefix) const;
	/*!
	 * \brief Returns if there are at least a Property starting for specific prefix.
	 *
	 * \param prefix of the Property to look for.
	 *
	 * \return true if there is at least on Property starting for the prefix.
	 */
	bool HaveNames(const std::string &prefix) const;
	/*!
	 * \brief Returns all a copy of all Property with a name starting with a specific prefix.
	 * 
	 * \param prefix of the Property names to use.
	 *
	 * \return a copy of all Property matching the prefix.
	 */
	bool HaveNamesRE(const std::string &regularExpression) const;
	/*!
	 * \brief Returns all a copy of all Property with a name matching the passed
	 * regular expression.
	 * 
	 * \param regularExpression to use for the pattern matching.
	 *
	 * \return a copy of all Property matching the prefix.
	 */
	Properties GetAllProperties(const std::string &prefix) const;
	/*!
	 * \brief Returns a property.
	 *
	 * \param propName is the name of the Property to return.
	 *
	 * \return a Property.
	 *
	 * \throws std::runtime_error if the Property doesn't exist.
	 */
	const Property &Get(const std::string &propName) const;
	/*!
	 * \brief Returns a Property with the same name of the passed Property if
	 * it has been defined or the passed Property itself (i.e. the default values).
	 *
	 * \param defaultProp has the Property to look for and the default values in
	 * case it has not been defined.
	 *
	 * \return a Property.
	 */
	const Property &Get(const Property &defaultProp) const;

	/*!
	 * \brief Returns if a Property with the given name has been defined.
	 *
	 * \param propName is the name of the Property to check.
	 *
	 * \return if defined or not.
	 */
	bool IsDefined(const std::string &propName) const;

	/*!
	 * \brief Deletes a Property with the given name.
	 *
	 * \param propName is the name of the Property to delete.
	 */
	void Delete(const std::string &propName);
	/*!
	 * \brief Deletes all listed Property.
	 *
	 * \param propNames is the list of the Property to delete.
	 */
	void DeleteAll(const std::vector<std::string> &propNames);

	/*!
	 * \brief Converts all Properties in a string.
	 *
	 * \return a string with the definition of all properties.
	 */
	std::string ToString() const;

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & names;
		ar & props;
	}

	// This vector used, among other things, to keep track of the insertion order
	std::vector<std::string> names;
	boost::unordered_map<std::string, Property> props;
};

Properties operator<<(const Property &prop0, const Property &prop1);
Properties operator<<(const Property &prop0, const Properties &props);

inline std::ostream &operator<<(std::ostream &os, const Properties &p) {
	os << p.ToString();

	return os;
}

}

BOOST_CLASS_VERSION(luxrays::Property, 1)
BOOST_CLASS_VERSION(luxrays::Properties, 1)

#endif	/* _LUXRAYS_PROPERTIES_H */
