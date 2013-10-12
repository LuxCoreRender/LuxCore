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

/*!
 * \file
 *
 * \brief LuxCore is new the LuxRender C++/Python API.
 * \author Bucciarelli David et al.
 * \version 1.0
 * \date October 2013
 *
 */

/*!
 * \mainpage
 * \section intro Introduction
 * LuxCore is the new LuxRender C++/Python API. It can be used to create and
 * render scenes. It includes the support for advanced new features like editing
 * materials, lights, geometry, interactive rendering and more.
 */

#ifndef _LUXCORE_H
#define	_LUXCORE_H

#include <cstddef>
#include <string>

#include <boost/variant.hpp>

#include <luxrays/utils/properties.h>

/*!
 * \namespace luxcore
 *
 * \brief The LuxCore classes are defined within this namespace.
 */
namespace luxcore {

//typedef enum {
//	SLG
//	// Soon (TM)
//	// LUXRENDER
//} SDLType;
//
//typedef enum {
//	PATHOCL,
//	LIGHTCPU,
//	PATHCPU,
//	BIDIRCPU,
//	BIDIRVMCPU,
//	FILESAVER,
//	RTPATHOCL,
//	BIASPATHCPU,
//	BIASPATHOCL,
//	RTBIASPATHOCL
//} RenderEngineType;

/*!
 * \brief Values that can be stored in a Property
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
 * \brief A generic container for values
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
	 *  of \p val.
	 *
	 * \param propName is the name of the new property.
	 * \param val is the value of the new property.
	 */
	Property(const std::string &propName, const PropertyValue &val);
	~Property();

	const std::string &GetName() const;
	u_int GetSize() const;

	void Clear();

	template<class T> T Get(const u_int index) const;
	std::string GetString() const;

	std::string ToString() const;

	template<class T> Property &operator,(T val) {
		prop, val;
		return *this;
	}

	template<class T> Property &operator=(T val) {
		prop = val;
		return *this;
	}

	// Required to work around the problem of char* to bool conversion
	// (instead of char* to string)
	Property &operator,(const char *val) {
		prop, val;
		return *this;
	}

	Property &operator=(const char *val) {
		prop = val;
		return *this;
	}

	friend class Properties;

private:
	luxrays::Property prop;
};

inline std::ostream &operator<<(std::ostream &os, const Property &p) {
	os << p;

	return os;
}

class Properties {
public:
	Properties();
	Properties(const std::string &fileName);
	~Properties();

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

private:
	luxrays::Properties props;
};

Properties operator%(const Property &prop0, const Property &prop1);

inline std::ostream &operator<<(std::ostream &os, const Properties &p) {
	os << p.ToString();

	return os;
}

//class RenderConfig {
//public:
//	RenderConfig(const SDLType sdlType, const std::string &fileName,
//			const Properties *additionalProperties = NULL) {
//		Initialize(sdlType, fileName, additionalProperties);
//	}
////	RenderConfig(const luxrays::Properties &props, Scene &scene) {
////		Initialize(props, scene);
////	}
//	~RenderConfig();
//
//	void SetScreenRefreshInterval(const u_int t);
//	u_int GetScreenRefreshInterval() const;
//
//private:
//	void Initialize(const SDLType &sdlType, const std::string &fileName,
//			const Properties *additionalProperties = NULL);
////	void Initialize(const luxrays::Properties &props, Scene &scene);
//};
//
//class RenderSession {
//public:
//	RenderSession(const RenderConfig &cfg) { Initialize(cfg); }
//	~RenderSession();
//
//	void Start();
//	void Stop();
//
//	void BeginEdit();
//	void EndEdit();
//
//	void SetRenderingEngineType(const RenderEngineType engineType);
//
//private:
//	void Initialize(const RenderConfig &cfg);
//};

}

#endif
