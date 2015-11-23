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

#ifndef _SLG_STATICTABLE_H
#define	_SLG_STATICTABLE_H

#include <string>
#include <sstream>
#include <boost/unordered_map.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include "luxrays/luxrays.h"
#include "luxrays/core/utils.h"

namespace slg {

// The REGISTRY template parameter is there to be able to separate different
// static tables (aka registries, name spaces)
template <class REGISTRY, class K, class T> class StaticTable {
public:
	StaticTable() { }
	~StaticTable() { }

	bool Get(const K &key, T &val) const {
		const boost::unordered_map<K, T> &table = GetTable();

		typename boost::unordered_map<K, T>::const_iterator it = table.find(key);
		if (it == table.end())
			return false;
		else {
			val = it->second;
			return true;
		}
	}

	size_t GetSize() const { return GetTable().size(); }

	std::string ToString() const {
		std::stringstream ss;

		ss << "StaticTable[";
		boost::unordered_map<K, T> &table = GetTable();
		bool first = true;
		for (typename boost::unordered_map<K, T>::const_iterator it = table.begin(); it != table.end(); ++it) {
			if (first)
				first = false;
			else
				ss << ",";

			ss << "(" << boost::lexical_cast<std::string>(it->first) << "," << boost::lexical_cast<std::string>(it->second) << ")";
		}
		ss << "]";

		return ss.str();
	}
	
	class RegisterTableValue {
	public:
		RegisterTableValue(const K &key, const T &val) {
			boost::unordered_map<K, T> &table = GetTable();

			typename boost::unordered_map<K, T>::const_iterator it = table.find(key);
			if (it == table.end())
				table[key] = val;
			else
				throw std::runtime_error("Already registered key in StaticTable::RegisterTableValue::RegisterTableValue(): " +
						boost::lexical_cast<std::string>(key));
		}
		virtual ~RegisterTableValue() { }
	};

private:
	static boost::unordered_map<K, T> &GetTable() {
		static boost::unordered_map<K, T> table;
		
		return table;
	}
};

#define STATICTABLE_NAME(F) F ## _StaticTable

// Use STATICTABLE_DECLARE_DECLARATION inside the base class holding the table
#define STATICTABLE_DECLARE_DECLARATION(R, K, F) static StaticTable<R, K, F> STATICTABLE_NAME(F)
// Use STATICTABLE_DECLARATION inside the base class .cpp
#define STATICTABLE_DECLARATION(R, K, F) StaticTable<R, K, R::F> R::STATICTABLE_NAME(F)

// Use STATICTABLE_DECLARE_REGISTRATION() inside the class declaration to register
#define STATICTABLE_DECLARE_REGISTRATION(R, C, K, F) static StaticTable<R, K, F>::RegisterTableValue C ## F ## _StaticTableRegister
// Use STATICTABLE_REGISTER() to register a class
// NOTE: you have to place all STATICTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the StaticTable) because order of static
// field initialization is otherwise undefined.
#define STATICTABLE_REGISTER(R, C, N, K, F) StaticTable<R, K, R::F>::RegisterTableValue R::C ## F ## _StaticTableRegister(N, C::F)

}

#endif	/* _SLG_STATICTABLE_H */
