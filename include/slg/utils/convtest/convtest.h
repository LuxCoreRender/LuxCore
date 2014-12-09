/***************************************************************************
 * Copyright 1998-2013 by authors (see AUTHORS.txt)                        *
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

#ifndef _SLG_CONVTEST_H
#define _SLG_CONVTEST_H

#include <vector>
#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>

#include "slg/utils/convtest/pdiff/metric.h"

namespace slg {

class ConvergenceTest {
public:
	ConvergenceTest(const u_int w, const u_int h);
	virtual ~ConvergenceTest();

	void NeedTVI();
	const float *GetTVI() const { return &tvi[0]; }
	
	void Reset();
	void Reset(const u_int w, const u_int h);
	u_int Test(const float *image);

	friend class boost::serialization::access;

private:
	// Used by serialization
	ConvergenceTest() { }

	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & width;
		ar & height;
		ar & reference;
		ar & tvi;
	}

	u_int width, height;
	
	std::vector<float> reference;
	std::vector<float> tvi;
};

}

BOOST_CLASS_VERSION(slg::ConvergenceTest, 1)

#endif

