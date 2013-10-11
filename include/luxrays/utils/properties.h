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

#include <boost/lexical_cast.hpp>
#include <boost/variant.hpp>
#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"

namespace luxrays {

//------------------------------------------------------------------------------
// Property class
//------------------------------------------------------------------------------

typedef boost::variant<bool, int, u_int, float, double, size_t, std::string> PropertyValue;

class Property {
public:
	Property() { }
	Property(const std::string &propName);
	Property(const std::string &propName, const PropertyValue &val);
	~Property();

	const std::string &GetName() const { return name; }
	u_int GetSize() const { return values.size(); }

	void Clear();

	template<class T> T Get(const u_int index) const;
	std::string GetString() const;

	std::string ToString() const;

	template<class T> Property &operator,(T val) {
		values.push_back(val);
		return *this;
	}

	template<class T> Property &operator=(T val) {
		values.clear();
		values.push_back(val);
		return *this;
	}

	// Required to work around the problem of char* to bool conversion
	// (instead of char* to string)
	Property &operator,(const char *val) {
		values.push_back(std::string(val));
		return *this;
	}

	Property &operator=(const char *val) {
		values.clear();
		values.push_back(std::string(val));
		return *this;
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

class Properties {
public:
	Properties() { }
	Properties(const std::string &fileName);
	~Properties() { }

	void Load(const Properties &prop);
	void Load(std::istream &stream);
	void LoadFromFile(const std::string &fileName);
	void LoadFromString(const std::string &propDefinitions);

	void Clear();
	const std::vector<std::string> &GetAllKeys() const;
	std::vector<std::string> GetAllKeys(const std::string prefix) const;

	bool IsDefined(const std::string &propName) const;
	void Delete(const std::string &propName);

	// The following methods perform the same action
	Properties &Set(const Property &prop);
	Properties &operator%=(const Property &prop);

	Properties &operator%(const Property &prop);

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

Properties operator%(const Property &prop0, const Property &prop1);

inline std::ostream &operator<<(std::ostream &os, const Properties &p) {
	os << p.ToString();

	return os;
}

}

#endif	/* _LUXRAYS_PROPERTIES_H */
