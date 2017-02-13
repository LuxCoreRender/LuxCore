/***************************************************************************
 * Copyright 1998-2017 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_NAMEDOBJECT_H
#define	_SLG_NAMEDOBJECT_H

#include <string>
#include <boost/serialization/version.hpp>
#include <boost/serialization/base_object.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/access.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "luxrays/luxrays.h"
#include "luxrays/utils/properties.h"

namespace slg {

class NamedObject {
public:
	NamedObject();
	NamedObject(const std::string &name);
	virtual ~NamedObject();

	const std::string &GetName() const { return name; }
	void SetName(const std::string &nm) { name = nm; }

	// Returns the Properties required to create this object
	virtual luxrays::Properties ToProperties() const;

	// Most sub-class will implement the many standard static methods used
	// in ObjectStaticRegistry

	friend class boost::serialization::access;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & name;
	}

	std::string name;
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::NamedObject)

BOOST_CLASS_VERSION(slg::NamedObject, 1)

#endif	/* _SLG_NAMEDOBJECT_H */
