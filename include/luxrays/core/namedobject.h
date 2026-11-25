/***************************************************************************
 * Copyright 1998-2026 by authors (see AUTHORS.txt)                        *
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

#ifndef _LUXRAYS_NAMEDOBJECT_H
#define	_LUXRAYS_NAMEDOBJECT_H

#include <string>
#include <iostream>
#include <type_traits>

#include "luxrays/luxrays.h"
#include "luxrays/usings.h"
#include "luxrays/utils/properties.h"
#include "luxrays/utils/serializationutils.h"

namespace luxrays {

class NamedObject {
public:
	NamedObject();
	NamedObject(const std::string &name);
	virtual ~NamedObject();

	// Delete copy operations (if you don't want copying)
	NamedObject(const NamedObject&) = delete;
	NamedObject& operator=(const NamedObject&) = delete;

	// Declare default move operations
	NamedObject(NamedObject&&) = default;
	NamedObject& operator=(NamedObject&&) = default;

	const std::string &GetName() const { return name; }
	void SetName(const std::string &nm) { name = nm; }

	// Returns the Properties required to create this object
	virtual luxrays::PropertiesUPtr ToProperties() const;

	// Most sub-class will implement the many standard static methods used
	// in ObjectStaticRegistry

	static std::string GetUniqueName(const std::string &prefix);

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & name;
	}

	std::string name;
};


inline bool operator==(const NamedObject& lhs, const NamedObject& rhs) {
	return std::addressof(lhs) == std::addressof(rhs);
}

inline bool operator!=(const NamedObject& lhs, const NamedObject& rhs) {
	return not (lhs == rhs);
}

}  // namespace luxrays

BOOST_SERIALIZATION_ASSUME_ABSTRACT(luxrays::NamedObject)

BOOST_CLASS_VERSION(luxrays::NamedObject, 3)

#endif	/* _LUXRAYS_NAMEDOBJECT_H */
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
