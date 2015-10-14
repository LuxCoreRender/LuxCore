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

#ifndef _SLG_FUNCTABLE_H
#define	_SLG_FUNCTABLE_H

#include <string>
#include <boost/unordered_map.hpp>

#include "luxrays/luxrays.h"

namespace slg {

template <class T> class FuncTable {
public:
	FuncTable() { }
	virtual ~FuncTable() { }

	void Register(const std::string &name, T func) {
		funcTable[name] = func;
	}

	T Get(const std::string &name) const {
		typename boost::unordered_map<std::string, T>::const_iterator it = funcTable.find(name);
		if (it == funcTable.end())
			return NULL;
		else
			return it->second;
	}

private:
	boost::unordered_map<std::string, T> funcTable;
};

template <class T> class FuncTableRegister {
public:
	FuncTableRegister(FuncTable<T> &funcTable, const std::string &name, T func) {
		funcTable.Register(name, func);
	}
	virtual ~FuncTableRegister() { }
};

}

#endif	/* _SLG_FUNCTABLE_H */
