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

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include "luxrays/core/namedobject.h"
#include "luxrays/utils/strutils.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// NamedObject
//------------------------------------------------------------------------------

NamedObject::NamedObject() {
	name = "NamedObject";
}

NamedObject::NamedObject(const string &nm) : name(nm) {
}

NamedObject::~NamedObject() {
}

Properties NamedObject::ToProperties() const {
	throw runtime_error("Named object \"" + name + "\" doesn't implement ToProperties() method");
}

string NamedObject::GetUniqueName(const string &prefix) {
	return prefix + "-" + ToString(boost::uuids::random_generator()());
}
