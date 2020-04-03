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

#ifndef _LUXRAYS_SPECTRUMGROUP_H
#define _LUXRAYS_SPECTRUMGROUP_H

#include <vector>

#include "luxrays/core/color/color.h"

namespace luxrays {

// Mostly used for Spectrum(s) related to light groups

class SpectrumGroup {
public:
	SpectrumGroup(const size_t groupsCount = 0) : group(groupsCount) {
	}
	virtual ~SpectrumGroup() { }

	size_t Size() const { return group.size(); }
	void Resize(const size_t s) { group.resize(s); }
	void Shrink(const size_t s) { group.shrink_to_fit(); }

	const Spectrum &operator[](int i) const { return group[i]; }
	Spectrum &operator[](int i) { return group[i]; }

	friend class boost::serialization::access;

	
private:
	std::vector<Spectrum> group;

	template<class Archive> void serialize(Archive & ar, const unsigned int version) {
		ar & group;
	}
};

}

// Eliminate serialization overhead at the cost of
// never being able to increase the version.
BOOST_CLASS_IMPLEMENTATION(luxrays::SpectrumGroup, boost::serialization::object_serializable)
BOOST_CLASS_EXPORT_KEY(luxrays::SpectrumGroup)

#endif // _LUXRAYS_SPECTRUMGROUP_H
