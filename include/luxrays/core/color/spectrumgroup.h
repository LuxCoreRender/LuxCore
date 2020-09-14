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
	SpectrumGroup(const u_int groupsCount = 0) : group(groupsCount) {
	}
	virtual ~SpectrumGroup() { }

	u_int Size() const { return group.size(); }
	void Resize(const u_int s) { group.resize(s); }
	void Shrink(const u_int s) { group.shrink_to_fit(); }

	void Clear() {
		for (auto &s : group)
			s = Spectrum();
	}

	Spectrum Sum() const {
		Spectrum sum;

		for (auto const &s : group)
			sum += s;
		
		return sum;
	}
	
	SpectrumGroup &Add(const u_int i, const Spectrum &s) {
		// Auto expand the group if required
		if (i >= group.size())
			group.resize(i + 1);

		group[i] += s;

		return *this;
	}

	SpectrumGroup &AddWeighted(const float a, const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());
		
		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] += a * s2.group[i];

		return *this;
	}

	SpectrumGroup &AddWeighted(const Spectrum &a, const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());
		
		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] += a * s2.group[i];

		return *this;
	}

	bool Black() const {
		for (auto const &s : group)
			if (!s.Black())
				return false;
		
		return true;
	}

	bool IsNaN() const {
		for (auto const &s : group)
			if (s.IsNaN())
				return true;
		
		return false;
	}
	bool IsInf() const {
		for (auto const &s : group)
			if (s.IsInf())
				return true;
		
		return false;
	}
	bool IsNeg() const {
		for (auto const &s : group)
			if (s.IsNeg())
				return true;
		
		return false;
	}
	bool IsValid() const {
		return !IsNaN() && !IsInf() && !IsNeg();
	}

	const Spectrum &operator[](int i) const { return group[i]; }
	Spectrum &operator[](int i) { return group[i]; }

	//--------------------------------------------------------------------------
	// With SpectrumGroup operators
	//--------------------------------------------------------------------------

	SpectrumGroup &operator+=(const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());

		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] += s2.group[i];

		return *this;
	}
	
	SpectrumGroup &operator-=(const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());

		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] -= s2.group[i];

		return *this;
	}

	SpectrumGroup &operator*=(const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());

		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] *= s2.group[i];

		return *this;
	}
	
	SpectrumGroup &operator/=(const SpectrumGroup &s2) {
		// Auto expand the group if required
		if (s2.group.size() > group.size())
			group.resize(s2.group.size());

		for (u_int i = 0; i < s2.group.size(); ++i)
			group[i] /= s2.group[i];

		return *this;
	}

	//--------------------------------------------------------------------------
	// With Spectrum operators
	//--------------------------------------------------------------------------

	SpectrumGroup &operator+=(const Spectrum &s) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] += s;

		return *this;
	}

	SpectrumGroup &operator-=(const Spectrum &s) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] -= s;

		return *this;
	}

	SpectrumGroup &operator*=(const Spectrum &s) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] *= s;

		return *this;
	}

	SpectrumGroup &operator/=(const Spectrum &s) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] /= s;

		return *this;
	}

	//--------------------------------------------------------------------------
	// With float operators
	//--------------------------------------------------------------------------

	SpectrumGroup &operator+=(const float a) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] += a;

		return *this;
	}

	SpectrumGroup &operator-=(const float a) {
		const float factor = 1.f/ a;
		for (u_int i = 0; i < group.size(); ++i)
			group[i] -= factor;

		return *this;
	}

	SpectrumGroup &operator*=(const float a) {
		for (u_int i = 0; i < group.size(); ++i)
			group[i] *= a;

		return *this;
	}

	SpectrumGroup &operator/=(const float a) {
		const float factor = 1.f/ a;
		for (u_int i = 0; i < group.size(); ++i)
			group[i] *= factor;

		return *this;
	}
	
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
