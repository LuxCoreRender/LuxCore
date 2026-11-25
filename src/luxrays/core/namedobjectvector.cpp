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

#include <memory>
#include <sstream>
#include <algorithm>
#include <tuple>

#include "luxrays/core/namedobject.h"
#include "luxrays/usings.h"
#include "luxrays/utils/strutils.h"
#include "luxrays/core/namedobjectvector.h"

using namespace std;
using namespace luxrays;

//------------------------------------------------------------------------------
// NamedObjectVector
//------------------------------------------------------------------------------


NamedObjectVector::NamedObjectVector() {
}

// Specialization (definition) of DefineObj for NamedObject
// Derived classes will rely on it
template<>
std::tuple< NamedObject&, std::unique_ptr<NamedObject> >
NamedObjectVector::DefineObj(std::unique_ptr<NamedObject>&& obj) {

	const std::string name = obj->GetName();

	if (IsObjDefined(name)) {
		// The object already exists in the container

		// Update name/object definition
		const u_int index = GetIndex(name);
		std::swap(objs[index], obj);  // warning: obj now contains the old object

		// Get reference on new object in container
		auto& newObjRef = *objs[index];

		// Update index structures
		name2index.left.erase(name);
		name2index.insert(Name2IndexType::value_type(name, index));

		index2obj.left.erase(index);
		index2obj.insert(Index2ObjType::value_type(index, std::ref(*objs[index])));

		// Return
		return std::make_tuple(std::ref(newObjRef), std::move(obj));

	} else {
		// The object does not already exist in the container

		// Add the new name/object definition
		const u_int index = objs.size();
		objs.push_back(std::move(obj));  // warning: obj is no more valid (moved...)

		auto& newObjRef = *objs.back();

		// Update index structures
		name2index.insert(Name2IndexType::value_type(name, index));
		index2obj.insert(Index2ObjType::value_type(index, std::ref(*objs.back())));

		return std::make_tuple(std::ref(newObjRef), std::move(std::unique_ptr<NamedObject>()));
	}
}


bool NamedObjectVector::IsObjDefined(const string &name) const {
	return (name2index.left.count(name) > 0);
}

NamedObjectConstRef NamedObjectVector::GetObj(const string &name) const {
	return *objs[GetIndex(name)];
}

NamedObjectRef NamedObjectVector::GetObj(const string &name) {
	return *objs[GetIndex(name)];
}

NamedObjectConstRef NamedObjectVector::GetObj(const u_int index) const {
	return *objs[index];
}

NamedObjectRef NamedObjectVector::GetObj(const u_int index) {
	return *objs[index];
}

u_int NamedObjectVector::GetIndex(const string &name) const {
	Name2IndexType::left_const_iterator it = name2index.left.find(name);

	if (it == name2index.left.end())
		throw UndefinedNamedObjectError(
			"Reference to an undefined NamedObject name: " + name
		);
	else
		return it->second;
}

u_int NamedObjectVector::GetIndex(NamedObjectConstRef o) const {
	Index2ObjType::right_const_iterator it = index2obj.right.find(o);

	if (it == index2obj.right.end())
		throw UndefinedNamedObjectError(
			"Reference to an undefined NamedObject pointer: " + luxrays::ToString(&o)
		);
	else
		return it->second;
}

const string &NamedObjectVector::GetName(const u_int index) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(index);

	if (it == name2index.right.end())
		throw UndefinedNamedObjectError(
			"Reference to an undefined NamedObject index: " + luxrays::ToString(index)
		);
	else
		return it->second;
}

const string &NamedObjectVector::GetName(NamedObjectConstRef o) const {
	Name2IndexType::right_const_iterator it = name2index.right.find(GetIndex(o));

	if (it == name2index.right.end())
		throw UndefinedNamedObjectError(
			"Reference to an undefined NamedObject: " + luxrays::ToString(&o)
		);
	else
		return it->second;
}

u_int NamedObjectVector::GetSize()const {
	return static_cast<u_int>(objs.size());
}

//void NamedObjectVector::GetNames(std::vector<string> &names) const {
	//const u_int size = GetSize();
	//names.resize(size);

	//for (u_int i = 0; i < size; ++i)
		//names[i] = GetName(i);
//}

template<>
NamedObjectUPtr NamedObjectVector::DeleteObj<NamedObject>(const string &name) {
	// We swap remove target and last object, and pop back
	if (objs.empty()) throw("Trying to delete an object in an empty container");

	// Record index
	auto removeObjIndex = GetIndex(name);
	auto lastObjIndex = objs.size() - 1;

	// Record names
	const std::string removeObjName{objs[removeObjIndex]->GetName()};
	const std::string lastObjName{objs[lastObjIndex]->GetName()};

	if (lastObjIndex != removeObjIndex)
	{

		std::swap(objs[removeObjIndex], objs[lastObjIndex]);

		// redo links
		name2index.left.erase(removeObjName);
		name2index.left.erase(lastObjName);
		name2index.insert(Name2IndexType::value_type(lastObjName, removeObjIndex));

		index2obj.left.erase(lastObjIndex);
		index2obj.left.erase(removeObjIndex);
		index2obj.insert(
			Index2ObjType::value_type(
				removeObjIndex, std::ref(*objs[removeObjIndex])
			)
		);

	}
	else
	{
		// last
		name2index.left.erase(removeObjName);
		index2obj.left.erase(removeObjIndex);
	}

	// remove last
	NamedObjectUPtr ret = std::move(objs.back());
	objs.pop_back();
	return std::move(ret);
}


void NamedObjectVector::DeleteObjs(const std::vector<string> &names) {

	for (const string& name : names) {
		DeleteObj<NamedObject>(name);
	}
}


std::ostream& operator<<(std::ostream& os, const luxrays::NamedObject& obj) {
    os << obj.GetName(); // Print the NamedObject
    return os;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const std::reference_wrapper<T>& ref) {
    os << ref.get(); // Print the value the wrapper refers to
    return os;
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
		ss << "(" << i << ", " << &objs[i] << ")";
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
// vim: autoindent noexpandtab tabstop=4 shiftwidth=4
