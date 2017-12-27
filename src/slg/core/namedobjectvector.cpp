/***************************************************************************
 * Copyright 1998-2018 by authors (see AUTHORS.txt)                        *
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

#include <boost/foreach.hpp>

#include "slg/core/namedobjectvector.h"

using namespace std;
using namespace luxrays;
using namespace slg;

//------------------------------------------------------------------------------
// NamedObjectVector
//------------------------------------------------------------------------------

NamedObjectVector::NamedObjectVector() {
}

NamedObjectVector::~NamedObjectVector() {
	BOOST_FOREACH(NamedObject *o, objs)
		delete o;
}

NamedObject *NamedObjectVector::DefineObj(NamedObject *newObj) {
	const string &name = newObj->GetName();

	if (IsObjDefined(name)) {
		NamedObject *oldObj = objs[GetIndex(name)];

		// Update name/object definition
		const u_int index = GetIndex(name);
		objs[index] = newObj;

		name2index.left.erase(name);
		name2index.insert(Name2IndexType::value_type(name, index));

		index2obj.left.erase(index);
		index2obj.insert(Index2ObjType::value_type(index, newObj));

		return oldObj;
	} else {
		// Add the new name/object definition
		const u_int index = objs.size();
		objs.push_back(newObj);
		name2index.insert(Name2IndexType::value_type(name, index));
		index2obj.insert(Index2ObjType::value_type(index, newObj));

		return NULL;
	}
}

bool NamedObjectVector::IsObjDefined(const std::string &name) const {
	return (name2index.left.count(name) > 0);
}

const NamedObject *NamedObjectVector::GetObj(const std::string &name) const {
	return objs[GetIndex(name)];
}

const NamedObject *NamedObjectVector::GetObj(const u_int index) const {
	return objs[index];
}

u_int NamedObjectVector::GetIndex(const std::string &name) const {
	Name2IndexType::left_const_iterator it = name2index.left.find(name);

	if (it == name2index.left.end())
		throw runtime_error("Reference to an undefined NamedObject: " + name);
	else
		return it->second;
}

u_int NamedObjectVector::GetIndex(const NamedObject *o) const {
	Index2ObjType::right_const_iterator it = index2obj.right.find(o);

	if (it == index2obj.right.end())
		throw runtime_error("Reference to an undefined NamedObject: " + boost::lexical_cast<string>(o));
	else
		return it->second;
}

const std::string &NamedObjectVector::GetName(const u_int index) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(index);

	if (it == name2index.right.end())
		throw runtime_error("Reference to an undefined NamedObject: " + index);
	else
		return it->second;
	
}

const std::string &NamedObjectVector::GetName(const NamedObject *o) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(GetIndex(o));

	if (it == name2index.right.end())
		throw runtime_error("Reference to an undefined NamedObject: " + boost::lexical_cast<string>(o));
	else
		return it->second;
}

u_int NamedObjectVector::GetSize()const {
	return static_cast<u_int>(objs.size());
}

void NamedObjectVector::GetNames(std::vector<std::string> &names) const {
	const u_int size = GetSize();
	names.resize(size);

	for (u_int i = 0; i < size; ++i)
		names[i] = GetName(i);
}

std::vector<NamedObject *> &NamedObjectVector::GetObjs() {
	return objs;
}

void NamedObjectVector::DeleteObj(const std::string &name) {
	const u_int index = GetIndex(name);
	objs.erase(objs.begin() + index);
	name2index.left.erase(name);
	index2obj.left.erase(index);
}
