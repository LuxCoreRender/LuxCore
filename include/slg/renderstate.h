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

#ifndef _SLG_RENDERSTATE_H
#define	_SLG_RENDERSTATE_H

#include <boost/serialization/version.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/export.hpp>

#include "eos/portable_oarchive.hpp"
#include "eos/portable_iarchive.hpp"

#include "slg/slg.h"

namespace slg {

class RenderState {
public:
	RenderState(const std::string &engineTag);
	virtual ~RenderState();

	void CheckEngineTag(const std::string &engineTag);
	void SaveSerialized(const std::string &fileName);

	static RenderState *LoadSerialized(const std::string &fileName);

	friend class boost::serialization::access;

protected:
	// Used by serialization
	RenderState() { }

	std::string engineTag;

private:
	template<class Archive> void serialize(Archive &ar, const u_int version) {
		ar & engineTag;
	}
};

}

BOOST_SERIALIZATION_ASSUME_ABSTRACT(slg::RenderState)

#endif	/* _SLG_RENDERSTATE_H */
