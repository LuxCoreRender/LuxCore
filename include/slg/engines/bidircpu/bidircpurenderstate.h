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

#ifndef _SLG_BIDIRCPURENDERSTATE_H
#define	_SLG_BIDIRCPURENDERSTATE_H

#include "luxrays/utils/serializationutils.h"
#include "slg/slg.h"
#include "slg/renderstate.h"

namespace slg {

class BiDirCPURenderState : public RenderState {
public:
	BiDirCPURenderState(const u_int seed);
	virtual ~BiDirCPURenderState();

	u_int bootStrapSeed;

	friend class boost::serialization::access;

private:
	// Used by serialization
	BiDirCPURenderState() { }

	template<class Archive> void serialize(Archive &ar, const u_int version);
};

}

BOOST_CLASS_VERSION(slg::BiDirCPURenderState, 1)

BOOST_CLASS_EXPORT_KEY(slg::BiDirCPURenderState)

#endif	/* _SLG_BIDIRCPURENDERSTATE_H */
