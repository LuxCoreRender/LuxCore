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
	~FuncTable() { }

	T GetFunc(const std::string &name) const {
		const boost::unordered_map<std::string, T> &funcTable = GetFuncTable();

		typename boost::unordered_map<std::string, T>::const_iterator it = funcTable.find(name);
		if (it == funcTable.end())
			return NULL;
		else
			return it->second;
	}

	class RegisterFunc {
	public:
		RegisterFunc(const std::string &name, T func) {
			boost::unordered_map<std::string, T> &funcTable = GetFuncTable();

			typename boost::unordered_map<std::string, T>::const_iterator it = funcTable.find(name);
			if (it == funcTable.end())
				funcTable[name] = func;
			else
				throw std::runtime_error("Already registered name in FuncTable::Register(): " + name);
		}
		virtual ~RegisterFunc() { }
	};

private:
	static boost::unordered_map<std::string, T> &GetFuncTable() {
		static boost::unordered_map<std::string, T> funcTable;
		return funcTable;
	}
};

#define FUNCTABLE_NAME(F) F ## _FuncTable

// Use FUNCTABLE_DECLARE_DECLARATION inside the base class holding the table
#define FUNCTABLE_DECLARE_DECLARATION(F) static FuncTable<F ## FuncPtr> FUNCTABLE_NAME(F)
// Use FUNCTABLE_DECLARATION inside the base class .cpp
#define FUNCTABLE_DECLARATION(C, F) FuncTable<C::F ## FuncPtr> C::FUNCTABLE_NAME(F)

// Use FUNCTABLE_DECLARE_REGISTRATION() inside the class declaration to register
#define FUNCTABLE_DECLARE_REGISTRATION(F) static FuncTable<F ## FuncPtr>::RegisterFunc F ## _FuncPtrDeclaration
// Use FUNCTABLE_REGISTER() to register a class
// NOTE: you have to place all FUNCTABLE_REGISTER() in the same .cpp file of the
// main base class (i.e. the one holding the FuncTable) or the compiler optimizer
// will remove the code.
#define FUNCTABLE_REGISTER(C, N, F) FuncTable<C::F ## FuncPtr>::RegisterFunc C::F ## _FuncPtrDeclaration(N, C::F)

}

#endif	/* _SLG_FUNCTABLE_H */
