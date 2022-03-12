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

#include <sstream>
#include <algorithm>
#include <boost/foreach.hpp>

#include "luxrays/utils/strutils.h"
#include "luxrays/core/namedobjectvector.h"

using namespace std;
using namespace luxrays;

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

bool NamedObjectVector::IsObjDefined(const string &name) const {
	return (name2index.left.count(name) > 0);
}

const NamedObject *NamedObjectVector::GetObj(const string &name) const {
	return objs[GetIndex(name)];
}

NamedObject *NamedObjectVector::GetObj(const string &name) {
	return objs[GetIndex(name)];
}

const NamedObject *NamedObjectVector::GetObj(const u_int index) const {
	return objs[index];
}

NamedObject *NamedObjectVector::GetObj(const u_int index) {
	return objs[index];
}

u_int NamedObjectVector::GetIndex(const string &name) const {
	Name2IndexType::left_const_iterator it = name2index.left.find(name);

	if (it == name2index.left.end())
		throw runtime_error("Reference to an undefined NamedObject name: " + name);
	else
		return it->second;
}

u_int NamedObjectVector::GetIndex(const NamedObject *o) const {
	Index2ObjType::right_const_iterator it = index2obj.right.find(o);

	if (it == index2obj.right.end())
		throw runtime_error("Reference to an undefined NamedObject pointer: " + luxrays::ToString(o));
	else
		return it->second;
}

const string &NamedObjectVector::GetName(const u_int index) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(index);

	if (it == name2index.right.end())
		throw runtime_error("Reference to an undefined NamedObject index: " + luxrays::ToString(index));
	else
		return it->second;
	
}

const string &NamedObjectVector::GetName(const NamedObject *o) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(GetIndex(o));

	if (it == name2index.right.end())
		throw runtime_error("Reference to an undefined NamedObject: " + luxrays::ToString(o));
	else
		return it->second;
}

u_int NamedObjectVector::GetSize()const {
	return static_cast<u_int>(objs.size());
}

void NamedObjectVector::GetNames(vector<string> &names) const {
	const u_int size = GetSize();
	names.resize(size);

	for (u_int i = 0; i < size; ++i)
		names[i] = GetName(i);
}

vector<NamedObject *> &NamedObjectVector::GetObjs() {
	return objs;
}

void NamedObjectVector::DeleteObj(const string &name) {
	const u_int index = GetIndex(name);
	objs.erase(objs.begin() + index);

	// I have to rebuild the indices from scratch
	name2index.clear();
	index2obj.clear();

	for (u_int i = 0; i < objs.size(); ++i) {
		NamedObject *obj = objs[i];

		name2index.insert(Name2IndexType::value_type(obj->GetName(), i));
		index2obj.insert(Index2ObjType::value_type(i, obj));	
	}
}

void NamedObjectVector::DeleteObjs(const vector<string> &names) {
	vector<u_int> removeList;
	removeList.reserve(names.size());
	for (const string &name : names) {
		if (IsObjDefined(name)) {
			const u_int index = GetIndex(name);
			removeList.push_back(index);
		}
	}
	sort(removeList.begin(), removeList.end(), greater<u_int>());

	for (u_int i : removeList)
		objs.erase(objs.begin() + i);

	// I have to rebuild the indices from scratch
	name2index.clear();
	index2obj.clear();

	for (u_int i = 0; i < objs.size(); ++i) {
		NamedObject *obj = objs[i];

		name2index.insert(Name2IndexType::value_type(obj->GetName(), i));
		index2obj.insert(Index2ObjType::value_type(i, obj));	
	}
}

template<class MapType> void PrintMap(const MapType &map, ostream &os) {
	typedef typename MapType::const_iterator const_iterator;

	os << "Map[";

	os << "(";
	for (const_iterator i = map.begin(), iend = map.end(); i != iend; ++i) {
		if (i != map.begin())
			os << ", ";

		os << "(" << i->first << ", " << i->second << ")";
	}
	os << ")";

	os << "]";
}

string NamedObjectVector::ToString() const {
	stringstream ss;

	ss << "NamedObjectVector[\n";
	
	for (u_int i = 0; i < objs.size(); ++i) {
		if (i > 0)
			ss << ", ";
		ss << "(" << i << ", " << objs[i] << ")";
	}
	ss << ",\n";

	ss << "name2index[";
	PrintMap(name2index.left, ss);
	ss << ", ";
	PrintMap(name2index.right, ss);
	ss << "],\n";

	ss << "index2obj[";
	PrintMap(index2obj.left, ss);
	ss << ", ";
	PrintMap(index2obj.right, ss);
	ss << "]\n";

	ss << "]";

	return ss.str();
}
