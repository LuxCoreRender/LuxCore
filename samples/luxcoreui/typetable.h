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

#ifndef _LUXCOREAPP_TYPETABLE_H
#define	_LUXCOREAPP_TYPETABLE_H

#include <string>
#include <boost/unordered_map.hpp>

class TypeTable {
public:
	TypeTable() { }
	~TypeTable() { }

	TypeTable &Add(const std::string &tag, const int val) {
		toValTable[tag] = val;
		toStringTable[val] = tag;

		// Remove the last \0
		if (tagListString.length() > 0)
			tagListString = tagListString.substr(0, tagListString.length() - 1);

		// Update tagListString
		tagListString.append(tag);
		tagListString.push_back(0);
		
		tagListString.push_back(0);

		return *this;
	}

	void SetDefault(const std::string &tag) {
		defaultTag = tag;
	}

	std::string GetDefaultTag() const { return defaultTag; }

	int GetVal(const std::string &tag) {
		if (toValTable.count(tag) > 0)
			return toValTable[tag];
		else
			return toValTable[defaultTag];
	}

	std::string GetTag(const int val) {
		if (toStringTable.count(val) > 0)
			return toStringTable[val];
		else
			return defaultTag;
	}

	const char *GetTagList() const {
		return tagListString.c_str();
	}

private:
	boost::unordered_map<std::string, int> toValTable;
	boost::unordered_map<int, std::string> toStringTable;

	std::string defaultTag;
	
	std::string tagListString;
};

#endif	/* _LUXCOREAPP_TYPETABLE_H */
