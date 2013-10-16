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

#include "luxrays/luxrays.h"

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

		return boost::apply_visitor(GetVistor<T>(), values[index]);
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
	template<class T> class GetVistor : public boost::static_visitor<T> {
	public:
		T operator()(const bool i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const int i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const u_int i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const float i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const double i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const size_t i) const {
			return boost::lexical_cast<T>(i);
		}

		T operator()(const std::string &i) const {
			return boost::lexical_cast<T>(i);
		}
	};
	
	const std::string name;
	std::vector<PropertyValue> values;
};	

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
	 * \brief Loads from a text file the list of Property.
	 * 
	 * \param fileName is the name of the file to read.
	 */
	Properties(const std::string &fileName);
	~Properties() { }

	/*!
	 * \brief Adds the list of Property to the current one.
	 * 
	 * \param prop is the list of Property to add.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Load(const Properties &prop);
	/*!
	 * \brief Adds the list of Property to the current one.
	 * 
	 * \param stream is the input stream to read.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Load(std::istream &stream);
	/*!
	 * \brief Adds the list of Property to the current one.
	 * 
	 * \param fileName is the name of the file to read.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &LoadFromFile(const std::string &fileName);
	/*!
	 * \brief Adds the list of Property to the current one.
	 * 
	 * \param propDefinitions is the list of Property to add in text format.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &LoadFromString(const std::string &propDefinitions);

	/*!
	 * \brief Removes all Property from the container.
	 * 
	 * \return a reference to the modified properties.
	 */
	Properties &Clear();

	/*!
	 * \brief Returns all Property keys defined.
	 * 
	 * \return a reference to all Property keys defined.
	 */
	const std::vector<std::string> &GetAllKeys() const;
	/*!
	 * \brief Returns all Property keys that start with a specific prefix.
	 *
	 * \param prefix is the prefix Property keys must have to be included in
	 * in the result.
	 *
	 * \return a vector of Property keys.
	 */
	std::vector<std::string> GetAllKeys(const std::string &prefix) const;

	bool IsDefined(const std::string &propName) const;
	const Property &Get(const std::string &propName) const;
	void Delete(const std::string &propName);

	// The following 2 methods perform the same action
	Properties &Set(const Property &prop);
	Properties &operator<<(const Property &prop);

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
	std::vector<std::string> keys;
	boost::unordered_map<std::string, Property> props;
};

Properties operator<<(const Property &prop0, const Property &prop1);

inline std::ostream &operator<<(std::ostream &os, const Properties &p) {
	os << p.ToString();

	return os;
}

}

#endif	/* _LUXRAYS_PROPERTIES_H */
